/* USER CODE BEGIN Header */
/**
 ******************************************************************************
 * @file    led_driver.h
 * @brief   This file contains all the function prototypes for
 *          the led_driver.c file
 ******************************************************************************
 */
/* USER CODE END Header */
/* Define to prevent recursive inclusion -------------------------------------*/
#ifndef __LED_DRIVER_H__
#define __LED_DRIVER_H__

#ifdef __cplusplus
extern "C"
{
#endif

#include "stm32g4xx_hal.h"

void led_disable(void);
void led_enable(void);

#ifdef __cplusplus
}
#endif

#endif /* __LED_DRIVER_H__ */
