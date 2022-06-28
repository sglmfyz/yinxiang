#ifndef __TIMER_H__
#define __TIMER_H__

#include "common.h"


#define Enable_TIMER_Int(timer) do {  \
    tmr_interrupt_enable(timer, TMR_OVF_INT, TRUE);  \
    tmr_counter_enable(timer, TRUE);    \
} while(0)

#define Disable_TIMER_Int(timer) do {  \
    tmr_interrupt_enable(timer, TMR_OVF_INT, FALSE);  \
    tmr_counter_enable(timer, FALSE);    \
} while(0)
    


#define Update_PSC_ARR(timer, psc, arr) do {   \
    tmr_base_init(timer, arr, psc);             \
} while (0)

#endif
