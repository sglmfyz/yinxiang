#ifndef __HW_H__
#define __HW_H__

#include "i2c.h"
#include "uart.h"

extern I2C_T *i2c1, *i2c2, *i2c3, *i2c4, *i2c5, *i2ce;
extern Uart_T *uart1, *uart2, *uart3;

void HW_Init(void);

#endif
