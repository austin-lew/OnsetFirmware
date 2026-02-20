/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file           : main.h
  * @brief          : Header for main.c file.
  *                   This file contains the common defines of the application.
  ******************************************************************************
  * @attention
  *
  * Copyright (c) 2026 STMicroelectronics.
  * All rights reserved.
  *
  * This software is licensed under terms that can be found in the LICENSE file
  * in the root directory of this software component.
  * If no LICENSE file comes with this software, it is provided AS-IS.
  *
  ******************************************************************************
  */
/* USER CODE END Header */

/* Define to prevent recursive inclusion -------------------------------------*/
#ifndef __MAIN_H
#define __MAIN_H

#ifdef __cplusplus
extern "C" {
#endif

/* Includes ------------------------------------------------------------------*/
#include "stm32g4xx_hal.h"

/* Private includes ----------------------------------------------------------*/
/* USER CODE BEGIN Includes */

/* USER CODE END Includes */

/* Exported types ------------------------------------------------------------*/
/* USER CODE BEGIN ET */

/* USER CODE END ET */

/* Exported constants --------------------------------------------------------*/
/* USER CODE BEGIN EC */

/* USER CODE END EC */

/* Exported macro ------------------------------------------------------------*/
/* USER CODE BEGIN EM */

/* USER CODE END EM */

/* Exported functions prototypes ---------------------------------------------*/
void Error_Handler(void);

/* USER CODE BEGIN EFP */

/* USER CODE END EFP */

/* Private defines -----------------------------------------------------------*/
#define LIMIT_SW_1_Pin GPIO_PIN_0
#define LIMIT_SW_1_GPIO_Port GPIOA
#define LIMIT_SW_1_EXTI_IRQn EXTI0_IRQn
#define LIMIT_SW_2_Pin GPIO_PIN_1
#define LIMIT_SW_2_GPIO_Port GPIOA
#define LIMIT_SW_2_EXTI_IRQn EXTI1_IRQn
#define LIMIT_SW_3_Pin GPIO_PIN_2
#define LIMIT_SW_3_GPIO_Port GPIOA
#define LIMIT_SW_3_EXTI_IRQn EXTI2_IRQn
#define LIMIT_SW_4_Pin GPIO_PIN_3
#define LIMIT_SW_4_GPIO_Port GPIOA
#define LIMIT_SW_4_EXTI_IRQn EXTI3_IRQn
#define BATT_MAIN_EN_L_Pin GPIO_PIN_4
#define BATT_MAIN_EN_L_GPIO_Port GPIOA
#define PRECHARG_EN_Pin GPIO_PIN_5
#define PRECHARG_EN_GPIO_Port GPIOA
#define ELBOW_STEP_Pin GPIO_PIN_6
#define ELBOW_STEP_GPIO_Port GPIOA
#define ELBOW_DIR_Pin GPIO_PIN_7
#define ELBOW_DIR_GPIO_Port GPIOA
#define PPBATT_VSENS_Pin GPIO_PIN_0
#define PPBATT_VSENS_GPIO_Port GPIOB
#define HEARTBEAT_LED_Pin GPIO_PIN_2
#define HEARTBEAT_LED_GPIO_Port GPIOB
#define LED_RGB_Pin GPIO_PIN_10
#define LED_RGB_GPIO_Port GPIOB
#define EFUSE_WAKE_Pin GPIO_PIN_15
#define EFUSE_WAKE_GPIO_Port GPIOB
#define ENC_1A_Pin GPIO_PIN_8
#define ENC_1A_GPIO_Port GPIOA
#define ENC_1B_Pin GPIO_PIN_9
#define ENC_1B_GPIO_Port GPIOA
#define EFUSE_FLT_L_Pin GPIO_PIN_10
#define EFUSE_FLT_L_GPIO_Port GPIOA
#define USB_D_N_Pin GPIO_PIN_11
#define USB_D_N_GPIO_Port GPIOA
#define USB_D_P_Pin GPIO_PIN_12
#define USB_D_P_GPIO_Port GPIOA
#define DEBUG_SWDIO_Pin GPIO_PIN_13
#define DEBUG_SWDIO_GPIO_Port GPIOA
#define DEBUG_SWCLK_Pin GPIO_PIN_14
#define DEBUG_SWCLK_GPIO_Port GPIOA
#define SERVO_PWM_Pin GPIO_PIN_15
#define SERVO_PWM_GPIO_Port GPIOA
#define DEBUG_SWO_Pin GPIO_PIN_3
#define DEBUG_SWO_GPIO_Port GPIOB
#define ENC_2A_Pin GPIO_PIN_6
#define ENC_2A_GPIO_Port GPIOB
#define ENC_2B_Pin GPIO_PIN_7
#define ENC_2B_GPIO_Port GPIOB
#define LED_EN_Pin GPIO_PIN_9
#define LED_EN_GPIO_Port GPIOB

/* USER CODE BEGIN Private defines */

/* USER CODE END Private defines */

#ifdef __cplusplus
}
#endif

#endif /* __MAIN_H */
