#include "uart.h"
#include "gpio.h"
#include "timer.h"
#include "config.h"
#include "heap.h"

#include "process.h"
#include "etimer.h"
#include "list.h"
#include "log.h"

enum{
	COM_START_BIT,
	COM_D0_BIT,
	COM_D1_BIT,
	COM_D2_BIT,
	COM_D3_BIT,
	COM_D4_BIT,
	COM_D5_BIT,
	COM_D6_BIT,
	COM_D7_BIT,
	COM_STOP_BIT,
};

enum {
    STATE_IDLE,
    STATE_BUSY_SEND,
    STATE_BUSY_RECV,
    STATE_RECV_WAIT,
};

typedef struct {
    Uart_T public;
    list_head_t list;
    
    gpio_type *gpio_tx;
    gpio_type *gpio_rx;
    tmr_type *timer;
    
    gpio_type *Re_Port;
    uint16_t Re_Pin;
    uint8_t Re_dir;
    uint16_t tx_pin;
    uint16_t rx_pin;
    uint8_t in_state;
    uint8_t state;

    uint8_t parity_control;
    uint8_t parity_check;
    uint8_t frame_stop_bit;
    
    uint8_t data;
    uint8_t *buf;
    uint16_t buf_size; //buf长度，方便未来扩展外部buffer
    uint16_t buf_index;
    uint16_t sender_len;
    uint8_t sender_bit;

    uint16_t interval; //每个bit间隔us数
    uint16_t period;
    uint16_t timeout; // idle超时时间

    uart_rx_cb cb;    
    void *param;
} sw_uart_info_t;

static list_head_t uart_list;
static int _write_bytes_it(Uart_T *uart, uint8_t *data, uint16_t len);

PROCESS(_sw_uart_recv, "sw_uart_recv");

PROCESS_THREAD(_sw_uart_recv, ev, data)
{
    static sw_uart_info_t *info;
    static list_head_t *node;
    static Uart_T *uart;
    int bytes;

    PROCESS_BEGIN();

    while (1) {
        PROCESS_YIELD();
        
        list_for_each(node, &uart_list) {
            info = container_of(node, sw_uart_info_t, list);
            uart = &info->public;

            bytes = info->buf_index;
            
            if (bytes > 0 && info->state == STATE_IDLE) {
                info->buf_index = 0;
                if (info->cb) {
                    info->cb(uart, info->param, info->buf, bytes);
                }
            }
        }
    }

    PROCESS_END();
}


static int _set_baud(sw_uart_info_t *info, uint32_t baud)
{
    tmr_type* tim;
    uint16_t pres, period;
    uint32_t timer_clock;
    uint32_t freq;

    tim = info->timer;
    timer_clock = get_tim_freq(tim);


    if (baud < 4800 || baud > 115200) {
        /*低于4800和高于115200波特率的未测试，理论上是可以支持的
        */
    
        return -PARAM_ERR;
    }
    
    freq = 4000000;
        
    pres = timer_clock/freq - 1;
    period = freq/ baud;

    info->period = period;
    info->interval = 1000000/baud + 1;
    info->timeout = info->period * 20;

    Update_PSC_ARR(tim, pres, period);
    
    //清除中断状态，避免硬件初始化后第一次rx下降沿开启时钟中断后，立即进入到处理函数。结果会导致获取的第一个rx值为原先的2倍
    tmr_flag_clear(info->timer, TMR_OVF_FLAG);
    info->in_state = info->frame_stop_bit;

    return 0;
}

static int _set_parity(sw_uart_info_t *info, uint8_t parity)
{
    if (parity == PARITY_EVEN || parity == PARITY_ODD) {
         info->parity_control = parity;
         info->frame_stop_bit = COM_STOP_BIT + 1;
    } else {
        info->parity_control = PARITY_NONE;
        info->frame_stop_bit  = COM_STOP_BIT;
    }
    info->in_state = info->frame_stop_bit;


    return 0;
}

static int _write(Uart_T *uart, uint8_t byte)
{
    _write_bytes_it(uart, &byte, 1);

    return 1;
}

static int _write_bytes_it(Uart_T *uart, uint8_t *data, uint16_t len)
{
    // 利用时钟中断来写入，避免死等
    sw_uart_info_t *info;
    
    info = container_of(uart, sw_uart_info_t, public);
    i_assert(info->interval != 0); //调用write之前必须设置波特率；

    if (info->state != STATE_IDLE) {
        syslog_error("Invalid state: %d", info->state);
        return -1;
    }
    if (len > info->buf_size) {
        syslog_error("Packet oversize: %d", info->buf_size);
        return -1;
    }
    memcpy(info->buf, data, len);
    info->sender_len = len;

    // 进入发送模式
    if (info->Re_dir == UART_EN_RX_LOW) {
        GPIO_Write_Hi(info->Re_Port, info->Re_Pin);
    } else if (info->Re_dir == UART_EN_RX_HIGH) {
        GPIO_Write_Low(info->Re_Port, info->Re_Pin);
    }
    
    info->buf_index = 0;
    info->sender_bit = 0;
    info->in_state = COM_START_BIT;
    info->parity_check = 0;
    info->state = STATE_BUSY_SEND;

    // 开启时钟中断开始传输
    tmr_counter_value_set(info->timer, 0);
    tmr_period_value_set(info->timer, info->period);
    Enable_TIMER_Int(info->timer);  

    return 0;
}

static void _write_it(sw_uart_info_t *info) 
{
    uint8_t bit;
    if (info->in_state == COM_START_BIT) {
        GPIO_Write_Low(info->gpio_tx, info->tx_pin); //发送起始位
    } else if (info->in_state < info->frame_stop_bit) {
        bit = (info->buf[info->buf_index] >> info->sender_bit) & 0x1;
        if (bit == 0) {
            GPIO_Write_Low(info->gpio_tx, info->tx_pin);
        } else {
            info->parity_check ^= 1;
            GPIO_Write_Hi(info->gpio_tx, info->tx_pin);
        }
        info->sender_bit++;
    } else if (info->in_state == info->frame_stop_bit) {
        if (info->parity_control != PARITY_NONE) {
            if ((info->parity_control == PARITY_ODD && info->parity_control) ||
               (info->parity_control == PARITY_EVEN && !info->parity_control)) {
                GPIO_Write_Low(info->gpio_tx, info->tx_pin);
            } else {
                GPIO_Write_Low(info->gpio_tx, info->tx_pin);
            }
        } else {
            info->in_state++; // 没有奇偶校验的话，直接走下面流程传输停止位
        }
    }

    if (info->in_state > info->frame_stop_bit) {
        // 停止位
        GPIO_Write_Hi(info->gpio_tx, info->tx_pin);
        info->buf_index++;
        info->sender_bit = 0;
        info->in_state = COM_START_BIT;

        if (info->buf_index >= info->sender_len) {
            // 结束
            if (info->Re_dir == UART_EN_RX_LOW) {
                GPIO_Write_Low(info->Re_Port, info->Re_Pin);
            } else if (info->Re_dir == UART_EN_RX_HIGH) {
                GPIO_Write_Hi(info->Re_Port, info->Re_Pin);
            }
            Disable_TIMER_Int(info->timer);  // 停止时钟中断
            info->state = STATE_IDLE;
            info->buf_index = 0;
        }
    } else {
        info->in_state++;
    }
}



static int _set_ren_pin(Uart_T *uart, gpio_type *GPIO_Port, uint16_t Pin, int dir)
{
    sw_uart_info_t *info;
    
    info = container_of(uart, sw_uart_info_t, public);
    info->Re_Port = GPIO_Port;
    info->Re_Pin = Pin;
    info->Re_dir = dir;

    return 0;
}
static int  _set_rx_callback(Uart_T *uart, uart_rx_cb cb, void *param)
{
    sw_uart_info_t *info;
    
    info = container_of(uart, sw_uart_info_t, public);
    info->cb = cb;
    info->param = param;

    return 0;
}


static void _exti_isr(Uart_T *uart)
{
    sw_uart_info_t *info;
    
    info = container_of(uart, sw_uart_info_t, public);
    if (info) {
        if (GPIO_Read(info->gpio_rx, info->rx_pin) == 0) {
            if (info->state == STATE_RECV_WAIT) {
                info->state = STATE_IDLE;
                tmr_counter_value_set(info->timer, 0);
                Disable_TIMER_Int(info->timer);
            }
            if (info->state == STATE_IDLE) {
                info->in_state = COM_START_BIT;
                info->data = 0;
                info->parity_check = 0;
                //第一次采样往后略微延长一点，防抖。
                tmr_period_value_set(info->timer, info->period + (info->period >> 3));
                Enable_TIMER_Int(info->timer);                
                info->state = STATE_BUSY_RECV;
            }
        }
    }
}


static void _timer_isr(Uart_T *uart)
{
    sw_uart_info_t *info;
    int frame_ok = 1;
    
    info = container_of(uart, sw_uart_info_t, public);

    if (info->state == STATE_BUSY_RECV) {
        info->in_state++;
        
        if(info->in_state == info->frame_stop_bit) {
        	if ((info->parity_control == PARITY_ODD && !info->parity_check) ||
                    (info->parity_control == PARITY_EVEN && info->parity_check)) {
                // 奇校验，但是偶数个1，或者偶校验，奇数个1，则这帧丢弃
                frame_ok = 0;
            }
            
    		if (frame_ok) {
    		    if (info->buf_index < info->buf_size) {
            	    info->buf[info->buf_index++] = info->data;
            	}
            }

            tmr_period_value_set(info->timer, info->timeout); // 等待rx idle
            info->state = STATE_RECV_WAIT;
            return;
        } else if (info->in_state == COM_D0_BIT) {
            //恢复1bit的采样间隔
            tmr_period_value_set(info->timer, info->period);
        }
        if (GPIO_Read(info->gpio_rx, info->rx_pin)) {
            info->data |= 1 << (info->in_state - 1);
        } else {
            info->data &= ~(1 << (info->in_state - 1));
        }
    } else if (info->state == STATE_RECV_WAIT) {
        info->state = STATE_IDLE;
        tmr_counter_value_set(info->timer, 0);
        Disable_TIMER_Int(info->timer);
        process_poll(&_sw_uart_recv);
    } else if (info->state == STATE_BUSY_SEND) {
        _write_it(info);
    }
}


static int _init(sw_uart_info_t *info, gpio_type *GPIO_Tx, uint32_t Tx_Pin, 
    gpio_type *GPIO_RX, uint32_t Rx_Pin, tmr_type* htim, uint16_t rxbuf_size)

{
    Uart_T *public;
    
    info->gpio_tx = GPIO_Tx;
    info->tx_pin = Tx_Pin;
    info->gpio_rx = GPIO_RX;
    info->rx_pin = Rx_Pin;
    info->timer  = htim; 
    zmalloc(info->buf, rxbuf_size); //此缓冲区用于实现队列，硬件收到数据时放在此处
    info->buf_size = rxbuf_size;

    i_assert(info->gpio_tx != NULL || (info->gpio_rx != NULL) && htim != NULL);

    public = &info->public;
    public->Write = _write;
    public->Write_Bytes = _write_bytes_it;
    public->SW_Rx_Exti_ISR = _exti_isr;
    public->SW_Timer_ISR = _timer_isr;
    public->Set_DE_RenPin = _set_ren_pin;
    public->Set_Rx_Callback = _set_rx_callback;

    if (uart_list.next == NULL) {
        LIST_HEAD_INIT(&uart_list);
        process_start(&_sw_uart_recv, NULL);
    }
    list_add_tail(&info->list, &uart_list);
    
    return 0;
}

Uart_T *Create_SW_Uart(gpio_type *GPIO_Tx, uint16_t Tx_Pin, 
    gpio_type *GPIO_RX, uint16_t Rx_Pin, tmr_type* htim, uint16_t rxbuf_size, uint32_t baud, uint8_t parity)
{
    sw_uart_info_t *info;

    zmalloc(info, sizeof(sw_uart_info_t));

    if (_init(info, GPIO_Tx, Tx_Pin, GPIO_RX, Rx_Pin, htim, rxbuf_size) == 0 
    	&& _set_parity(info, parity) == 0 && _set_baud(info, baud) == 0) {
        return &info->public;
    } else {
        return NULL;
    }
}

