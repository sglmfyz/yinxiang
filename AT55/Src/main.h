/**
  ******************************************************************************
  * @file           : main.h
  * @brief          : Header for main.c file.
  *                   This file contains the common defines of the application.
  ******************************************************************************
  ** This notice applies to any and all portions of this file
  * that are not between comment pairs USER CODE BEGIN and
  * USER CODE END. Other portions of this file, whether 
  * inserted by the user or by software development tools
  * are owned by their respective copyright owners.
  *
  * COPYRIGHT(c) 2021 STMicroelectronics
  *
  * Redistribution and use in source and binary forms, with or without modification,
  * are permitted provided that the following conditions are met:
  *   1. Redistributions of source code must retain the above copyright notice,
  *      this list of conditions and the following disclaimer.
  *   2. Redistributions in binary form must reproduce the above copyright notice,
  *      this list of conditions and the following disclaimer in the documentation
  *      and/or other materials provided with the distribution.
  *   3. Neither the name of STMicroelectronics nor the names of its contributors
  *      may be used to endorse or promote products derived from this software
  *      without specific prior written permission.
  *
  * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
  * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
  * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
  * DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE
  * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
  * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
  * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
  * CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
  * OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
  * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
  *
  ******************************************************************************
  */

/* Define to prevent recursive inclusion -------------------------------------*/
#ifndef __MAIN_H__
#define __MAIN_H__

/* Includes ------------------------------------------------------------------*/

/* USER CODE BEGIN Includes */

/* USER CODE END Includes */

/* Private define ------------------------------------------------------------*/

#define LAMP_CTL_Pin GPIO_PINS_2
#define LAMP_CTL_GPIO_Port GPIOE
#define BEEP_CTL_Pin GPIO_PINS_3
#define BEEP_CTL_GPIO_Port GPIOE
#define RTC_SDA_Pin GPIO_PINS_4
#define RTC_SDA_GPIO_Port GPIOE
#define RTC_INT_Pin GPIO_PINS_5
#define RTC_INT_GPIO_Port GPIOE
#define RTC_SCL_Pin GPIO_PINS_6
#define RTC_SCL_GPIO_Port GPIOE
#define FAN_ENABLE_Pin GPIO_PINS_4
#define FAN_ENABLE_GPIO_Port GPIOA
#define FAN_FB_Pin GPIO_PINS_7
#define FAN_FB_GPIO_Port GPIOA
#define LOCK4_CTL_Pin GPIO_PINS_4  // 手动修改
#define LOCK4_CTL_GPIO_Port GPIOC // 手动修改
#define LOCK3_CTL_Pin GPIO_PINS_5  // 手动修改
#define LOCK3_CTL_GPIO_Port GPIOC  // 手动修改
#define RUART_TX_Pin GPIO_PINS_8
#define RUART_TX_GPIO_Port GPIOE
#define RUART_DE_REN_Pin GPIO_PINS_9
#define RUART_DE_REN_GPIO_Port GPIOE
#define RUART_RX_Pin GPIO_PINS_10
#define RUART_RX_GPIO_Port GPIOE    
#define EPORT_NR_Pin GPIO_PINS_11
#define EPORT_NR_GPIO_Port GPIOE
#define EPORT_NRST_Pin GPIO_PINS_12
#define EPORT_NRST_GPIO_Port GPIOE
#define DOOR0_Pin GPIO_PINS_13
#define DOOR0_GPIO_Port GPIOE
#define DOOR1_Pin GPIO_PINS_14
#define DOOR1_GPIO_Port GPIOE
#define COMM6_DE_Ren_Pin GPIO_PINS_15
#define COMM6_DE_Ren_GPIO_Port GPIOE
#define WIFI_NRST_Pin GPIO_PINS_12
#define WIFI_NRST_GPIO_Port GPIOB
#define WIFI_NR_Pin GPIO_PINS_13
#define WIFI_NR_GPIO_Port GPIOB
#define WIFI_RDY_Pin GPIO_PINS_14
#define WIFI_RDY_GPIO_Port GPIOB
#define COMM3_TX_Pin GPIO_PINS_8
#define COMM3_TX_GPIO_Port GPIOD
#define COMM3_RX_Pin GPIO_PINS_9
#define COMM3_RX_GPIO_Port GPIOD
#define COMM3_DE_Ren_Pin GPIO_PINS_10
#define COMM3_DE_Ren_GPIO_Port GPIOD
#define I2C2_SDA_Pin GPIO_PINS_11
#define I2C2_SDA_GPIO_Port GPIOD
#define CAT1_PWR_Pin GPIO_PINS_12
#define CAT1_PWR_GPIO_Port GPIOD
#define CAT1_RST_Pin GPIO_PINS_13
#define CAT1_RST_GPIO_Port GPIOD
#define I2C2_SCL_Pin GPIO_PINS_14
#define I2C2_SCL_GPIO_Port GPIOD
#define COMM6_TX_Pin GPIO_PINS_6
#define COMM6_TX_GPIO_Port GPIOC
#define COMM6_RX_Pin GPIO_PINS_7
#define COMM6_RX_GPIO_Port GPIOC
#define I2C3_SDA_Pin GPIO_PINS_9
#define I2C3_SDA_GPIO_Port GPIOC
#define I2C3_SCL_Pin GPIO_PINS_8
#define I2C3_SCL_GPIO_Port GPIOA
#define LCD_TX_Pin GPIO_PINS_9
#define LCD_TX_GPIO_Port GPIOA
#define DEBUG_TX_Pin GPIO_PINS_10
#define DEBUG_TX_GPIO_Port GPIOA
#define DEBUG_RX_Pin GPIO_PINS_11
#define DEBUG_RX_GPIO_Port GPIOA
#define DEBUG_RX_EXTI_IRQn EXTI15_10_IRQn
#define SPI1_CS_N_Pin GPIO_PINS_15
#define SPI1_CS_N_GPIO_Port GPIOA
#define COMM5_TX_Pin GPIO_PINS_12
#define COMM5_TX_GPIO_Port GPIOC
#define PCB_VER0_Pin GPIO_PINS_0
#define PCB_VER0_GPIO_Port GPIOD
#define PCB_VER1_Pin GPIO_PINS_1
#define PCB_VER1_GPIO_Port GPIOD
#define COMM5_RX_Pin GPIO_PINS_2
#define COMM5_RX_GPIO_Port GPIOD
#define STATUS_LED_Pin GPIO_PINS_3
#define STATUS_LED_GPIO_Port GPIOD
#define CAT1_TX_Pin GPIO_PINS_5
#define CAT1_TX_GPIO_Port GPIOD
#define CAT1_RX_Pin GPIO_PINS_6
#define CAT1_RX_GPIO_Port GPIOD
#define CAT1_STATUS_Pin GPIO_PINS_7
#define CAT1_STATUS_GPIO_Port GPIOD
#define I2C1_SCL_Pin GPIO_PINS_6
#define I2C1_SCL_GPIO_Port GPIOB
#define LCD_RX_Pin GPIO_PINS_7
#define LCD_RX_GPIO_Port GPIOB
#define I2C1_SDA_Pin GPIO_PINS_9
#define I2C1_SDA_GPIO_Port GPIOB
int rewriteflash4x(void);

/* ########################## Assert Selection ############################## */
/**
  * @brief Uncomment the line below to expanse the "assert_param" macro in the 
  *        HAL drivers code
  */
/* #define USE_FULL_ASSERT    1U */

/* USER CODE BEGIN Private defines */

/* USER CODE END Private defines */

#ifdef __cplusplus
 extern "C" {
#endif
void _Error_Handler(char *, int);

#define Error_Handler() _Error_Handler(__FILE__, __LINE__)
#ifdef __cplusplus
}
#endif

#endif /* __MAIN_H__ */

/************************ (C) COPYRIGHT STMicroelectronics *****END OF FILE****/
