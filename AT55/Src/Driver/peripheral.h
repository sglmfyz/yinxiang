#ifndef __PERIPHERAL_H__
#define __PERIPHERAL_H__

#include "common.h"
#include "uart.h"

typedef struct Lock_485 Lock_485_T;


typedef void (*Lock_Callback)(void *lock, int id, int is_opened, uint32_t card);

struct Lock_485 {
    int (*Probe)(Lock_485_T *p, uint8_t **buf, uint16_t *len);
    int (*Open_Door)(Lock_485_T *);
    int (*Close_Door)(Lock_485_T *);
    int (*Query_Lock)(Lock_485_T *);
    int (*MQuery_Lock)(Lock_485_T *);
    int (*Clear_Cards)(Lock_485_T *);
    int (*Query_Param)(Lock_485_T *);
    int (*Set_Param)(Lock_485_T *, uint8_t open_method, uint8_t delay);
    int (*Set_Delay)(Lock_485_T *, uint8_t delay);
    void (*Lock_Status_Change_Callback)(Lock_485_T *, Lock_Callback cb);
    int (*Quit)(Lock_485_T *);
    int (*Is_Mounted)(Lock_485_T *, uint8_t *resp, uint16_t size);
};

typedef struct Relay_485 Relay_485_T;
struct Relay_485 {
    int (*Probe)(Relay_485_T *p, uint8_t **buf, uint16_t *len);
    int (*Open_Door)(Relay_485_T *p, int door_id);
    int (*Close_Door)(Relay_485_T *p, int door_id);
    int (*Set_Delay)(Relay_485_T *, uint8_t delay);
    void (*Lock_Status_Change_Callback)(Relay_485_T *, Lock_Callback cb);
    int (*Is_Mounted)(Relay_485_T *, uint8_t *resp, uint16_t size);
    int (*Quit)(Relay_485_T *);
};


Lock_485_T *Create_Lock485(Uart_T * uart, uint8_t lock_delay);
void Lock485_Start();

Relay_485_T* Create_Relay485(Uart_T *uart, uint8_t lock_delay);
void Relay485_Start();


typedef void (*Door_Callback)(uint8_t door_no, int is_opened);

typedef struct {
    gpio_type *port;
    uint16_t pin;
} board_port_conf;

void door_detect_init();
void door_callback_register(Door_Callback cb);




#endif
