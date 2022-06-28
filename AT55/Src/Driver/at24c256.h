#ifndef __AT24C256_H__
#define __AT24C256_H__

#include "i2c.h"

typedef struct AT24c256 AT24c256_T;

struct AT24c256{
    void (*Set_I2C_Addr)(AT24c256_T *this, I2C_T *i2c, uint8_t addr);
    int (*Write_Bytes)(AT24c256_T *this, uint16_t addr, uint8_t *data, uint16_t len);
    int (*Read_Bytes)(AT24c256_T *this, uint16_t addr, uint8_t *data, uint16_t len);
    int (*Write_Word)(AT24c256_T *this, uint16_t addr, uint16_t data);
    int (*Read_Word)(AT24c256_T *this, uint16_t addr, uint16_t *data);
} ;

AT24c256_T *Create_AT24C256();

#endif

