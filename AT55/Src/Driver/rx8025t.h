#ifndef __RX8025T_H__
#define __RX8025T_H__

#include "common.h"
#include "i2c.h"

struct rtc_time {  
    uint8_t tm_sec;  
    uint8_t tm_min;  
    uint8_t tm_hour;  
    uint8_t tm_mday;  
    uint8_t tm_mon;  
    uint8_t tm_year;  
    uint8_t tm_wday;  
    uint8_t tm_yday;   
};

struct my_rtc {
    int (*Get_ReInit_Flag)(struct my_rtc *this, uint8_t *reinit_flag);
	int (*Get_Time)(struct my_rtc *this, struct rtc_time *tm);
	int (*Set_Time)(struct my_rtc *this, struct rtc_time *tm);
};

typedef struct my_rtc RTC_T;

RTC_T *Create_RX8025T(I2C_T *i2c);

#endif