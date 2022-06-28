#ifndef __GPRS_H__
#define __GPRS_H__
//GPIO_TypeDef

#include "common.h"
#include "uart.h"
#include "process.h"

#define CONNECT_TIMEOUT 10000
#define ICCID_LEN   20
#define LSB_LEN     30 // LSB长度
#define SERVER_ADDR_LEN 50

enum {

    PROTO_UDP,
    PROTO_TCP,
};

enum {
    SENDER_NOT_READY,
    SENDER_AT_FAIL,
    SENDER_REG_OK=100,
    SENDER_IDLE,
};

enum {
    NET_SPEC_ICCID=1,
    NET_SPEC_LSB,
};

typedef void (*Net_Data_Callback_T)(void *data, int size);
typedef void (*Net_Cmd_Callback_T)(void *data, int size);

typedef struct Net Net_T;
struct Net {    
    process_event_t net_state_event;
    
    int (*Set_Cmd_Callback)(Net_T *this, Net_Cmd_Callback_T cb);
    int (*Set_Data_Callback)(Net_T *this, Net_Data_Callback_T cb);
    int (*Connect)(Net_T *this, uint8_t proto, char *server, uint16_t port);
    int (*Reset)(Net_T *this);
    int (*Power_Off)(Net_T *this);
    int (*Send_Pkt)(Net_T *this, void *pkt, int size);
    int (*Get_State)(Net_T *this);
    int (*Send_Cmd)(Net_T *this, char *cmd);
    int (*Set_Networker)(Net_T *this, struct process *process);
    int (*Get_Specified)(Net_T *this, int type, uint8_t *data);
};

Net_T *Create_GPRS(Uart_T *uart, gpio_type *GPIO_PWR, uint16_t PWR_Pin);
Net_T *Create_CAT1(Uart_T *uart, gpio_type *GPIO_PWR, uint16_t PWR_Pin);


Net_T *Create_Wifi(Uart_T *uart, gpio_type *GPIO_Resetn, uint16_t Resetn_Pin, 
    gpio_type *GPIO_Reloadn, uint16_t Reloadn_Pin, gpio_type *GPIO_Readyn, uint16_t Readyn_Pin);


#endif
