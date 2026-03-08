/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file    stepper_motor.c
  * @brief   This file provides code for the stepper motor driver
  *          using GPIO bit banging for step pulse generation
  ******************************************************************************
  * @attention
  *
  * Copyright (c) 2026 STMicroelectronics.
  * All rights reserved.
  *
  * This software is licensed under terms that can be found in the LICENSE file
  * in the root directory of this software component.
  * If no LICENSE file comes with this SOFTWARE, it is provided AS-IS.
  *
  ******************************************************************************
  */
/* USER CODE END Header */

/* Includes ------------------------------------------------------------------*/
#include "stepper_motor.h"
#include <stdio.h>
#include <string.h>

/* USER CODE BEGIN 0 */

static Stepper_ServiceCallback_t g_stepper_service_callback = NULL;

/**
  * @brief Delay in microseconds using busy wait
  * @param us Microseconds to delay
  * @retval None
  */
static inline void Delay_us(uint32_t us)
{
  /* At 84 MHz, 1 cycle = ~11.9ns, so 84 cycles = ~1us */
  /* Use approximately 84 cycles per microsecond */
  uint32_t cycles = us * 84 / 3;  /* Divide by 3 for loop overhead */
  for (volatile uint32_t i = 0; i < cycles; i++) {
    __NOP();
  }
}

/* USER CODE END 0 */

/**
  * @brief Initialize stepper motor driver
  * @param motor Pointer to stepper motor structure
  * @retval None
  */
void Stepper_Init(StepperMotor_t* motor)
{
  if (motor == NULL) {
    return;
  }

  /* Initialize motor structure */
  motor->step_count = 0;
  motor->is_moving = false;
  motor->direction = STEPPER_DIR_CLOCKWISE;
  motor->steps_per_second = 1000;  /* Default 1000 steps/sec */
  motor->current_position_microsteps = 0;

  /* Set initial pin states */
  HAL_GPIO_WritePin(motor->step_gpio_port, motor->step_gpio_pin, GPIO_PIN_RESET);
  HAL_GPIO_WritePin(motor->dir_gpio_port, motor->dir_gpio_pin, GPIO_PIN_RESET);
}

/**
  * @brief Execute a single step (pulse the step output via GPIO bit banging)
  * @param motor Pointer to stepper motor structure
  * @retval None
  */
void Stepper_Step(StepperMotor_t* motor)
{
  if (motor == NULL) {
    return;
  }

  /* Set direction pin based on current direction */
  HAL_GPIO_WritePin(motor->dir_gpio_port, motor->dir_gpio_pin,
                   motor->direction == STEPPER_DIR_CLOCKWISE ? GPIO_PIN_SET : GPIO_PIN_RESET);

  /* Toggle LED to indicate step */
  HAL_GPIO_TogglePin(LD2_GPIO_Port, LD2_Pin);

  /* Generate step pulse: HIGH for ~2us, then LOW */
  HAL_GPIO_WritePin(motor->step_gpio_port, motor->step_gpio_pin, GPIO_PIN_SET);
  Delay_us(2);
  HAL_GPIO_WritePin(motor->step_gpio_port, motor->step_gpio_pin, GPIO_PIN_RESET);

  /* Increment step counter */
  motor->step_count++;

  if (motor->direction == STEPPER_DIR_CLOCKWISE) {
    motor->current_position_microsteps++;
  } else {
    motor->current_position_microsteps--;
  }
}

/**
  * @brief Set stepper motor direction
  * @param motor Pointer to stepper motor structure
  * @param direction New direction value
  * @retval None
  */
void Stepper_SetDirection(StepperMotor_t* motor, StepperDirection_t direction)
{
  if (motor == NULL) {
    return;
  }

  motor->direction = direction;
  HAL_GPIO_WritePin(motor->dir_gpio_port, motor->dir_gpio_pin,
                   direction == STEPPER_DIR_CLOCKWISE ? GPIO_PIN_SET : GPIO_PIN_RESET);
}

/**
  * @brief Set motor speed (steps per second)
  * @param motor Pointer to stepper motor structure
  * @param steps_per_sec Speed in steps per second
  * @retval None
  */
void Stepper_SetSpeed(StepperMotor_t* motor, uint32_t steps_per_sec)
{
  if (motor == NULL || steps_per_sec < 1) {
    return;
  }

  motor->steps_per_second = steps_per_sec;
}

/**
  * @brief Move motor for specified number of steps
  * @param motor Pointer to stepper motor structure
  * @param steps Number of steps to execute
  * @retval None
  */
void Stepper_MoveSteps(StepperMotor_t* motor, uint32_t steps)
{
  if (motor == NULL || steps == 0 || motor->steps_per_second == 0) {
    return;
  }

  motor->is_moving = true;

  /* Calculate delay in microseconds between steps */
  uint32_t step_delay_us = 1000000 / motor->steps_per_second;

  for (uint32_t i = 0; i < steps; i++) {
    if (!motor->is_moving) {
      break;
    }

    Stepper_Step(motor);

    if (g_stepper_service_callback != NULL) {
      g_stepper_service_callback();
    }

    /* Delay between steps */
    if (step_delay_us >= 1000) {
      /* Use HAL_Delay for delays >= 1ms */
      uint32_t delay_ms = (step_delay_us + 500) / 1000;
      if (delay_ms > 0) {
        HAL_Delay(delay_ms);
      }
    } else {
      /* Use microsecond delay */
      Delay_us(step_delay_us);
    }
  }

  motor->is_moving = false;
}

/**
  * @brief Move motor to absolute microstep position using trapezoidal profile
  * @param motor Pointer to stepper motor structure
  * @param target_position_microsteps Absolute target position
  * @param max_steps_per_second Maximum speed in microsteps/s
  * @param accel_steps_per_second2 Accel/decel in microsteps/s^2
  * @retval true if executed, false otherwise
  */
bool Stepper_MoveToPositionMicrosteps(StepperMotor_t* motor,
                                      int32_t target_position_microsteps,
                                      uint32_t max_steps_per_second,
                                      uint32_t accel_steps_per_second2)
{
  if (motor == NULL || max_steps_per_second == 0 || accel_steps_per_second2 == 0) {
    return false;
  }

  int32_t delta = target_position_microsteps - motor->current_position_microsteps;
  if (delta == 0) {
    return true;
  }

  uint32_t total_steps = (delta >= 0) ? (uint32_t)delta : (uint32_t)(-delta);
  Stepper_SetDirection(motor, (delta >= 0) ? STEPPER_DIR_CLOCKWISE : STEPPER_DIR_COUNTERCLOCKWISE);

  motor->is_moving = true;

  float current_speed = 200.0f;
  float max_speed = (float)max_steps_per_second;
  float accel = (float)accel_steps_per_second2;

  if (current_speed > max_speed) {
    current_speed = max_speed;
  }

  for (uint32_t i = 0; i < total_steps; i++) {
    if (!motor->is_moving) {
      break;
    }

    uint32_t steps_remaining = total_steps - i;
    float steps_to_stop = (current_speed * current_speed) / (2.0f * accel);
    float dt = 1.0f / current_speed;

    if (steps_to_stop >= (float)steps_remaining) {
      current_speed -= accel * dt;
      if (current_speed < 200.0f) {
        current_speed = 200.0f;
      }
    } else {
      current_speed += accel * dt;
      if (current_speed > max_speed) {
        current_speed = max_speed;
      }
    }

    Stepper_Step(motor);

    if (g_stepper_service_callback != NULL) {
      g_stepper_service_callback();
    }

    uint32_t step_delay_us = (uint32_t)(1000000.0f / current_speed);
    if (step_delay_us < 50U) {
      step_delay_us = 50U;
    }
    Delay_us(step_delay_us);
  }

  motor->is_moving = false;
  return true;
}

/**
  * @brief Register callback invoked periodically during motion
  * @param callback Callback function pointer
  * @retval None
  */
void Stepper_SetServiceCallback(Stepper_ServiceCallback_t callback)
{
  g_stepper_service_callback = callback;
}

/**
  * @brief Stop motor movement
  * @param motor Pointer to stepper motor structure
  * @retval None
  */
void Stepper_Stop(StepperMotor_t* motor)
{
  if (motor == NULL) {
    return;
  }

  motor->is_moving = false;
}

/**
  * @brief Get current step count
  * @param motor Pointer to stepper motor structure
  * @retval Step count
  */
uint32_t Stepper_GetStepCount(StepperMotor_t* motor)
{
  if (motor == NULL) {
    return 0;
  }

  return motor->step_count;
}

/**
  * @brief Reset step counter
  * @param motor Pointer to stepper motor structure
  * @retval None
  */
void Stepper_ResetCounter(StepperMotor_t* motor)
{
  if (motor == NULL) {
    return;
  }

  motor->step_count = 0;
}

/**
  * @brief Get current signed motor position in microsteps
  * @param motor Pointer to stepper motor structure
  * @retval Signed microstep position
  */
int32_t Stepper_GetCurrentPositionMicrosteps(StepperMotor_t* motor)
{
  if (motor == NULL) {
    return 0;
  }

  return motor->current_position_microsteps;
}

/**
  * @brief Get motor status string
  * @param motor Pointer to stepper motor structure
  * @param buffer Pointer to output buffer
  * @param size Buffer size
  * @retval Number of characters written
  */
int Stepper_GetStatusString(StepperMotor_t* motor, char* buffer, size_t size)
{
  if (motor == NULL || buffer == NULL || size == 0) {
    return 0;
  }

  return snprintf(buffer, size,
    "=== STEPPER MOTOR STATUS ===\r\n"
    "Steps Executed: %lu\r\n"
    "Position (microsteps): %ld\r\n"
    "Direction: %s\r\n"
    "Moving: %s\r\n"
    "Speed: %lu steps/sec\r\n",
    motor->step_count,
    motor->current_position_microsteps,
    motor->direction == STEPPER_DIR_CLOCKWISE ? "CW" : "CCW",
    motor->is_moving ? "Yes" : "No",
    motor->steps_per_second);
}

/* USER CODE BEGIN 1 */

/* USER CODE END 1 */
