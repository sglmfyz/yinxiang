#ifndef __KEYPAD_H__
#define __KEYPAD_H__

#include "common.h"
#include "process.h"

#define MAX_KEY_NUM 10

typedef enum  {
    LEVEL_FALLING,
    LEVEL_RISING,
} KEY_PRESS_TRIGGER;

typedef enum  {
    KEY_RELEASE, //占位，不会上报该事件
    KEY_PRESS,
    KEY_HOLDING,
    KEY_COMBO,
    KEY_CONTINUES_REPORT,
} KEY_EVENT;


extern process_event_t key_event;
typedef void key_callback(KEY_EVENT event, uint32_t key);

int keypad_init();
int keypad_add_button(GPIO_TypeDef *port, uint32_t pin);
void keypad_set_continuous(int btn_id, int enable);
void keypad_button_trigger(KEY_PRESS_TRIGGER level);
uint16_t key_state_get();
int keypad_set_callback(key_callback *callback);

#endif
