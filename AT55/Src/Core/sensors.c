#include "sht3x.h"
#include "heap.h"
#include "log.h"
#include "util.h"
#include "etimer.h"
#include "hw.h"
#include "modbus_regs.h"
#include "adc.h"
#include "sensors.h"

#define eee 1
#define I2C_NUM 5
#define MAX_ERROR_NUM 10
#define SENSOR_UPDATE_TIMEOUT 1000 //ms

#define SHT3X_ADDR (0x44 << 1)
#define SHT3X_CONFIG1 REPEATAB_HIGH
#define SHT3X_CONFIG2 FREQUENCY_4HZ

#define TGS2620_UPDATE_TIMEOUT 330

#define  AD_VAL_MUL      10        // 比值结果放大10倍以避免小数运算
#define  AD_MAX_VAL      (3102u)
#define  TGS_R0_VAL      (29198u)  // R0的缺省值
#define  TGS_RL_VAL      (5600u)   // RL值
#define  TGS_CON_VAL     (6204u)   // 化简得到的一个计算常数
#define  TGS_R0_MUL      (TGS_R0_VAL*AD_VAL_MUL)  // R0的缺省值

typedef int (*sensor_get)(void *this, I2C_T *i2c, int probe, float *temper, float *humidity);

enum {

    TYPE_UNKNOW,
    TYPE_SHT3X,
    TYPE_NUM,
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
} *sensor_info;
//extern struct modbus_info;好像也不需要加这个
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
static uint16_t _ad0()
{
    int16_t temp10 = sensor_info->temp_last;
    uint32_t rs1000, rbase;
    uint16_t ad0;

    rbase = 80;
    
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

static uint16_t _reg2VOC(uint16_t reg, uint16_t ad0)
{
    int32_t x1e4, y;
    
    if (reg <= 100) {
        y = 0;
    } else {
        x1e4 = (int32_t)(((6204*1e4/reg) - 20000)/((6204.0f/(float)ad0) - 2.0f));
        if (x1e4 > 5000) {
            y = 0;
        } else if (x1e4 >= 2000 && x1e4 <= 5000) {
            y = (int32_t)(-0.003f * x1e4 + 16.0f);
        } else if (x1e4 >= 900 && x1e4 < 2000) {
            y = (int32_t)(-0.018182f * x1e4 + 46.36364f);
        } else if (x1e4 >= 300 && x1e4 < 900) {
            y = (int32_t)(-0.1166667f * x1e4 + 135.0f);
        } else {
            y = 100;
        }
    }
    
    setv_restrict(y, 0, MAX_VOC_VALUE);
    return (uint16_t)y;
}

static int _sht3x_get(void *this, I2C_T *i2c, int probe, float *temper, float *humidity)
{
    int status;
    SHT3X_T *sht3x;

    if (this == NULL || i2c == NULL) {
        return -PARAM_ERR;
    }
    sht3x = (SHT3X_T *)this;
    sht3x->Set_I2C_Addr(sht3x, i2c, SHT3X_ADDR);

    if (probe) {
        status = sht3x->Start_Periodic_Measurment(sht3x, SHT3X_CONFIG1, SHT3X_CONFIG2);
        if (status == 0) {
            Delay_us(2000);
        }
    } else {
        status = 0;
    }
    
    if (status == 0) {
        status = sht3x->Read_Measurement_Buffer(sht3x, temper, humidity);
    }
    return status;
}

#if 1
PROCESS(_sensors, "sensor");

PROCESS_THREAD(_sensors, ev, data)
{
    int i, j;
    static int c;
    int status;
    int succeed;
    float temper, humidity;
    I2C_T *i2c;
    static struct etimer etimer;
    //uint16_t reg1, reg2, v;
    /*static uint16_t ad0;
    extern ADC_HandleTypeDef hadc;
    uint16_t pid = board_info->product_id;*/

    PROCESS_BEGIN();
    syslog_debug("In th_sensor thread\n");

    //ad0 = _ad0();
    while (1) {
        etimer_set(&etimer, SENSOR_UPDATE_TIMEOUT);
        c++;

        if (c >= 60) {
            c = 0;
            //ad0 = _ad0();
        }

        PROCESS_WAIT_UNTIL(etimer_expired(&etimer));

        /*
        reg1 = ADC_Get(&hadc, 0);
        v = _reg2VOC(reg1, ad0);
        if (board_info->dev_id >= 0x200 || pid == 0x85) {
            Sys_Reg_Update(SYS_REG_VOC1, v);
        } else {
            Sys_Reg_Update(SYS_REG_VOC2, v);
        }
        
        reg2 = ADC_Get(&hadc, 1);
        v = _reg2VOC(reg2, ad0);

        if (board_info->dev_id >= 0x200 || pid == 0x85) {
            Sys_Reg_Update(SYS_REG_VOC2, v);
        } else {
            Sys_Reg_Update(SYS_REG_VOC1, v);
        }
        */
        
        for (i=0; i<I2C_NUM; i++) {
            succeed = 0;
            i2c = sensor_info->i2c[i];
            
            if (sensor_info->type[i] == 0) {
                /*try to probe sht3x*/
                for (j=1; j<TYPE_NUM; j++) {
                    if (sensor_info->sensor[j] && sensor_info->get[j]) {
                        status = sensor_info->get[j](sensor_info->sensor[j], i2c, 1, &temper, &humidity);
                        if (status == 0) {
                            succeed = 1;
                            sensor_info->type[i] = j;
                            syslog_debug("Probe sensor on port %d, type %d\n", i+1, j);
                            break;
                        } 
                    }
                }
            } else {
                j = sensor_info->type[i];
                
                if (sensor_info->sensor[j] && sensor_info->get[j]) {
                    status = sensor_info->get[j](sensor_info->sensor[j], i2c, 0, &temper, &humidity);
                    if (status == 0) {
                        succeed = 1;
                    } else {
                        sensor_info->err[i]++;
                        if (sensor_info->err[i] > MAX_ERROR_NUM) {
                            sensor_info->err[i] = 0;
                            sensor_info->type[i] = TYPE_UNKNOW;
                            syslog_debug("Lost sensor on port %d\n", i+1);
                            temper = -15.0f;
                            
                            //Sys_Reg_Update(SYS_REG_TEMP + i, 0);
                            //Sys_Reg_Update(SYS_REG_HUMIDITY + i, 0);
                        }
                    }
                } else {
                    sensor_info->type[i] = TYPE_UNKNOW;
                }
            }
            if (succeed) {
                int16_t temp10 = (int16_t)(temper*10.0f);
                
                sensor_info->err[i] = 0;

                if (!sensor_info->temp_valid) {
                    sensor_info->temp_last = temp10;
                    sensor_info->temp_valid = 1;
                }
                sensor_info->temp_sum += temp10;
                sensor_info->temp_num++;
                if (sensor_info->temp_num >= 60) {
                    sensor_info->temp_last = sensor_info->temp_sum/sensor_info->temp_num;
                    sensor_info->temp_num = 0;
                    sensor_info->temp_sum = 0;
                } 
                /*update sensor info*/
                syslog_debug("temp=%d.%d, humidity=%d\n", temp10/10, temp10%10, (uint16_t)(humidity));
                //Sys_Reg_Update(SYS_REG_TEMP + i, (uint16_t)temp10);
                //Sys_Reg_Update(SYS_REG_HUMIDITY + i, (uint16_t)(humidity));
            } 
        }
    }
    PROCESS_END();
}

int Sensors_Init()
{

    zmalloc(sensor_info, sizeof(*sensor_info));
    
    sensor_info->sensor[TYPE_SHT3X] = Create_SHT3X(NULL);
    sensor_info->get[TYPE_SHT3X] = _sht3x_get;
    sensor_info->temp_last = 200;

    return 0;
}

int Sensors_Add_Temp(int channel, I2C_T *i2c1)
{
    i_assert(channel < I2C_NUM && sensor_info->i2c[channel] == NULL);
    
    sensor_info->i2c[channel] = i2c1;

    return 0;
}
#endif

#if 0
#error 1
PROCESS(_sensors, "sensor");

PROCESS_THREAD(_sensors, ev, data)
{
    int i, j;
    static int c;
    int status;
    int succeed;
    float temper, humidity;
    I2C_T *i2c;
    static struct etimer etimer;
    uint16_t reg1, reg2, v;
    static uint16_t ad0;

    PROCESS_BEGIN();
    syslog_debug("In th_sensor thread\n");
    
    ad0 = _ad0();
    
    while (1) {
        etimer_set(&etimer, SENSOR_UPDATE_TIMEOUT);        
 
        PROCESS_WAIT_UNTIL(etimer_expired(&etimer));
        printf("value=%d    \n");
        reg1 = ADC_Get(ADC1, 0);
        v = _reg2VOC(reg1, ad0);
        update_input_reg(UREG3X_ADC0_VALUE, v);
       
        reg1 = ADC_Get(ADC1, 1);
        v = _reg2VOC(reg1, ad0);
        update_input_reg(UREG3X_ADC1_VALUE, v);

        reg1 = ADC_Get(ADC1, 2);
        v = _reg2VOC(reg1, ad0);
        update_input_reg(UREG3X_ADC2_VALUE, v);
        reg1 = ADC_Get(ADC1, 3);
        v = _reg2VOC(reg1, ad0);
        update_input_reg(UREG3X_ADC3_VALUE, v);
        reg1 = ADC_Get(ADC1, 4);
        v = _reg2VOC(reg1, ad0);
        update_input_reg(UREG3X_ADC4_VALUE, v);   //??
        
        }
    PROCESS_END();
}
#endif


int Sensor_Start()
{
    process_start(&_sensors, NULL);
    return 0;
}