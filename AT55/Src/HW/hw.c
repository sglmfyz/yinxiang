#include "hw.h"

I2C_T *i2c1, *i2c2, *i2c3, *i2c4, *i2c5, *i2ce;
Uart_T *uart1, *uart2, *uart3,*RUart;

void HW_Init(void)
{
    int status;

    i2c1 = Create_SW_I2C(GPIOB, GPIO_PINS_9, GPIOB, GPIO_PINS_8);
    i2c2 = Create_SW_I2C(GPIOB, GPIO_PINS_7, GPIOB, GPIO_PINS_6);
    i2c3 = Create_SW_I2C(GPIOB, GPIO_PINS_5, GPIOB, GPIO_PINS_4);
    i2c4 = Create_SW_I2C(GPIOB, GPIO_PINS_12, GPIOB, GPIO_PINS_11);
    i2c5 = Create_SW_I2C(GPIOB, GPIO_PINS_10, GPIOB, GPIO_PINS_2);
    i2ce = Create_SW_I2C(GPIOB, GPIO_PINS_3, GPIOB, GPIO_PINS_13);

    uart1 = Create_HW_Uart(USART1, 512, 2);
    status = uart1->Set_DE_RenPin(uart1, GPIOA, GPIO_PINS_11, UART_EN_RX_LOW);
    i_assert(status == 0);
    
    uart2 = Create_HW_Uart(USART2, 512, 1);
    status = uart2->Set_DE_RenPin(uart2, GPIOA, GPIO_PINS_0, UART_EN_RX_LOW);
    i_assert(status == 0);

    uart3 = Create_SW_Uart(GPIOA, GPIO_PINS_8, GPIOF, GPIO_PINS_7, TMR16, 512, 115200, PARITY_NONE);
    status = uart3->Set_DE_RenPin(uart3, GPIOF, GPIO_PINS_6, UART_EN_RX_LOW);
    i_assert(status == 0);
}



