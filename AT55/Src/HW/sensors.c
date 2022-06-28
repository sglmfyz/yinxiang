#include "main.h"
#include "sht3x.h"
#include "heap.h"
#include "log.h"
#include "util.h"
#include "etimer.h"
#include "hw.h"
#include "adc.h"
#include "sensors.h"
#include "modbus_regs.h"

#define I2C_NUM 2
#define MAX_ERROR_NUM 10
#define SENSOR_UPDATE_TIMEOUT 300 //ms

#define SHT3X_ADDR (0x44 << 1)
#define SHT3X_CONFIG1 REPEATAB_HIGH
#define SHT3X_CONFIG2 FREQUENCY_4HZ

#define  AD_VAL_MUL      10        // 比值结果放大10倍以避免小数运算
#define  AD_MAX_VAL      (3102u)
#define  TGS_R0_VAL      (29198u)  // R0的缺省值
#define  TGS_RL_VAL      (5600u)   // RL值
#define  TGS_CON_VAL     (6204u)   // 化简得到的一个计算常数
#define  TGS_R0_MUL      (TGS_R0_VAL*AD_VAL_MUL)  // R0的缺省值
#define  UART485_BUS_NUM 2

typedef int (*sensor_get)(void *this, I2C_T *i2c, int probe, float *temper, float *humidity);

enum {
    TYPE_UNKNOW,
    TYPE_SHT3X,
    TYPE_SHT2X,
    TYPE_NUM,
};

enum {
    UART485_FIN,
    UART485_INIT,
    UART485_PROBED,
};

enum {
    ENUM485_GAS2,
    ENUM485_TEMP,
    ENUM485_HD,
    ENUM485_GAS1,
    ENUM485_NUM,
};

static struct {
    I2C_T *i2c[I2C_NUM];
    uint8_t type[I2C_NUM];
    uint8_t err[I2C_NUM];
    void *sensor[TYPE_NUM];
    sensor_get get[TYPE_NUM];
    int32_t temp_sum;
    int16_t temp_num;
    int16_t temp_last;
    int8_t temp_valid;
    int8_t voc_alg;

    uint8_t probed485_mask;  // 已经加载成功的485设备掩码
    struct {
        uint8_t stage[ENUM485_NUM];
        uint8_t station;    //modbus station - 1
        uint32_t timeout[ENUM485_NUM]; // probe超时时间
        Uart_T *port;
    } uart485[UART485_BUS_NUM];
} *sensor_info;

/*
传感器AD采样示意图：

                    -----            -->   5V
                     _|_
                    |___|            -->  VOC传感器阻值R0或Rs[R0指代洁净空气状态下的传感器阻值，Rs指代检测到其它气体时候的阻值]
                     _|_
                    |___|            -->   R1=5.6k
                     _|_------ AD    
                    |___|            -->   R2=5.6k
                     _|_
                      -              -->   GND

Rs+R1 = (Vcc-Vad)*R2/Vad     [Vad=AD点电压]
Rs = (Vcc-Vad)*R2/Vad - R1   
因为R1=R2=RL=5600欧，Rs=((Vcc-Vad)/Vad - 1)*R
Vcc=5000mv,Vad=3300mv*AD/4095  [AD=AD点的采样值]
代入化简得：
Rs=((6204-AD)/AD - 1)*RL

设定Rs/R0为x, ppm为y，按照乙醇和甲苯综合考虑，定了一个曲线
x      y
0.5    1
0.2   10
0.09  30
0.03   100

AD0采用380，一般刚开的时候，值在100左右，但随着时间增加，voc变热，这个值会上升到326左右，
对应的VOC阻值约为80K

x = Rs/R0 = ((6204/ADs) - 2)/((6204/AD0) - 2)
x1e4 = (4332400/ADs - 1397)

气体浓度越高，探头电阻越小，AD采的电压值越大
因此，浓度越高，AD越大，x1e4越小

另外，随着温湿度上升，rs/r0也会变小，取图上的点
温度 rs/r0           temp10 ratio * 1000
50   0.35          500      2857.143
40   0.54          400      1851.85
30   0.75          300      1333.33
20   1.0           200      1000
10   1.35          100      740.7

*/
static uint16_t _ad0(void)
{
    int16_t temp10 = sensor_info->temp_last;
    uint32_t rs1000, rbase;
    uint16_t ad0;

    rbase = 80;
    if (temp10 == 0 && get_input_reg(LCD_3X_TEMPER_GET) == 0 && get_input_reg(LCD_3X_HD_GET) == 0) {
        temp10 = 200;
    }
    
    if (temp10 <= 200) {
        rs1000 = (uint32_t)((-3.5f * (float)temp10 + 1700) * rbase);
    } else if (temp10 <= 300) {
        rs1000 = (uint32_t)((-2.5f * (float)temp10 + 1500) * rbase);
    } else if (temp10 <= 400) {
        rs1000 = (uint32_t)((-2.1f* (float)temp10 + 1380) * rbase);
    } else if (temp10 <= 600) {
        rs1000 = (uint32_t)((-1.9f * (float)temp10 + 1300) * rbase);
    } else {
        rs1000 = (uint32_t)((-1.9f * 600 + 1300) * rbase);
    }
    // 5.0 * 5.6 / (11.2 + rs1000/1000) = 3.3 * Ad / 4095
    ad0 = (uint16_t)((5.0f *  5.6f * 4095.0f) / (3.3f * (11.2f + (float)rs1000 /1000.0f)));
    
    return ad0;
}

static uint16_t _reg2VOC_max30(uint16_t reg, uint16_t ad0)
{
    uint32_t  rs, x, y;
    
    rs = TGS_CON_VAL - reg;
    rs *= TGS_RL_VAL;
    rs /= reg;
    rs -= TGS_RL_VAL;
    
    x = TGS_R0_MUL / rs;
    
    if(x < 13) {
        y = 0;
    } else if(x < 20) {
        y = 286*x-2710;
        y = y / 1000;
    } else if(x < 33) {
        y = 538*x-7760;
        y = y / 1000;
    } else if(x < 83)  {
        y = x / 5 + 4;
    } else if(x < 233) {
        y = x / 15 + 14;
    } else if (x < 350) {
        y = 29;
    } else {
        y = 30;
    }
    
    if (y > 30) {
        y = 30;
    }
    
    return (uint16_t)y;
}




