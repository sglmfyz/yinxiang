#include "misc.h"
#include "process.h"
#include "etimer.h"
#include "heap.h"
#include "util.h"
#include "log.h"
#include "sys_reg.h"
#include "shell.h"

#define BEEPER_EV_NUM 3

static struct {
    process_event_t beeper_ev;
    TIM_HandleTypeDef *dc_tim;
    uint16_t flag;
    uint16_t beeper_data[BEEPER_EV_NUM];
    uint8_t beeper_front, beeper_rear;
} *misc_info;

enum {
    SHORT_BEEPER,
    LONG_BEEPER,
};


#define FAN_PORT RELAY3_GPIO_Port
#define FAN_PIN RELAY3_Pin

#define LIGHT_PORT RELAY1_GPIO_Port
#define LIGHT_PIN RELAY1_Pin

#define BACKUP_PORT RELAY0_GPIO_Port
#define BACKUP_PIN RELAY0_Pin

#define ALARM_PORT RELAY2_GPIO_Port
#define ALARM_PIN RELAY2_Pin

#define beep_on() HAL_GPIO_WritePin(BEEPER_CTRL_GPIO_Port, BEEPER_CTRL_Pin, GPIO_PIN_SET);
#define beep_off() HAL_GPIO_WritePin(BEEPER_CTRL_GPIO_Port, BEEPER_CTRL_Pin, GPIO_PIN_RESET);


#define DISPLAY_FAN_STEP 3

PROCESS(_beeper_process, "beeper");

PROCESS_THREAD(_beeper_process, ev, data)
{
    int v;
    static struct etimer timer;
    static uint16_t i, times;
    
    PROCESS_BEGIN();


    while (TRUE) {
        PROCESS_WAIT_EVENT_UNTIL(ev == misc_info->beeper_ev);

        while (!queue_is_empty(misc_info->beeper_front, misc_info->beeper_rear)) {
            v = (uint16_t)misc_info->beeper_data[misc_info->beeper_front];
            queue_fetch(misc_info->beeper_front, misc_info->beeper_rear, BEEPER_EV_NUM);
        
            if (bit_is_set(v, SHORT_BEEPER)) {
                times = (v >> 8) & 0xff;
                for (i=0; i<times; i++) {
                    beep_on();
                    etimer_set(&timer, 100);
                    PROCESS_WAIT_UNTIL(etimer_expired(&timer));

                    beep_off();                
                    etimer_set(&timer, 100);
                    PROCESS_WAIT_UNTIL(etimer_expired(&timer));
                }
            } else if (bit_is_set(v, LONG_BEEPER)) {
                beep_on();
                etimer_set(&timer, 500);
                PROCESS_WAIT_UNTIL(etimer_expired(&timer));
                beep_off();
            }
        }
    }
    PROCESS_END();
}

PROCESS(_fan_process, "fan");

PROCESS_THREAD(_fan_process, ev, data)
{
    static int speed_set;
    static struct etimer etime;
    int16_t reg;
    PROCESS_BEGIN();

    while (TRUE) {
        PROCESS_YIELD();

        while (Sys_Reg_Get(SYS_REG_FAN_SPEED_SET) != Sys_Reg_Get(SYS_REG_FAN_SPEED)) {
            speed_set = Sys_Reg_Get(SYS_REG_FAN_SPEED_SET);
            etimer_set(&etime, 350);//350ms刷新一次
            PROCESS_WAIT_UNTIL(etimer_expired(&etime));

            reg = (int16_t)Sys_Reg_Get(SYS_REG_FAN_SPEED);
            if (speed_set != reg) {
                if (speed_set < reg) {
                    reg -= DISPLAY_FAN_STEP;
                } else {
                    reg += DISPLAY_FAN_STEP;
                }
            }
            if (abs(reg-speed_set) < DISPLAY_FAN_STEP) {
                reg = speed_set;
            }
            
            setv_restrict(reg, 0, 100);
            Sys_Reg_Update(SYS_REG_FAN_SPEED, reg);
            syslog_debug("Fan process %d\n", reg);
        } 
    }
    
    PROCESS_END();
}


void beeper_short(int times)
{
    int v;
    
    v = (times << 8) | (1 << SHORT_BEEPER);
    if (!queue_is_full(misc_info->beeper_front, misc_info->beeper_rear, BEEPER_EV_NUM)) {
        misc_info->beeper_data[misc_info->beeper_rear] = v;
        queue_push(misc_info->beeper_front, misc_info->beeper_rear, BEEPER_EV_NUM);
        process_post(&_beeper_process, misc_info->beeper_ev, NULL);
    }
}

void beeper_raw(int ms, int times)
{
    int i;

    for (i=0; i<times; i++) {
        beep_on();
        Delay_MS(ms);
        beep_off();
        Delay_MS(ms);
    }
}


void beeper_long()
{
    int v;
    
    v = 1 << LONG_BEEPER;

    if (!queue_is_full(misc_info->beeper_front, misc_info->beeper_rear, BEEPER_EV_NUM)) {
        misc_info->beeper_data[misc_info->beeper_rear] = v;
        queue_push(misc_info->beeper_front, misc_info->beeper_rear, BEEPER_EV_NUM);
        process_post(&_beeper_process, misc_info->beeper_ev, (void *)v);
    }
}

void backlight_ctrl(int on)
{
    if (on) {
        HAL_GPIO_WritePin(LCD_LIGHT_DRV_GPIO_Port, LCD_LIGHT_DRV_Pin, GPIO_PIN_SET);
    } else {
        HAL_GPIO_WritePin(LCD_LIGHT_DRV_GPIO_Port, LCD_LIGHT_DRV_Pin, GPIO_PIN_RESET);
    }
}

void beep_ctrl(int on)
{
    if (on) {
        beep_on();
    } else {
        beep_off();
    }
}

void light_ctrl(int on)
{
    if (on) {
        HAL_GPIO_WritePin(LIGHT_PORT, LIGHT_PIN, GPIO_PIN_SET);
    } else {
        HAL_GPIO_WritePin(LIGHT_PORT, LIGHT_PIN, GPIO_PIN_RESET);
    }
}

void backup_ctrl(int on)
{
    if (on) {
        HAL_GPIO_WritePin(BACKUP_PORT, BACKUP_PIN, GPIO_PIN_SET);
    } else {
        HAL_GPIO_WritePin(BACKUP_PORT, BACKUP_PIN, GPIO_PIN_RESET);
    }
}

void dc_fan_speed(int speed)
{
    int total_counter = misc_info->dc_tim->Instance->ARR + 1;
    int counter;

    counter = (speed * total_counter * 11) / (100*20);  //实测55%就已经达到最高速度，因此这里计算一下。
    misc_info->dc_tim->Instance->CCR1 = counter;
}

void fan_ctrl(int speed, BOOL dc)
{
    setv_restrict(speed, 0, 100);
    if (!dc) {
        if (speed > 0) {
            speed = 100;
            HAL_GPIO_WritePin(FAN_PORT, FAN_PIN, GPIO_PIN_SET);
        } else {
            HAL_GPIO_WritePin(FAN_PORT, FAN_PIN, GPIO_PIN_RESET);
        }    
    } else {        
        if (speed < DC_FAN_MIN) {
            speed = 0;
        } 
        backup_ctrl(speed > 0);
        dc_fan_speed(speed);
        HAL_GPIO_WritePin(FAN_PORT, FAN_PIN, GPIO_PIN_RESET);
    }
    Sys_Reg_Update(SYS_REG_FAN_SPEED_SET, speed);
    process_poll(&_fan_process);
}

void alarm_ctrl(int on)
{
    if (on) {
        HAL_GPIO_WritePin(ALARM_PORT, ALARM_PIN, GPIO_PIN_SET);
    } else {
        HAL_GPIO_WritePin(ALARM_PORT, ALARM_PIN, GPIO_PIN_RESET);
    }
}

static int _misc(Shell_T *this, void *caller, int argc, char *argv[])
{
    if (argc >= 1) {
        if (strcmp(argv[0], "dc")) {
            if (argc >= 2) {
                int speed = atoi(argv[1]);
                setv_restrict(speed, 0, 100);
                dc_fan_speed(speed);
                this->Println(this, "Set fan speed %d\n", speed);

                return SHELL_RT_OK;
            }
        }
    }
    return SHELL_RT_HELP;
}

int misc_init(TIM_HandleTypeDef *dc_tim, uint32_t Channel)
{
    zmalloc(misc_info, sizeof(*misc_info));
    misc_info->beeper_ev = process_alloc_event();
    misc_info->dc_tim = dc_tim;
    dc_fan_speed(0);
    HAL_TIM_PWM_Start(dc_tim, Channel);
        
    process_start(&_beeper_process, NULL);
    process_start(&_fan_process, NULL);

    //Main_Shell->Register(Main_Shell, "misc", misc_info, _misc, "misc dc [0-100]");
    return 0;    
}

