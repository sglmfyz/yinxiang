/**
  **************************************************************************
  * @file     at32f421_board.c
  * @version  v2.0.5
  * @date     2022-04-02
  * @brief    set of firmware functions to manage leds and push-button.
  *           initialize delay function.
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
#include "common.h"
#include "util.h"
#include "timer.h"
#include "modbus_regs.h"
/** @addtogroup AT32F421_board
  * @{
  */


static void uart_init()
{
    gpio_init_type gpio_init_struct;

    /* enable the uart and gpio clock */
    crm_periph_clock_enable(CRM_USART1_PERIPH_CLOCK, TRUE);
    crm_periph_clock_enable(CRM_USART2_PERIPH_CLOCK, TRUE);

    crm_periph_clock_enable(CRM_GPIOA_PERIPH_CLOCK, TRUE);
    
    gpio_default_para_init(&gpio_init_struct);

    /* configure the uart tx pin */
    gpio_init_struct.gpio_drive_strength = GPIO_DRIVE_STRENGTH_STRONGER;
    gpio_init_struct.gpio_out_type  = GPIO_OUTPUT_PUSH_PULL;
    gpio_init_struct.gpio_mode = GPIO_MODE_MUX;
    gpio_init_struct.gpio_pins =  GPIO_PINS_2 | GPIO_PINS_3 | GPIO_PINS_9 | GPIO_PINS_10;
    gpio_init_struct.gpio_pull = GPIO_PULL_NONE;
    gpio_init(GPIOA, &gpio_init_struct);

    /* uart1 */
    gpio_pin_mux_config(GPIOA, GPIO_PINS_SOURCE9, GPIO_MUX_1);
    gpio_pin_mux_config(GPIOA, GPIO_PINS_SOURCE10, GPIO_MUX_1);
    /* configure uart param */
    usart_init(USART1, 115200, USART_DATA_8BITS, USART_STOP_1_BIT);
    usart_transmitter_enable(USART1, TRUE);
    usart_interrupt_enable(USART1, USART_IDLE_INT, TRUE);
    usart_interrupt_enable(USART1, USART_RDBF_INT, TRUE);
    usart_receiver_enable(USART1, TRUE);
    usart_enable(USART1, TRUE);
    nvic_irq_enable(USART1_IRQn, 3, 0);
    /* uart2 */
    gpio_pin_mux_config(GPIOA, GPIO_PINS_SOURCE2, GPIO_MUX_1);
    gpio_pin_mux_config(GPIOA, GPIO_PINS_SOURCE3, GPIO_MUX_1);
    /* configure uart param */
    usart_init(USART2, 115200, USART_DATA_8BITS, USART_STOP_1_BIT);
    usart_transmitter_enable(USART2, TRUE);
    usart_interrupt_enable(USART2, USART_IDLE_INT, TRUE);
    usart_interrupt_enable(USART2, USART_RDBF_INT, TRUE);
    usart_receiver_enable(USART2, TRUE);
    usart_enable(USART2, TRUE);
    nvic_irq_enable(USART2_IRQn, 3, 0);
}

  static void ADC1_CH_DMA_Config(void)
{
    adc_base_config_type       ADC_InitStructure;
    dma_init_type       DMA_InitStructure;
    gpio_init_type      GPIO_InitStructure;
    adc_type *adc = ADC1;

    /* Enable ADC1, DMA2 and GPIO clocks ****************************************/
    crm_periph_clock_enable(CRM_ADC1_PERIPH_CLOCK,TRUE);   
    crm_periph_clock_enable(CRM_DMA1_PERIPH_CLOCK,TRUE);   //dma1线和uart1线都在apb2总线上，4分频过后波特率要2分频
    ADC_Buffer_Init(adc, 50, 5);//avg和channel是用来调什么的？除了一以外每个减一
    GPIO_InitStructure.gpio_pins =  GPIO_PINS_1|GPIO_PINS_4|GPIO_PINS_5|GPIO_PINS_6|GPIO_PINS_7 ;  //5个voc；
    GPIO_InitStructure.gpio_mode = GPIO_MODE_INPUT;
	GPIO_InitStructure.gpio_out_type=GPIO_OUTPUT_PUSH_PULL;
    GPIO_InitStructure.gpio_drive_strength = GPIO_DRIVE_STRENGTH_STRONGER;
    gpio_init(GPIOA, &GPIO_InitStructure);
    dma_reset(DMA1_CHANNEL1);
    dma_default_para_init(&DMA_InitStructure);
    
    DMA_InitStructure.peripheral_base_addr=(uint32_t)&adc->odt;
    DMA_InitStructure.memory_base_addr=(uint32_t)ADC_Get_Buffer(adc);
    DMA_InitStructure.direction=DMA_DIR_PERIPHERAL_TO_MEMORY;
	DMA_InitStructure.buffer_size=50*5;
    DMA_InitStructure.memory_inc_enable=DMA_MEMORY_INC_ENABLE;
	DMA_InitStructure.peripheral_inc_enable=DMA_PERIPHERAL_INC_DISABLE;
    DMA_InitStructure.peripheral_data_width=DMA_PERIPHERAL_DATA_WIDTH_HALFWORD;
	DMA_InitStructure.memory_data_width=DMA_MEMORY_DATA_WIDTH_HALFWORD;
    DMA_InitStructure.loop_mode_enable=TRUE;
    DMA_InitStructure.priority=DMA_PRIORITY_HIGH;
    dma_init(DMA1_CHANNEL1, &DMA_InitStructure);


    /* Enable DMA1 channel1 */
    dma_channel_enable(DMA1_CHANNEL1, TRUE);
	adc_base_default_para_init(&ADC_InitStructure);
    ADC_InitStructure.sequence_mode=TRUE;
    ADC_InitStructure.repeat_mode =TRUE;
    ADC_InitStructure.data_align=   ADC_RIGHT_ALIGNMENT;
    ADC_InitStructure.ordinary_channel_length=5;    //这是转换长度？之前的参数是选择通道?
    adc_base_config(adc, &ADC_InitStructure);
    adc_ordinary_channel_set(adc,ADC_CHANNEL_1, 1,  ADC_SAMPLETIME_55_5);
    adc_ordinary_channel_set(adc,ADC_CHANNEL_4, 2,  ADC_SAMPLETIME_55_5);
    adc_ordinary_channel_set(adc,ADC_CHANNEL_5, 3,  ADC_SAMPLETIME_55_5);
    adc_ordinary_channel_set(adc,ADC_CHANNEL_6, 4,  ADC_SAMPLETIME_55_5);
    adc_ordinary_channel_set(adc,ADC_CHANNEL_7, 5,  ADC_SAMPLETIME_55_5);
    adc_ordinary_conversion_trigger_set(ADC1, ADC12_ORDINARY_TRIG_SOFTWARE, TRUE);
    adc_dma_mode_enable(adc, TRUE);
    adc_enable(adc, TRUE);
	adc_calibration_init(adc);
	while(adc_calibration_init_status_get(adc));
	adc_calibration_start(adc);
	while(adc_calibration_status_get(adc));
    adc_ordinary_software_trigger_enable(ADC1, TRUE);

}

static void board_gpio_init() 
{
    gpio_init_type gpio_init_struct;
     
    crm_periph_clock_enable(CRM_GPIOA_PERIPH_CLOCK, TRUE);
    crm_periph_clock_enable(CRM_GPIOB_PERIPH_CLOCK, TRUE);
    crm_periph_clock_enable(CRM_GPIOF_PERIPH_CLOCK, TRUE);

    gpio_init_struct.gpio_drive_strength = GPIO_DRIVE_STRENGTH_STRONGER;
    gpio_init_struct.gpio_out_type  = GPIO_OUTPUT_PUSH_PULL;
    gpio_init_struct.gpio_mode = GPIO_MODE_OUTPUT;
    gpio_init_struct.gpio_pins = GPIO_PINS_0 | GPIO_PINS_2 | GPIO_PINS_8 | GPIO_PINS_9 | GPIO_PINS_11 
            | GPIO_PINS_12 | GPIO_PINS_15;
    gpio_init_struct.gpio_pull = GPIO_PULL_NONE;

    gpio_bits_set(GPIOA, GPIO_PINS_15); // Debug uart
    gpio_bits_set(GPIOA, GPIO_PINS_8); // uart3 tx
    gpio_init(GPIOA, &gpio_init_struct);

    gpio_init_struct.gpio_drive_strength = GPIO_DRIVE_STRENGTH_STRONGER;
    gpio_init_struct.gpio_out_type  = GPIO_OUTPUT_OPEN_DRAIN;
    gpio_init_struct.gpio_mode = GPIO_MODE_OUTPUT;
    gpio_init_struct.gpio_pins = GPIO_PINS_0 | GPIO_PINS_1 | GPIO_PINS_2 | GPIO_PINS_3 | GPIO_PINS_4 | GPIO_PINS_5 | GPIO_PINS_6 
            | GPIO_PINS_7 | GPIO_PINS_8 | GPIO_PINS_9 | GPIO_PINS_10 | GPIO_PINS_11 | GPIO_PINS_12 | GPIO_PINS_13 | GPIO_PINS_14 | GPIO_PINS_15;
    gpio_init_struct.gpio_pull = GPIO_PULL_NONE;
    gpio_init(GPIOB, &gpio_init_struct);

    gpio_init_struct.gpio_drive_strength = GPIO_DRIVE_STRENGTH_STRONGER;
    gpio_init_struct.gpio_out_type  = GPIO_OUTPUT_PUSH_PULL;
    gpio_init_struct.gpio_mode = GPIO_MODE_OUTPUT;
    gpio_init_struct.gpio_pins = GPIO_PINS_6;
    gpio_init_struct.gpio_pull = GPIO_PULL_NONE;
    gpio_init(GPIOF, &gpio_init_struct);

    gpio_init_struct.gpio_drive_strength = GPIO_DRIVE_STRENGTH_STRONGER;
    gpio_init_struct.gpio_out_type  = GPIO_OUTPUT_OPEN_DRAIN;
    gpio_init_struct.gpio_mode = GPIO_MODE_INPUT;
    gpio_init_struct.gpio_pins = GPIO_PINS_7;
    gpio_init_struct.gpio_pull = GPIO_PULL_UP;
    gpio_init(GPIOF, &gpio_init_struct);

}



static void delay_init(tmr_type *tim, crm_periph_clock_type clock)
{
    uint32_t freq;

    crm_periph_clock_enable(clock, TRUE);
    freq = get_tim_freq(tim);
    tmr_base_init(tim, 65535, freq/1000000 - 1); // 1us，计数器增加1
    tmr_cnt_dir_set(tim, TMR_COUNT_UP);
    tmr_counter_enable(tim, TRUE);

    Delay_SetTMR(tim);

    /* configure systick */
    systick_clock_source_config(SYSTICK_CLOCK_SOURCE_AHBCLK_NODIV);
    SysTick_Config(system_core_clock / 1000U);

    nvic_irq_enable(SysTick_IRQn, 0, 0);
}

void sw_uart_init(tmr_type *tim, crm_periph_clock_type clock)
{
    exint_init_type exint_struct;

    crm_periph_clock_enable(clock, TRUE);

    tmr_base_init(tim, 0, 0);
    Disable_TIMER_Int(tim);
    nvic_irq_enable(TMR16_GLOBAL_IRQn, 3, 0);

    crm_periph_clock_enable(CRM_SCFG_PERIPH_CLOCK, TRUE);
    crm_periph_clock_enable(CRM_GPIOF_PERIPH_CLOCK, TRUE);

    scfg_exint_line_config(SCFG_PORT_SOURCE_GPIOF, SCFG_PINS_SOURCE7);

    exint_struct.line_mode = EXINT_LINE_INTERRUPUT;
    exint_struct.line_select = EXINT_LINE_7;
    exint_struct.line_polarity = EXINT_TRIGGER_FALLING_EDGE;
    exint_struct.line_enable = TRUE;
    exint_init(&exint_struct);

    nvic_irq_enable(EXINT15_4_IRQn, 3, 0);
}


void Board_Init()
{
    delay_init(TMR17, CRM_TMR17_PERIPH_CLOCK);
    
    board_gpio_init();
    uart_init();
    ADC1_CH_DMA_Config();
    sw_uart_init(TMR16, CRM_TMR16_PERIPH_CLOCK);
    crm_periph_clock_enable(CRM_I2C2_PERIPH_CLOCK,  TRUE);//???是没开启时钟的问题？
}





