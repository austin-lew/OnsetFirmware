/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file    stepper_motor.h
  * @brief   This file contains all the function prototypes for
  *          the stepper_motor.c file
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

#ifndef __STEPPER_MOTOR_H__
#define __STEPPER_MOTOR_H__

#ifdef __cplusplus
extern "C" {
#endif

/* Includes ------------------------------------------------------------------*/
#include "main.h"
#include <stdint.h>
#include <stdbool.h>

/* USER CODE BEGIN Includes */

/* USER CODE END Includes */

/**
 * @brief Direction enumeration for stepper motor
 */
typedef enum {
  STEPPER_DIR_CLOCKWISE = 0,      /*!< Clockwise rotation */
  STEPPER_DIR_COUNTERCLOCKWISE = 1 /*!< Counter-clockwise rotation */
} StepperDirection_t;

/**
 * @brief Stepper motor control structure
 */
typedef struct {
  GPIO_TypeDef* step_gpio_port;    /*!< Step pin GPIO port (PB1) */
  uint16_t step_gpio_pin;          /*!< Step pin number */
  
  GPIO_TypeDef* dir_gpio_port;     /*!< Direction pin GPIO port (PB2) */
  uint16_t dir_gpio_pin;           /*!< Direction pin number */
  
  volatile uint32_t step_count;    /*!< Total steps executed */
  volatile bool is_moving;         /*!< Motor movement flag */
  volatile bool stop_requested;    /*!< Smooth stop request flag */
  StepperDirection_t direction;    /*!< Current direction */
  uint32_t steps_per_second;       /*!< Motor speed in steps/sec */
  volatile int32_t current_position_microsteps; /*!< Signed elbow position in microsteps */
} StepperMotor_t;

typedef void (*Stepper_ServiceCallback_t)(void);

/* USER CODE BEGIN ET */

/* USER CODE END ET */

/**
 * @brief Initialize stepper motor driver
 * @param motor Pointer to stepper motor structure
 * @retval HAL status
 */
void Stepper_Init(StepperMotor_t* motor);

/**
 * @brief Execute a single step
 * @param motor Pointer to stepper motor structure
 * @retval None
 */
void Stepper_Step(StepperMotor_t* motor);

/**
 * @brief Set stepper motor direction
 * @param motor Pointer to stepper motor structure
 * @param direction New direction value
 * @retval None
 */
void Stepper_SetDirection(StepperMotor_t* motor, StepperDirection_t direction);

/**
 * @brief Set motor speed
 * @param motor Pointer to stepper motor structure
 * @param steps_per_sec Speed in steps per second
 * @retval None
 */
void Stepper_SetSpeed(StepperMotor_t* motor, uint32_t steps_per_sec);

/**
 * @brief Move motor for specified number of steps
 * @param motor Pointer to stepper motor structure
 * @param steps Number of steps to execute
 * @retval None
 */
void Stepper_MoveSteps(StepperMotor_t* motor, uint32_t steps);

/**
 * @brief Move motor to absolute position in microsteps using trapezoidal profile
 * @param motor Pointer to stepper motor structure
 * @param target_position_microsteps Absolute target position
 * @param max_steps_per_second Maximum step rate in microsteps/s
 * @param accel_steps_per_second2 Acceleration/deceleration in microsteps/s^2
 * @retval true if command executed, false on invalid parameters
 */
bool Stepper_MoveToPositionMicrosteps(StepperMotor_t* motor,
                                      int32_t target_position_microsteps,
                                      uint32_t max_steps_per_second,
                                      uint32_t accel_steps_per_second2);

/**
 * @brief Register callback invoked periodically during motion
 * @param callback Callback function pointer (NULL to clear)
 * @retval None
 */
void Stepper_SetServiceCallback(Stepper_ServiceCallback_t callback);

/**
 * @brief Request smooth deceleration stop for profile-based motion
 * @param motor Pointer to stepper motor structure
 * @retval None
 */
void Stepper_RequestSmoothStop(StepperMotor_t* motor);

/**
 * @brief Stop motor movement
 * @param motor Pointer to stepper motor structure
 * @retval None
 */
void Stepper_Stop(StepperMotor_t* motor);

/**
 * @brief Get current step count
 * @param motor Pointer to stepper motor structure
 * @retval Step count
 */
uint32_t Stepper_GetStepCount(StepperMotor_t* motor);

/**
 * @brief Reset step counter
 * @param motor Pointer to stepper motor structure
 * @retval None
 */
void Stepper_ResetCounter(StepperMotor_t* motor);

/**
 * @brief Get current signed motor position in microsteps
 * @param motor Pointer to stepper motor structure
 * @retval Signed microstep position
 */
int32_t Stepper_GetCurrentPositionMicrosteps(StepperMotor_t* motor);

/**
 * @brief Get motor status string
 * @param motor Pointer to stepper motor structure
 * @param buffer Pointer to output buffer
 * @param size Buffer size
 * @retval Number of characters written
 */
int Stepper_GetStatusString(StepperMotor_t* motor, char* buffer, size_t size);

/* USER CODE BEGIN PFP */

/* USER CODE END PFP */

#ifdef __cplusplus
}
#endif

#endif /* __STEPPER_MOTOR_H__ */
