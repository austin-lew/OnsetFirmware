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

// Stepper motor struct
typedef struct{
  uint16_t            step_count;
  uint16_t            step_count_setpoint;
  stepper_dir_t       direction;
  TIM_HandleTypeDef*  timer;
  uint32_t            timer_channel;
} stepper_t;


bool stepper_absolute_move(stepper_t* stepper, uint16_t steps);

bool stepper_emergency_stop(stepper_t* stepper);

#ifdef __cplusplus
}
#endif

#endif /* __STEPPER_DRIVER_H__ */


