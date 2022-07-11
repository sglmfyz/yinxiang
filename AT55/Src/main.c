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
#include "update.h"
#include "modbus_regs.h"


    u16 FLASH_ReadHalfWord(u32 faddr)
    {
        return *(vu16*)faddr; 
        
    }
    
    void FLASH_Read(u32 ReadAddr, u16 *pBuffer, u16 NumToRead)      
    {
        u16 i;
        
        for(i=0;i<NumToRead;i++)
        {
            pBuffer[i]=FLASH_ReadHalfWord(ReadAddr);//读取2个字节.
            ReadAddr+=2;//偏移2个字节. 
        }
    }
    int rewriteflash4x(void)
        {
        int status;
        crm_clock_source_enable(CRM_CLOCK_SOURCE_HICK, TRUE);
         flash_unlock();
         Delay_ms(80);
         status=flash_sector_erase(FLASH_MODBUS_4X_BASE);
#if 1         
         uint32_t a=1,b=2;
         for(int i=0;i<UREG4X_REG_END*2;i+=4)
            {
             uint32_t temp=(b<<16)+a;
             a+=20;b+=20;
           status=flash_word_program(FLASH_MODBUS_4X_BASE+i, temp);
            if (status != FLASH_OPERATE_DONE)
             {
            uint32_t  addr=FLASH_MODBUS_4X_BASE+i;
            syslog_error("write error, addr %08x, status %d \n", addr, status);
            break;
             }   
            }
#endif         
         flash_lock();
      
         return 0;
        }



    int main(void)
{  

   
    FLASH_Read(BOARD_INFO_ADDR, (u16 *)board_info, sizeof(board_info_t)/sizeof(u16));//相当于值已经读到指针里了board_info地址
    

    int status;
    
    
    system_clock_config();
    
    process_init();
    process_start(&etimer_process, NULL);
    
    Board_Init();
    HW_Init();

    status = Log_SW_Init(GPIOA, GPIO_PINS_15, 9600);
    i_assert(status == 0);
    status =rewriteflash4x();
    i_assert(status == 0);
    status = Worker_Init();
    i_assert(status == 0);

    while(1) {
    process_run();
    etimer_request_poll();

  }
}



