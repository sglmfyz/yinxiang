#ifndef __MISC_H__
#define __MISC_H__

#include "common.h"

#define DC_FAN_MIN 30
#define DC_FAN_MAX 100

#define MAX_VOC_NUM 2

int misc_init(TIM_HandleTypeDef *dc_tim, uint32_t Channel);
void beeper_short(int times);
void beeper_long();
void beep_ctrl(int on)
;
void beeper_raw(int ms, int times);
void backlight_ctrl(int on);


void light_ctrl(int on);
void backup_ctrl(int on);
void fan_ctrl(int speed, BOOL dc);
void alarm_ctrl(int on);



#endif
