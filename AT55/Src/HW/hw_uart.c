#include "uart.h"

#include "gpio.h"
#include "heap.h"
#include "list.h"
#include "log.h"
#include "util.h"

#define RX_MAX_BUF_NUM 2 
// 硬件的uart似乎idle中断经常会断包，暂时用多个缓存，避免cat1_7630发包找不到的问题

typedef struct {
    Uart_T public;
    list_head_t list;
    usart_type *hd;
    uart_rx_cb rx_cb;
    void *param;
    int pkt;
    
    uint8_t *rx_buf[RX_MAX_BUF_NUM];
    uint16_t rdx[RX_MAX_BUF_NUM];
    uint16_t recv_len[RX_MAX_BUF_NUM];
    uint16_t rx_size;
    uint8_t rxbuf_num;
    uint8_t rxbuf_id;
    gpio_type *Re_Port;
    uint16_t Re_Pin;
    int state;
    int Re_dir;
    uint16_t timeout;
} hw_uart_info_t;

static void _pkt_process(hw_uart_info_t *info);
static list_head_t uart_list;


PROCESS(_hw_uart_recv, "hw_uart_recv");

PROCESS_THREAD(_hw_uart_recv, ev, data)
{
    hw_uart_info_t *info;
    list_head_t *node;

    PROCESS_BEGIN();

    while (1) {
        PROCESS_YIELD();
        
        list_for_each(node, &uart_list) {
            info = container_of(node, hw_uart_info_t, list);
            if (info->pkt > 0) {
                info->pkt = 0;
                _pkt_process(info);
            }
        }
    }

    PROCESS_END();

    
}


static void _pkt_process(hw_uart_info_t *info)
{    
    int cur_id = info->rxbuf_id;
    int size = info->recv_len[cur_id];

    if (size > 0) {
        info->rxbuf_id += 1;
        if (info->rxbuf_id >= info->rxbuf_num) {
            info->rxbuf_id = 0;
        }
        
        if (info->rx_cb) {     
            info->rx_cb(&info->public, info->param, info->rx_buf[cur_id], info->recv_len[cur_id]);
        } 
        info->recv_len[cur_id] = 0;
    } 
}

static int _write_bytes(Uart_T *uart, uint8_t *data, uint16_t size)
{
    hw_uart_info_t *info;
    int i;

    info = container_of(uart, hw_uart_info_t, public);

    if (info->Re_dir == UART_EN_RX_LOW) {
        //收和发的方向正好相反
        GPIO_Write_Hi(info->Re_Port, info->Re_Pin);
    } else if (info->Re_dir == UART_EN_RX_HIGH) {
        GPIO_Write_Low(info->Re_Port, info->Re_Pin);
    }

    for (i=0; i<size; i++) {
        while(usart_flag_get(info->hd, USART_TDC_FLAG) != SET);
        if (data != NULL) {
            usart_data_transmit(info->hd, data[i]);
        }
    }
    while(usart_flag_get(info->hd, USART_TDC_FLAG) != SET);

    if (info->Re_dir == UART_EN_RX_LOW) {
        GPIO_Write_Low(info->Re_Port, info->Re_Pin);
    } else if (info->Re_dir == UART_EN_RX_HIGH) {
        GPIO_Write_Hi(info->Re_Port, info->Re_Pin);
    }

    return size;
}

static int _write(Uart_T *uart, uint8_t byte)
{
    return _write_bytes(uart, &byte, 1);
}


static int _set_ren_pin(Uart_T *uart, gpio_type *GPIO_Port, uint16_t Pin, int dir)
{
    hw_uart_info_t *info;
    
    info = container_of(uart, hw_uart_info_t, public);
    info->Re_Port = GPIO_Port;
    info->Re_Pin = Pin;
    info->Re_dir = dir;

    return 0;
}

int _set_rx_cb(Uart_T *uart, uart_rx_cb rx_cb, void *param)
{
    hw_uart_info_t *info;

    info = container_of(uart, hw_uart_info_t, public);
    info->rx_cb = rx_cb;
    info->param = param;

    return 0;
}

void _hw_recv_byte(Uart_T *uart)
{
    hw_uart_info_t *info;
    int buf_id;
    
    info = container_of(uart, hw_uart_info_t, public);
    buf_id = info->rxbuf_id;

    if(usart_flag_get(info->hd, USART_RDBF_FLAG) != RESET) {
        if (info->recv_len[buf_id] < info->rx_size) {
            info->rx_buf[buf_id][info->recv_len[buf_id]] = usart_data_receive(info->hd);
            info->recv_len[buf_id]++;
        }
    } 
    if (usart_flag_get(info->hd, USART_IDLEF_FLAG) != RESET) {
        usart_data_receive(info->hd);
        info->pkt = 1;
        process_poll(&_hw_uart_recv); // 在下半部处理回调
    }
}

Uart_T *Create_HW_Uart(usart_type *UART_hd, uint16_t rxbuf_size, uint8_t rxbuf_num)
{
    hw_uart_info_t *info;
    Uart_T *public;
    int i;

    zmalloc(info, sizeof(hw_uart_info_t));
    i_assert(rxbuf_num <= RX_MAX_BUF_NUM);
    
    if (rxbuf_size > 0 && rxbuf_num > 0) {
        for (i=0; i<rxbuf_num; i++) {
            zmalloc(info->rx_buf[i], rxbuf_size);
        }
        info->rxbuf_num = rxbuf_num;
    }

    info->hd = UART_hd;
    info->rx_size = rxbuf_size;
    info->rxbuf_id = 0;
    info->timeout = 12 * (1000000/UART_hd->baudr + 1);
    
    public = &info->public;
    public->Write = _write;
    public->Write_Bytes = _write_bytes;
    public->HW_Recv_Byte = _hw_recv_byte;
    public->Set_DE_RenPin = _set_ren_pin;
    public->Set_Rx_Callback = _set_rx_cb;
    
    if (uart_list.next == NULL) {
        LIST_HEAD_INIT(&uart_list);
        process_start(&_hw_uart_recv, NULL);
    }
    list_add_tail(&info->list, &uart_list);
    
    return public;
}

