#ifndef __UART_H__
#define __UART_H__

#include "common.h"
#include "process.h"

typedef struct Uart Uart_T;
typedef void (*uart_rx_cb)(Uart_T *this, void *param, uint8_t *pkt, uint16_t pkt_size);

enum {
    PARITY_NONE,
    PARITY_ODD, // 奇校验
    PARITY_EVEN, // 偶校验
};

enum UART_RX_DIR{
    UART_EN_RX_NOT_USED,
    UART_EN_RX_LOW,
    UART_EN_RX_HIGH,
};
    

extern process_event_t uart_rx_event;

struct Uart {
    /**
      * @brief  写一个字节
      * @param  this:  this指针
      * @param  byte:  待写入的字节
      * @retval 写入的字节数
      */
    int (*Write)(Uart_T *this, uint8_t byte);

    /**
      * @brief  写多个字节
      * @param  this: this指针
      * @param  data: 待写入的数据
      * @param  len:  待写入的长度
      * @retval 写入的字节数
      */
    int (*Write_Bytes)(Uart_T *this, uint8_t *data, uint16_t len);

    
    /*设置下半部的进程*/
    int (*Set_Rx_Callback)(Uart_T *this, uart_rx_cb rx_cb, void *param);

    int (*Set_DE_RenPin)(Uart_T *this, gpio_type *GPIO_Port, uint16_t Pin, int dir);

    /**
      * @brief  硬件中断处理函数
      * @param  this: this指针
      * @retval None
    */

    void (*HW_Recv_Byte)(Uart_T *this); 
    
    /**
      * @brief  Rx Exti中断处理函数，软件模拟uart需要快速处理中断，因此中断需要
                传入uart指针调用，而非硬件uart那样可以用通用函数
      * @param  this: this指针
      * @retval None
      */
    void (*SW_Rx_Exti_ISR)(Uart_T *this); 

    /**
      * @brief  Timer中断处理函数，软件模拟uart需要快速处理中断，因此中断需要
                传入uart指针调用，而非硬件uart那样可以用通用函数
      * @param  this: this指针
      * @retval None
      */
    void (*SW_Timer_ISR)(Uart_T *this);

};

/**
  * @brief  软件串口工厂函数，一个软件串口消耗2个gpio端口和1个timer
  * @param  GPIO_Tx: Tx的GPIO Port

  * @param  Tx_Pin:  Tx的GPIO Pin
  * @param  GPIO_RX: Rx的GPIO Port

  * @param  Rx_Pin:  Rx的GPIO Pin，Rx需要设成Exti模式，并开启中断
  * @param  htim:    用于Rx数据采样的timer，在cube里面打开，并开启中断
  * @param  baud：    波特率
  * @retval uart指针
  */
Uart_T *Create_SW_Uart(gpio_type *GPIO_Tx, uint16_t Tx_Pin, 
    gpio_type *GPIO_RX, uint16_t Rx_Pin, tmr_type * htim, uint16_t rxbuf_size, uint32_t baud, uint8_t parity);

/**
  * @brief  硬件串口工厂函数，一个硬件串口需要一个硬件串口控制器
  * @param  UART_hd: uart的句柄，cube的配置需要打开串口，配置好波特率，打开中断
  * @param  rxbuf_size： 数据缓存的大小，由于使用idle中断处理，因此必须至少能缓存一个完整帧的大小
  * @retval uart指针
  */
Uart_T *Create_HW_Uart(usart_type *UART_hd, uint16_t rxbuf_size, uint8_t rxbuf_num);



#endif
