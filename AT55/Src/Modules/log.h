#ifndef __LOG_H__
#define __LOG_H__

#include <stdio.h>
#include "common.h"

enum {
    EMERG,
    ALERT,
    CRIT,
    ERR,
    INFO,
    DEBUG,
};
extern int log_level;


int Log_SW_Init(gpio_type *GPIO_Tx, uint16_t Tx_Pin, uint32_t baud);
int Log_SW_SetLevel(int level);

#define syslog_print(level, fmt, args...) do { \
    if (level <= log_level) {  \
        printf(fmt, ##args);   \
    }      \
}while (0)  


#define syslog_emege(fmt, args...) syslog_print(EMERG, fmt, ##args)
#define syslog_alert(fmt, args...) syslog_print(ALERT, fmt, ##args)
#define syslog_critical(fmt, args...) syslog_print(CRIT, fmt, ##args)
#define syslog_error(fmt, args...) syslog_print(ERR, fmt, ##args)
#define syslog_info(fmt, args...) syslog_print(INFO, fmt, ##args)
#define syslog_debug(fmt, args...) syslog_print(DEBUG, fmt, ##args)
#define syslog_raw printf

#define TRACE printf("TRACE:%s:%s, line %d\n", __FILE__,  __FUNCTION__, __LINE__)



#endif
