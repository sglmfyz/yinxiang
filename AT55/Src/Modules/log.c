#include <stdio.h>
#include <stdarg.h>
#include <string.h>

#include "common.h"
#include "util.h"
#include "gpio.h"
#include "log.h"

static int read_bc = 0;
static gpio_type *gpio_tx;
static uint16_t tx_pin;
static uint16_t interval;
int log_level;

static void __serial_write_byte(char byte)
{
    int i;
    uint8_t tmp;

    GPIO_Write_Low(gpio_tx, tx_pin); //发送起始位
    Delay_us(interval);

    for(i=0;i<8;i++) {
        tmp = (byte >> i) & 0x01;  //低位在前
        if(tmp == 0) {
            GPIO_Write_Low(gpio_tx, tx_pin);
            Delay_us(interval); //0     
        }
        else
        {
            GPIO_Write_Hi(gpio_tx, tx_pin);
            Delay_us(interval); //1     
        }   
    }
    GPIO_Write_Hi(gpio_tx, tx_pin);
    Delay_us(interval);
}

static int __serial_send(char *buf, uint16_t size)
{
    int i;
    
    for (i=0; i<size; i++) {
    	 __serial_write_byte(buf[i]);
    }
    return size;
}

int putchar(int ch)
{
    /* Place your implementation of fputc here */
    /* e.g. write a character to the USART */
    char buf[2];
    buf[0] = ch;

    if (ch == '\r') {
        read_bc = 1;
    } else if (ch == '\n') {
        if (!read_bc) {
            buf[1] = '\r';
            __serial_send(buf, 2);
            return ch;
        }
        read_bc = 0;
    } else if (read_bc) {
        read_bc = 0;
    }

    __serial_send(buf, 1);
    /* Loop until the end of transmission */
    return ch;
}

int Log_SW_Init(gpio_type *GPIO_Tx, uint16_t Tx_Pin, uint32_t baud)
{    
    gpio_tx = GPIO_Tx;
    tx_pin = Tx_Pin;
    interval = 1000000/baud;
    log_level = DEBUG;
    GPIO_Write_Hi(gpio_tx, tx_pin);

    return 0;    
}

int Log_SW_SetLevel(int level)
{
    log_level = level;
    return 0;
}

void assert_failed(uint8_t *file, int line)
{
    while (1) {
        printf("FILE: %s, line:%d\n", file, line); 
        Delay_ms(1000); 
    }
 
}

