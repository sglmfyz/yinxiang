/**
  **************************************************************************
  * @file     at32f421_int.c
  * @version  v2.0.5
  * @date     2022-04-02
  * @brief    main interrupt service routines.
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

/* includes ------------------------------------------------------------------*/
#include "at32f421_it.h"
#include "util.h"
#include "hw.h"
#include "gpio.h"
/** @addtogroup AT32F421_periph_template
  * @{
  */

/** @addtogroup 421_LED_toggle
  * @{
  */

/**
  * @brief  this function handles nmi exception.
  * @param  none
  * @retval none
  */
void NMI_Handler(void)
{
}

/**
  * @brief  this function handles hard fault exception.
  * @param  none
  * @retval none
  */
void HardFault_Handler(void)
{
  /* go to infinite loop when hard fault exception occurs */
  while(1)
  {
  }
}

/**
  * @brief  this function handles memory manage exception.
  * @param  none
  * @retval none
  */
void MemManage_Handler(void)
{
  /* go to infinite loop when memory manage exception occurs */
  while(1)
  {
  }
}

/**
  * @brief  this function handles bus fault exception.
  * @param  none
  * @retval none
  */
void BusFault_Handler(void)
{
  /* go to infinite loop when bus fault exception occurs */
  while(1)
  {
  }
}

/**
  * @brief  this function handles usage fault exception.
  * @param  none
  * @retval none
  */
void UsageFault_Handler(void)
{
  /* go to infinite loop when usage fault exception occurs */
  while(1)
  {
  }
}

/**
  * @brief  this function handles svcall exception.
  * @param  none
  * @retval none
  */
void SVC_Handler(void)
{
}

/**
  * @brief  this function handles debug monitor exception.
  * @param  none
  * @retval none
  */
void DebugMon_Handler(void)
{
}

/**
  * @brief  this function handles pendsv_handler exception.
  * @param  none
  * @retval none
  */
void PendSV_Handler(void)
{
}

/**
  * @brief  this function handles systick handler.
  * @param  none
  * @retval none
  */
void SysTick_Handler(void)
{
    Sys_IncTick();
}


void USART1_IRQHandler(void)
{
    
    uart1->HW_Recv_Byte(uart1);
}

void USART2_IRQHandler(void)
{
    uart2->HW_Recv_Byte(uart2);
}

void TMR16_GLOBAL_IRQHandler(void)
{
    if (tmr_flag_get(TMR16, TMR_OVF_FLAG)) {
        tmr_flag_clear(TMR16, TMR_OVF_FLAG);
        if (uart3) {
            uart3->SW_Timer_ISR(uart3);
        }
    }
}

void EXINT15_4_IRQHandler(void)
{
    if (exint_flag_get(EXINT_LINE_7) && GPIO_Read(GPIOF, GPIO_PINS_7) == RESET) {
        exint_flag_clear(EXINT_LINE_7);
        if (uart3) {
            uart3->SW_Rx_Exti_ISR(uart3);
        }
    }
}

/**
  * @}
  */

/**
  * @}
  */
