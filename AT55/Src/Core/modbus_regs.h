#ifndef __MODBUS_REGS_H__
#define __MODBUS_REGS_H__

#include "uart.h"
#include "net.h"

enum {
    UREG3X_HW_CONFIG = 1,
    UREG3X_FW_VERSION, //固件版本
    UREG3X_GPRS_STATUS,
    UREG3X_GPRS_SIGNAL,
    UREG3X_GPRS_DATA,
    UREG3X_HW_FEATURE,

    UREG3X_SENSOR_ALARM=9,
    UREG3X_VOC1_VALUE = 10,
    UREG3X_VOC2_VALUE,
    UREG3X_VOC3_VALUE,
    UREG3X_TEMPER1_VALUE,   //温度1
    UREG3X_HD1_VALUE,       //湿度1
    UREG3X_TEMPER2_VALUE,   //温度2
    UREG3X_HD2_VALUE,       //湿度2
    UREG3X_SENSOR8_VALUE,   //共预留8个传感器
    
    UREG3X_FAN_VALUE,
    
    UREG3X_DOOR1_STATUS = 20, //门1状态
    UREG3X_DOOR2_STATUS, //门2状态
    UREG3X_DOOR3_STATUS, //预留门3状态
    UREG3X_DOOR4_STATUS, //预留门4状态

    UREG3X_LOCK1_ID_LOW , //锁1 RFID标识
    UREG3X_LOCK1_ID_HI,
    UREG3X_LOCK2_ID_LOW, //锁2 RFID标识
    UREG3X_LOCK2_ID_HI,

    UREG3X_READER_STATUS, // 来回翻转，通知上层有卡刷入
    UREG3X_READER_ID_LOW,
    UREG3X_READER_ID_HI,
    
    UREG3X_SWITCH1_STATUS,  //行程开关的状态
    UREG3X_SWITCH2_STATUS,  //行程开关的状态
    UREG3X_LOCK1_STATUS, //门锁1状态
    UREG3X_LOCK2_STATUS, //门锁2状态
    
    UREG3X_SWITCH3_STATUS,  //行程开关的状态
    UREG3X_SWITCH4_STATUS,  //行程开关的状态
    UREG3X_LOCK3_STATUS, //门锁1状态
    UREG3X_LOCK4_STATUS, //门锁2状态

    UREG3X_TIME_CUR_SECS = 40,
    UREG3X_TIME_CUR_MINS,
    UREG3X_TIME_CUR_HOURS, 
    UREG3X_TIME_CUR_DAY,   
    UREG3X_TIME_CUR_MON,   
    UREG3X_TIME_CUR_YEAR, 
    
    UREG3X_RUN_DAYS,
    UREG3X_RUN_HOURS,
    UREG3X_RUN_MINS,     
    UREG3X_TOTAL_DAYS,   
    UREG3X_TOTAL_HOURS,  
    UREG3X_TOTAL_MINS,
    
    UREG3X_MONITOR,                 //监视寄存器，当4X寄存器发生改变时，通知读4X寄存器

	UREG3X_ADC0_VALUE=55,
    UREG3X_ADC1_VALUE,
    UREG3X_ADC2_VALUE,
    UREG3X_ADC3_VALUE,
    UREG3X_ADC4_VALUE,
    UREG3X_FIRMWARE_VER = 60,    
    UREG3X_REG_END=128,
};

enum {
    //UREG4X_FAN_ON=0,
    UREG4X_UNUSED,
    UREG4X_FAN_CTRL,        
    UREG4X_FAN_SPEED,               //设置模式下 风速
    
    UREG4X_TEMP_HI=10,
    UREG4X_TEMP_LOW,
    UREG4X_HUMIDITY_HI,
    UREG4X_HUMIDITY_LOW,
    UREG4X_VOC_HI, 
    
    UREG4X_ADJ_SECS=20,
    UREG4X_ADJ_MINS,
    UREG4X_ADJ_HOURS,
    UREG4X_ADJ_DAY,
    UREG4X_ADJ_MON,
    UREG4X_ADJ_YEAR,
    
    UREG4X_LOCK1_CMD=40,
    UREG4X_LOCK2_CMD,
    UREG4X_LOCK3_CMD,
    UREG4X_LOCK4_CMD,
    
    UREG4X_EMLOCK_EN,       // 485锁不在时，使能电磁锁
    UREG4X_EMLOCK_TIMEOUT,  // 电磁锁超时ms
    UREG4X_SWITCH_EN,       // 使能开关门检测
    UREG4X_LOCK_DELAY,      // 电子锁开门时间
    
    UREG4X_ALARM_CTL=48,    //1为自动报警
    UREG4X_ALARM_FORCE,     //1为开启报警，0为关闭报警
    
    UREG4X_SYS_CMD=50,
    UREG4X_MODBUS_UID=51,   //板子modbus栈号，可以通过屏幕修改，屏幕modbus栈号一直是1
    
    UREG4X_AUTH=60,         //是否已经进行过认证
    UREG4X_BOX1_NO=62,      //柜子1标号,如果为0则说明未使用
    UREG4X_BOX2_NO,         //柜子2标号,如果为0则说明未使用
    UREG4X_BOX3_NO,         //柜子3标号,如果为0则说明未使用
    UREG4X_BOX4_NO,         //柜子4标号,如果为0则说明未使用
    UREG4X_BOX1_NAME=70,    //柜子1名称
    UREG4X_BOX2_NAME=80,    //柜子2名称

    UREG4X_LOCAL_UPDATE=  246,  // 本地升级
    UREG4X_REG_END = 256,
};

enum {
    HW_485_LOCK1,
    HW_485_LOCK2,
    HW_485_RELAY1,
    HW_485_RELAY2,    
    HW_485_MISC1=8,
    HW_485_MISC2,
    //HW_485_CARD1,
    //HW_485_CARD2,
};

enum {
    FEATURE_EMLOCK_EN,  //电磁锁使能
};

#define MAX_MODBUS_UID 64
#define MAX_FAN_SPEED 100
#define MIN_FAN_SPEED 30
#define FLASH_MODBUS_4X_BASE  (FLASH_BASE_ADDR + FLASH_SIZE - 1024 - 1024)//原来-16


void modbus_param_init(void);
uint16_t get_input_reg(uint16_t reg);
int update_input_reg(uint16_t reg, uint16_t value);
uint16_t get_hold_reg(uint16_t reg);
int update_hold_reg(uint16_t reg, uint16_t value);
void modbus_init();
uint16_t* get_hold_reg_ptr(uint16_t addr);
int update_hold_regs(uint16_t start_reg, uint16_t *value, uint16_t num);
uint16_t* get_input_reg_ptr(uint16_t addr);

#endif
