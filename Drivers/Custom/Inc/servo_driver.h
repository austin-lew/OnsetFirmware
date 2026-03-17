/* USER CODE BEGIN Header */
/**
 ******************************************************************************
 * @file    servo_driver.h
 * @note    Timer PWM based servo control
 * @brief   This file contains all the function prototypes for
 *          the servo_driver.c file.
 ******************************************************************************
 */
/* USER CODE END Header */
/* Define to prevent recursive inclusion -------------------------------------*/
#ifndef __SERVO_DRIVER_H__
#define __SERVO_DRIVER_H__

#ifdef __cplusplus
extern "C"
{
#endif

#include <stdbool.h>
#include <stdint.h>
#include "stm32g4xx_hal.h"

  void servo_init(TIM_HandleTypeDef *timer, uint32_t timer_channel);

  bool servo_set_angle_deg(float angle_deg);

#ifdef __cplusplus
}
#endif

#endif /* __SERVO_DRIVER_H__ */
