#ifndef __MISC_485__H__
#define __MISC_485_H__

#include "common.h"
#include "process.h"
#include "peripheral.h"

enum {
    MISC485_TYPE_READER  =1,
};

enum {
    NO_LOCK = 0,
    LOCK_TYPE_485,
    LOCK_TYPE_RELAY,
};

typedef struct Misc485 Misc485_T;
typedef void (*misc485_cb)(int type, int uart_num, void *param);

struct Misc485 {
    int (*Add_Callback)(Misc485_T *this, Lock_Callback lock_cb, misc485_cb cb);
    void (*Open_Lock)(Misc485_T *this, int lock_id);
    void (*Close_Lock)(Misc485_T *this, int lock_id);
    void (*Set_Delay)(Misc485_T *this, uint8_t delay);
    int (*Get_Lock)(Misc485_T *this, int if_id, int *type, void **lock);
} ;


Misc485_T *Create_Misc485(Uart_T *uart1, Uart_T *uart2);
extern Misc485_T *misc485;

#endif
