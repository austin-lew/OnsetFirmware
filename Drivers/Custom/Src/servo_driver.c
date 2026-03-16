/**
 ******************************************************************************
 * @file           : servo_driver.c
 * @brief          : Implements the functions for controlling a PWM servo
 ******************************************************************************
 */

#include "servo_driver.h"

typedef struct
{
  TIM_HandleTypeDef *timer;
  uint32_t timer_channel;
  bool initialized;
} servo_config_t;

static servo_config_t servo1;

#define SERVO_MIN_DUTY (0.025f)
#define SERVO_MAX_DUTY (0.125f)
#define SERVO_MIN_ANGLE_DEG (0.0f)
#define SERVO_MAX_ANGLE_DEG (180.0f)

static float clampf(float value, float min, float max)
{
  if (value < min)
  {
    return min;
  }

  if (value > max)
  {
    return max;
  }

  return value;
}

void servo_init(TIM_HandleTypeDef *timer, uint32_t timer_channel)
{
  servo1.timer = timer;
  servo1.timer_channel = timer_channel;
  servo1.initialized = false;

  if ((servo1.timer == NULL) || (servo1.timer->Instance == NULL))
  {
    return;
  }

  if (HAL_TIM_PWM_Start(servo1.timer, servo1.timer_channel) != HAL_OK)
  {
    return;
  }

  __HAL_TIM_SET_COMPARE(servo1.timer, servo1.timer_channel, 0U);
  servo1.initialized = true;
}

static bool servo_set_duty_cycle(float duty_cycle)
{
  if (!servo1.initialized)
  {
    return false;
  }

  float clamped_duty = clampf(duty_cycle, 0.0f, 1.0f);
  uint32_t period_ticks = __HAL_TIM_GET_AUTORELOAD(servo1.timer) + 1U;
  uint32_t compare = (uint32_t)(clamped_duty * (float)period_ticks);

  if (compare > period_ticks)
  {
    compare = period_ticks;
  }

  __HAL_TIM_SET_COMPARE(servo1.timer, servo1.timer_channel, compare);
  return true;
}

bool servo_set_angle_deg(float angle_deg)
{
  float clamped_angle = clampf(angle_deg, SERVO_MIN_ANGLE_DEG, SERVO_MAX_ANGLE_DEG);
  float normalized = clamped_angle / SERVO_MAX_ANGLE_DEG;
  float duty = SERVO_MIN_DUTY + (normalized * (SERVO_MAX_DUTY - SERVO_MIN_DUTY));

  return servo_set_duty_cycle(duty);
}
