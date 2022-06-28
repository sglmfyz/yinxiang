#include "common.h"
#include "process.h"
#include "etimer.h"
#include "log.h"
#include "gpio.h"
#include "sensors.h"
#include "hw.h"
#include "modbus.h"
#include "heap.h"



PROCESS(_led_process, "led_process");
PROCESS_THREAD(_led_process, ev, data)
{
    static struct etimer etime;
    int i;
    
    PROCESS_BEGIN();
    while (TRUE) {
        etimer_set(&etime, 100);
        PROCESS_WAIT_UNTIL(etimer_expired(&etime));
        GPIO_Toggle(GPIOA, GPIO_PINS_12);
        

        }
    
    PROCESS_END();
}


int Worker_Init()
{
    int status;
    modbus_param_init();
    modbus_init();
    status = Sensor_Start();
    i_assert(status == 0);
    process_start(&_led_process, NULL); 

    return 0;
}
