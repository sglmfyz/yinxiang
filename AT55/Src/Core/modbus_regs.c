#include "modbus_regs.h"
#include "uart.h"
#include "modbus.h"
#include "heap.h"
#include "worker.h"
#include "log.h"
#include "process.h"
#include "etimer.h"
#include "util.h"
#include "hw.h"
#include "peripheral.h"
#include "main.h"
#include "gpio.h"
#include "misc_485.h"
#include "update.h"
#include "miscdrv.h"
#include "sensors.h"
#include "eeprom.h"
#define INPUT_REG_NUM 64
#define HOLD_REG_NUM 256

#define MODBUS_STRING_FUNC 66 //自定义modbus功能码，用于传输字符串命令，地址、功能码（66）、长度（两字节）、字符串, crc（两字节）
#define MAX_PKT_LEN 1480

static struct {
    uint16_t input_reg[INPUT_REG_NUM];
    uint16_t hold_reg[HOLD_REG_NUM];
} *modbus_info;
static uint16_t *input, *hold;



extern void open_lock(uint16_t time_out);
extern void close_lock(void);

static uint16_t reg_map(uint8_t type, uint16_t addr)
{
    return addr;
}

int update_input_reg(uint16_t reg, uint16_t value)
{
    if (reg < INPUT_REG_NUM) {
        modbus_info->input_reg[reg] = value;
    } else {
        return -PARAM_ERR;
    }
    return 0;
}


uint16_t get_input_reg(uint16_t reg)
{
    if (reg < INPUT_REG_NUM) {
        return modbus_info->input_reg[reg];
    } else {
        return 0xffff;
    }
}

int update_hold_reg(uint16_t reg, uint16_t value)
{
    if (reg < HOLD_REG_NUM) {
        modbus_info->hold_reg[reg] = value;
    } else {
        return -PARAM_ERR;
    }
    return 0;
}

uint16_t get_hold_reg(uint16_t reg)
{
    if (reg < HOLD_REG_NUM) {
        return modbus_info->hold_reg[reg];
    } else {
        return 0xffff;
    }
}

uint16_t* get_hold_reg_ptr(uint16_t addr)
{
    if(addr < HOLD_REG_NUM){
        return &modbus_info->hold_reg[addr];
    }
    
    return NULL;
}

uint16_t* get_input_reg_ptr(uint16_t addr)
{   
    if(addr < INPUT_REG_NUM){
        return &modbus_info->input_reg[addr];
    }
    
    return NULL;
}

int update_hold_regs(uint16_t start_reg, uint16_t *value, uint16_t num)
{
    if(start_reg + num < HOLD_REG_NUM){
        memcpy(&modbus_info->hold_reg[start_reg], value, num * 2);
        return 0;
    }
    
    return -1;
}
//设置hold_reg字符串
void set_hold_reg_str(uint16_t hold_reg, char* str)
{
    uint16_t val;
    int8_t len = strlen(str);
    while(len > 0 && *str){
        val = *(str + 1);
        val <<= 8;
        val += *str;
        update_hold_reg(hold_reg, val);
        hold_reg++;
        str += 2;
        len -= 2;
    }
    update_hold_reg(hold_reg, 0);   
}
//设置hold_reg字符串时，将字符串反转
void set_hold_reg_restr(uint16_t hold_reg, char* str)
{
    uint16_t val;
    int8_t len = strlen(str);
    while(len > 0 && *str){
        val = *(str + 1);
        val <<= 8;
        val += *str;
        val = ntohs(val);
        update_hold_reg(hold_reg, val);
        hold_reg++;
        str += 2;
        len -= 2;
    }
    update_hold_reg(hold_reg, 0);   
}

//默认的存储函数，如果没有注册回调函数则调用此函数
void reg_default_store(uint16_t addr)
{
	EE_WriteBytes(EE_MODBUS_4X_BASE + addr * 2, (uint8_t *)&modbus_info->hold_reg[addr], 2);
}

static void change_monitor(void)
{
    uint16_t monitor = get_input_reg(UREG3X_MONITOR);
    monitor = !monitor;
    update_input_reg(UREG3X_MONITOR, monitor);
}


static void _lock_cmd(uint16_t addr)
{
    uint16_t v = modbus_info->hold_reg[addr];
    int offset;
    //uint16_t emlock_enable = modbus_info->hold_reg[UREG4X_EMLOCK_EN];
    //syslog_info("Ctrl the door");
    //开门时先打开相应的灯，用于指示需要打开哪个门，开门后即关闭相应的灯
    if (addr >= UREG4X_LOCK1_CMD && addr <= UREG4X_LOCK4_CMD) {
        offset = addr - UREG4X_LOCK1_CMD;
        if (v) {
            lamp_ctrl(1, offset);
            misc485->Open_Lock(misc485, offset);
        } else {
            lamp_ctrl(0, offset);
            misc485->Close_Lock(misc485, offset);
        }
    }
}

static void _lock_delay_set(uint16_t addr)
{
    uint16_t v = modbus_info->hold_reg[addr];

    misc485->Set_Delay(misc485, v);
}



static void _sys_cmd(uint16_t addr)
{
    uint16_t *hold_reg = modbus_info->hold_reg;
    if (hold_reg[addr] == 1) {
        EXTI_Reset(); 
        NVIC_SystemReset();
    } 
}



//如果有任何一端（屏幕和PC）改变了4X寄存器的值，那么则修改monitor寄存器的值，通知读取4X寄存器
static void change_box_info(uint16_t addr)
{
    change_monitor();
}
#define FLASH_MODBUS_4X_BASE  (FLASH_BASE_ADDR + FLASH_SIZE - 16 - 1024)

static void get_hold_reg_by_flash(uint16_t *hold_reg)
{
    uint32_t temp;
    
    for(uint16_t i = 0; i < HOLD_REG_NUM * 2; i += 4){
        temp = *(__IO uint32_t*)(FLASH_MODBUS_4X_BASE + i);
        update_hold_reg(i / 2, temp);
        update_hold_reg(i / 2 + 1, temp >> 16);
    }
}
#if 0
static void set_eeprom_init_flag(void)
{
    int status;
    uint8_t cmd = 0xfe;

    status = flash_page_modify_value(offsetof(board_info_t, cmd) + BOARD_INFO_ADDR, &cmd, 1);
    i_assert(status == 0);
    
    status = flash_page_finish();
    i_assert(status == 0);        
}
#endif
static void _reboot_local_update(uint16_t addr)
{
    syslog_debug("Reboot local update\n");
    usram_info->update = 1;
    NVIC_SystemReset();
}


void modbus_param_init(void)
{ 
    uint16_t is_ee_init;
    zmalloc(modbus_info, sizeof(*modbus_info));
    
    EE_ReadVariable(EE_MODBUS_4X_BASE, &is_ee_init);//判断是否第一次上电
    syslog_info("The board_info cmd value is 0x%x\n", board_info->cmd);
    if(bit_is_set(board_info->cmd, BOARD_CMD_EEPROM_BIT)){
        syslog_info("First start up after manufactured!\n");
        is_ee_init = 0;
        EE_WriteVariable(EE_MODBUS_4X_BASE, is_ee_init);
    }

	if(is_ee_init != EEPROM_INIT_VAR){
        wdt_counter_reload();
        get_hold_reg_by_flash(modbus_info->hold_reg);//首次上电从flash中读取hold_reg的值
        wdt_counter_reload();
        EE_WriteBytes(EE_MODBUS_4X_BASE + 2, (uint8_t *)(&modbus_info->hold_reg[1]), HOLD_REG_NUM - 2);
        wdt_counter_reload();
        EE_WriteBytes(EE_MODBUS_4X_BASE + HOLD_REG_NUM, (uint8_t *)(&modbus_info->hold_reg[HOLD_REG_NUM / 2]), HOLD_REG_NUM);
        wdt_counter_reload();
        EE_WriteVariable(EE_MODBUS_4X_BASE, EEPROM_INIT_VAR);//最后写入标志，下次上电则不会再进行初始化
        //t_eeprom_init_flag();
        syslog_info("modbus hold reg init is ok!\n");
	}else{
		EE_ReadBytes(EE_MODBUS_4X_BASE, (uint8_t *)modbus_info->hold_reg, HOLD_REG_NUM * 2);
	}
    
    syslog_info("BOX1_NO is %d, BOX2_NO is %d\n", get_hold_reg(UREG4X_BOX1_NO), get_hold_reg(UREG4X_BOX2_NO));
    
    uint16_t ver = uflash_info->major_v * 100;
    ver += uflash_info->minor_v;
    
    uint32_t board_id = (board_info->product_id << 16) + board_info->dev_id;
    
    update_input_reg(UREG3X_FW_VERSION, ver);
    
    syslog_info("Board ID is %06X, version is %d\n", board_id, ver);
    
}

void modbus_init()
{
    int status;
    MBSlave_T *modbus;
    int i;
    
    status = Sensors_Init();
    i_assert(status == 0);
    printf("did it");
    status = Sensors_Add_Temp(0, i2c1);
    status |=Sensors_Add_Temp(1, i2c2);
    status |=Sensors_Add_Temp(2, i2c3);
    status |=Sensors_Add_Temp(3, i2c4);
    status |=Sensors_Add_Temp(4, i2c5);
    status |= Sensor_Start();
    i_assert(status == 0); 

    modbus = Create_Modbus_Slave();
    //modbus->Add_Uart(modbus, uart2, MODBUS_RTU, 1);
    modbus->Add_Uart(modbus, uart3, MODBUS_RTU, 1);
    zmalloc(input, 128 * sizeof(uint16_t));
    zmalloc(hold, 128 * sizeof(uint16_t));

    for (i=0; i<128; i++) {
        input[i] = i + 10;
        hold[i] = i * 2 + 1;
    }
    
    modbus->Set_Reg(modbus, MODBUS_INPUT_REG, input, 128);
    modbus->Set_Reg(modbus, MODBUS_HOLDING_REG, hold, 128);
 
    

#ifdef sksks
#error 1
    MBSlave_T *modbus;
    uint8_t uid = low_byte(board_info->dev_id);

    modbus = Create_Modbus_Slave();

    printf("Uid:%d\n", uid);
    modbus->Add_Uart(modbus, uart1, MODBUS_RTU, uid); //
    modbus->Add_Uart(modbus, RUart, MODBUS_RTU, uid);

    modbus->Slave_Reg_Default_ProcessCB(modbus, reg_default_store);
    modbus->Set_Reg(modbus, MODBUS_INPUT_REG, modbus_info->input_reg, INPUT_REG_NUM);
    modbus->Set_Reg(modbus, MODBUS_HOLDING_REG, modbus_info->hold_reg, HOLD_REG_NUM);


    
    uint16_t ver = uflash_info->major_v * 100;
    ver += uflash_info->minor_v;
    update_input_reg(UREG3X_FIRMWARE_VER, ver);
    
    modbus->Set_AddrMap(modbus, reg_map);
    modbus->Slave_Reg_ProcessCB(modbus, UREG4X_LOCK1_CMD, _lock_cmd);
    modbus->Slave_Reg_ProcessCB(modbus, UREG4X_LOCK2_CMD, _lock_cmd);
    modbus->Slave_Reg_ProcessCB(modbus, UREG4X_LOCK3_CMD, _lock_cmd);
    modbus->Slave_Reg_ProcessCB(modbus, UREG4X_LOCK4_CMD, _lock_cmd);
    modbus->Slave_Reg_ProcessCB(modbus, UREG4X_TEMP_HI, change_box_info);    
    modbus->Slave_Reg_ProcessCB(modbus, UREG4X_TEMP_LOW, change_box_info);
    modbus->Slave_Reg_ProcessCB(modbus, UREG4X_LOCK_DELAY, _lock_delay_set);
    modbus->Slave_Reg_ProcessCB(modbus, UREG4X_HUMIDITY_HI, change_box_info);
    modbus->Slave_Reg_ProcessCB(modbus, UREG4X_HUMIDITY_LOW, change_box_info);
    modbus->Slave_Reg_ProcessCB(modbus, UREG4X_VOC_HI, change_box_info);
    modbus->Slave_Reg_ProcessCB(modbus, UREG4X_ALARM_FORCE, _alarm_force);    
    modbus->Slave_Reg_ProcessCB(modbus, UREG4X_SYS_CMD, _sys_cmd);
    modbus->Slave_Reg_ProcessCB(modbus, UREG4X_BOX1_NO, change_box_info);
    modbus->Slave_Reg_ProcessCB(modbus, UREG4X_BOX2_NO, change_box_info);
    modbus->Slave_Reg_ProcessCB(modbus, UREG4X_BOX1_NAME, change_box_info);
    modbus->Slave_Reg_ProcessCB(modbus, UREG4X_BOX2_NAME, change_box_info);
    modbus->Slave_Reg_ProcessCB(modbus, UREG4X_LOCAL_UPDATE, _reboot_local_update);
#endif
}


