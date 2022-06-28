#ifndef __I2C_H__
#define __I2C_H__

#include "common.h"

enum ENUM_SI2C_BUS_STATE
{
    SI2C_READY=0,
    SI2C_BUS_BUSY = 1,
    SI2C_BUS_ERROR = 2,    
    SI2C_NACK = 3,
    SI2C_ACK = 4, 
};

typedef struct I2C I2C_T;

struct I2C{
    int (*Set_Delay)(I2C_T *this, uint16_t us);
    
    int (*Start)(I2C_T *this);
    void (*Stop)(I2C_T *this);
    void (*Send_Ack)(I2C_T *this);
    void (*Send_Nack)(I2C_T *this);
    int (*Send_Byte)(I2C_T *this, uint8_t Data, uint16_t ack_us);
    uint8_t (*Recv_Byte)(I2C_T *this);
    
    int (*Mem_Write)(I2C_T *this, uint8_t DevAddress, uint8_t *MemAddress, uint16_t AddrSize, uint8_t *pData, uint16_t Size);
    int (*Mem_Read)(I2C_T *this, uint8_t DevAddress, uint8_t *MemAddress, uint16_t AddrSize, uint8_t *pData, uint16_t Size);
    int (*Master_Transmit)(I2C_T *this, uint8_t DevAddress, uint8_t *pData, uint16_t Size);
    int (*Master_Receive)(I2C_T *this, uint16_t DevAddress, uint8_t *pData, uint16_t Size);
};


I2C_T *Create_SW_I2C(gpio_type       *clk_io, uint16_t clk_pin, gpio_type *data_io, uint16_t data_pin);


#endif
