#ifndef __MODBUS_H__
#define __MODBUS_H__

#include "common.h"
#include "uart.h"

#define MAX_MASTER_POLL_NUM 8
#define MAX_REG_MONITOR 512

typedef struct MBSlave MBSlave_T;
typedef struct MBMaster MBMaster_T;

enum {
    MODBUS_RTU,
    MODBUS_ASCII,
};

enum {
    MODBUS_INPUT_REG,
    MODBUS_HOLDING_REG,
    MODBUS_COIN_REG,
    MODBUS_DISCRETE_REG,
    MODBUS_REG_TYPE_NUM,
};

/**
 * @brief 外部寄存器（外部器件地址）转成内部寄存器（程序内部地址）的回调函数
 * @param type: MODBUS_INPUT_REG Or MODBUS_HOLDING_REG
 * @param addr: 外部寄存器地址
 * @retval 内部寄存器地址
*/
typedef uint16_t (*ModAddr_Map)(uint8_t type, uint16_t addr);

/**
  * @brief  Holding Register写入后，需要处理的寄存器回调
  * @param  addr：地址
  * @retval None
  */
typedef void (*reg_process_cb)(uint16_t addr);

typedef int (*extra_func_cb)(MBSlave_T *modbus, uint8_t *buf, uint16_t size);


struct MBSlave{    
    /**
      * @brief  设置Slave的寄存器指针
      * @param  this: this指针
      * @param  reg_type: MODBUS_INPUT_REG或MODBUS_HOLDING_REG,
      * @param  reg: reg指针
      * @param  num: reg个数
      * @retval modbus指针
      */
    int (*Set_Reg)(MBSlave_T *this, uint8_t reg_type, uint16_t *reg, uint16_t num);
    /**
      * @brief  设置Slave的模式
      * @param  this: this指针
      @param    uart: uart指针
      * @param  mode: MODBUS_RTU或MODBUS_ASCII
      * @param  uid: 站号
      * @retval -0: 成功
                其他值: 失败
      */
    int (*Add_Uart)(MBSlave_T *this, Uart_T *uart, uint8_t mode, uint8_t uid);
      
    int (*Set_AddrMap)(MBSlave_T *this, ModAddr_Map map_fn);

    int (*Slave_ExtraFuncCB)(MBSlave_T *this, uint8_t func, extra_func_cb cb);

    /**
      * @brief  注册Holding register写入处理回调
      * @param  this: this指针
      * @param  addr: reg地址
      * @param  cb: 回调
      * @retval -0: 成功
                其他值: 失败
      */
    int (*Slave_Reg_ProcessCB)(MBSlave_T *this, uint16_t addr, reg_process_cb cb);
	/**
      * @brief  注册Holding register写入处理默认回调
      * @param  this: this指针
      * @param  cb: 回调
      * @retval -0: 成功
                其他值: 失败
      */
	int (*Slave_Reg_Default_ProcessCB)(MBSlave_T *this, reg_process_cb cb);

};


/**
 * @brief 外部寄存器（外部器件地址）转成内部寄存器（程序内部地址）的回调函数
 * @param type: MODBUS_INPUT_REG Or MODBUS_HOLDING_REG
 * @param addr: 外部寄存器地址
 * @retval 内部寄存器地址
*/
typedef uint16_t (*ModAddr_MasterMap)(uint8_t type, uint8_t uid, uint16_t addr);


struct MBMaster {
    int (*Set_Reg)(MBMaster_T *this, uint8_t reg_type, uint16_t *reg, uint16_t num);
    int (*Add_Master_Poll)(MBMaster_T *this, uint8_t func, uint16_t addr, uint16_t num);
    int (*Write_Regs)(MBMaster_T *this, uint16_t reg_start, uint16_t *values, uint16_t num);
    int (*Set_AddrMap)(MBSlave_T *this, ModAddr_MasterMap map_fn);
};


/**
  * @brief  创建modbus_slave
  * @param  uart: uart指针
  * @retval modbus指针
  */
MBSlave_T *Create_Modbus_Slave();

MBMaster_T *Create_Modbus_Master();

#endif

