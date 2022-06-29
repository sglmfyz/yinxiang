/**
  **************************************************************************
  * @file     main.c
  * @version  v2.0.5
  * @date     2022-04-02
  * @brief    main program
  **************************************************************************
  *                       Copyright notice & Disclaimer
  *
  * The software Board Support Package (BSP) that is made available to
  * download from Artery official website is the copyrighted work of Artery.
  * Artery authorizes customers to use, copy, and distribute the BSP
  * software and its related documentation for the purpose of design and
  * development in conjunction with Artery microcontrollers. Use of the
  * software is governed by this copyright notice and the following disclaimer.
  *
  * THIS SOFTWARE IS PROVIDED ON "AS IS" BASIS WITHOUT WARRANTIES,
  * GUARANTEES OR REPRESENTATIONS OF ANY KIND. ARTERY EXPRESSLY DISCLAIMS,
  * TO THE FULLEST EXTENT PERMITTED BY LAW, ALL EXPRESS, IMPLIED OR
  * STATUTORY OR OTHER WARRANTIES, GUARANTEES OR REPRESENTATIONS,
  * INCLUDING BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY,
  * FITNESS FOR A PARTICULAR PURPOSE, OR NON-INFRINGEMENT.
  *
  **************************************************************************
  */

#include "at32f421_board.h"
#include "at32f421_clock.h"

#include "common.h"
#include "process.h"
#include "etimer.h"
#include "gpio.h"
#include "log.h"
#include "worker.h"
#include "hw.h"

    int main(void)
{
    int status;
    
    system_clock_config();
    
    process_init();
    process_start(&etimer_process, NULL);
    
    Board_Init();
    HW_Init();

    status = Log_SW_Init(GPIOA, GPIO_PINS_15, 9600);
    i_assert(status == 0);

    status = Worker_Init();
    i_assert(status == 0);
    
    while(1) {
        process_run();
        etimer_request_poll();

  }
}

