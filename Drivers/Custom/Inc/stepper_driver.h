/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file    stepper_driver.h
  * @note    Timer based stepper motor control
  * @brief   This file contains all the function prototypes for
  *          the stepper_driver.c file.
  ******************************************************************************
  */
/* USER CODE END Header */
/* Define to prevent recursive inclusion -------------------------------------*/
#ifndef __STEPPER_DRIVER_H__
#define __STEPPER_DRIVER_H__

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>
#include <stdbool.h>
#include "stm32g4xx_hal.h"

// Enum for clockwise and counterclockwise direction
typedef enum{
  DIR_CW = 0,
  DIR_CCW = 1
} stepper_dir_t;


void stepper_init(TIM_HandleTypeDef* timer,
                  uint32_t timer_channel,
                  uint32_t max_steps_per_second,
                  uint32_t max_steps_per_second2,
                  GPIO_TypeDef* dir_gpio_port,
                  uint16_t dir_gpio_pin);

void stepper_home();

void stepper_tare();

bool stepper_absolute_move(uint16_t steps);

bool stepper_emergency_stop(void);

#ifdef __cplusplus
}
#endif

#endif /* __STEPPER_DRIVER_H__ */


