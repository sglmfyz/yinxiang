#include "keypad.h"
#include "etimer.h"
#include "util.h"
#include "log.h"
#include "heap.h"
#include "misc.h"

enum {
    HOLDING_CONTINUOUS_BIT
};

typedef struct {
    GPIO_TypeDef *port;
    uint32_t pin;
    uint32_t press_time;
    uint32_t flag;
} button_info_t;

#define KEY_QUEUE_SIZE 16
#define LONG_PRESS_TIMEOUT 1200
#define LONG_REPORT_TIMEOUT 280


static struct {
    KEY_PRESS_TRIGGER trigger;
    key_callback *callback;
    button_info_t btn[MAX_KEY_NUM];    
} *keypad_info;

#define KEY_CHECK_TIMEOUT 50
#define KEY_CONTINUOUS_TIMEOUT 60

uint32_t key_processed;
uint32_t key_event_add;

process_event_t key_event;

PROCESS(_key_process, "key");

PROCESS_THREAD(_key_process, ev, data)
{
    static uint16_t old_state;    
    static uint16_t todo_mask;
    static struct etimer timer;
    static int timeout;
    
    uint16_t new_state;
    button_info_t *btn;
    int i;
    BOOL continuous = FALSE;


    PROCESS_BEGIN();
    todo_mask = 0;    
    timeout = KEY_CHECK_TIMEOUT;

    while (1) {
        etimer_set(&timer, timeout);
        key_event_add++;
        PROCESS_WAIT_UNTIL(etimer_expired(&timer));
        key_processed++;
        new_state = key_state_get();
        
        continuous = FALSE;
        for (i=0;  i<MAX_KEY_NUM; i++) {
            btn = &keypad_info->btn[i];
            if (btn->port == NULL) {
                break;
            }
            if (bit_is_clr(old_state, i) && bit_is_set(new_state, i)) {
                // 有键按下
                //beeper_short(1);
                btn->press_time = Micros();
                if (new_state & ~(1<<i)) {
                    //有组合键按下，立即上报
                    todo_mask &= ~new_state;
                    if (keypad_info->callback) {
                        keypad_info->callback(KEY_COMBO, new_state);
                    }
                } else {
                    //没有组合键，再等等看是否是长按
                    bit_set(todo_mask, i);
                }
            } else if (bit_is_set(old_state, i) && bit_is_clr(new_state, i)) {
                // 按键释放了
                if (bit_is_set(todo_mask, i)) {
                    bit_clr(todo_mask, i);

                    if (Micros() - btn->press_time <= LONG_PRESS_TIMEOUT) {
                        if (keypad_info->callback) {
                            keypad_info->callback(KEY_PRESS, i+1);
                        }
                    }
                }
            } else if (bit_is_set(old_state, i) && bit_is_set(new_state, i) &&
                bit_is_set(todo_mask, i) && 
                Micros() - btn->press_time >= LONG_PRESS_TIMEOUT) {
                //holding test
                
                if (bit_is_set(btn->flag, HOLDING_CONTINUOUS_BIT)) {
                    if (Micros() - btn->press_time <= LONG_PRESS_TIMEOUT + KEY_CHECK_TIMEOUT) {
                        beeper_short(1);
                    } 
                    if (keypad_info->callback) {
                        keypad_info->callback(KEY_CONTINUES_REPORT, i+1);
                        continuous = TRUE;
                    }
                } else {
                    //report
                    if (keypad_info->callback) {
                        keypad_info->callback(KEY_HOLDING, i+1);
                    }
                    bit_clr(todo_mask, i);
                }
            }
        }
        if (continuous) {
            timeout = KEY_CONTINUOUS_TIMEOUT;
        } else {
            timeout = KEY_CHECK_TIMEOUT;
        }
        old_state = new_state;
    }
    PROCESS_END();
}



int keypad_init()
{
    zmalloc(keypad_info, sizeof(*keypad_info));
    keypad_info->trigger = LEVEL_FALLING;
    keypad_info->callback = NULL;
    key_event = process_alloc_event();

    key_processed = 0;
    key_event_add = 0;
    process_start(&_key_process, NULL);
    return 0;
}

void keypad_button_trigger(KEY_PRESS_TRIGGER level)
{
    keypad_info->trigger = level;
}

int keypad_add_button(GPIO_TypeDef *port, uint32_t pin)
{
    int i;
    button_info_t *info;

    for (i=0; i<MAX_KEY_NUM; i++) {
        if (keypad_info->btn[i].port == NULL) {
            break;
        }
    }
    i_assert(i < MAX_KEY_NUM);
    info = &keypad_info->btn[i];
    info->port = port;
    info->pin = pin;

    return i+1;
}

void keypad_set_continuous(int btn_id, int enable)
{
    if (btn_id > 0 && btn_id <= MAX_KEY_NUM) {
        if (enable) {
            bit_set(keypad_info->btn[btn_id-1].flag, HOLDING_CONTINUOUS_BIT);
        } else {
            bit_clr(keypad_info->btn[btn_id-1].flag, HOLDING_CONTINUOUS_BIT);
        }
    }
}


uint16_t key_state_get()
{
    uint16_t mask = 0;
    button_info_t *info;
    int i;

    for (i=0; i<MAX_KEY_NUM; i++) {
        info = &keypad_info->btn[i];
        if (info->port == NULL) {
            break;
        }
        if (HAL_GPIO_ReadPin(info->port, info->pin)) {
            bit_set(mask, i);
        } 
    }
    if (keypad_info->trigger == LEVEL_FALLING) {
        mask = ~mask;
    }
    mask &= (1 << i) - 1;
    return mask;
}

int keypad_set_callback(key_callback *callback)
{
    keypad_info->callback = callback;
    
    return 0;
}



