/**
  ******************************************************************************
  * @file           : stepper_driver.c
  * @brief          : Implements the functions for controlling stepper motors
  ******************************************************************************
  */

/* Includes ------------------------------------------------------------------*/
#include "stepper_driver.h"
#include <math.h>

// Stepper motor config struct
typedef struct{
  TIM_HandleTypeDef*  timer;
  uint32_t            timer_channel;
  GPIO_TypeDef*       dir_gpio_port;
  uint16_t            dir_gpio_pin;
  uint32_t            max_steps_per_second;
  uint32_t            max_steps_per_second2;
  stepper_dir_t       direction;
  uint32_t            step_count;
  uint32_t            step_count_setpoint;
  uint16_t            ccr_increment;
} stepper_config_t;

stepper_config_t stepper1;

static uint16_t calculate_ccr_increment(stepper_config_t *stepper);

void stepper_init(TIM_HandleTypeDef* timer,
                  uint32_t timer_channel,
                  uint32_t max_steps_per_second,
                  uint32_t max_steps_per_second2,
                  GPIO_TypeDef* dir_gpio_port,
                  uint16_t dir_gpio_pin)
{
  stepper1.timer = timer;
  stepper1.timer_channel = timer_channel;
  stepper1.dir_gpio_port = dir_gpio_port;
  stepper1.dir_gpio_pin = dir_gpio_pin;
  stepper1.max_steps_per_second = max_steps_per_second;
  stepper1.max_steps_per_second2 = max_steps_per_second2;
  
  stepper1.direction = DIR_CW;
  stepper1.step_count = 0;
  stepper1.step_count_setpoint = 0;
  stepper1.ccr_increment = 0;
}

void stepper_home(){
  stepper1.step_count = 4294967295;
  stepper1.direction = DIR_CCW;
  stepper1.step_count_setpoint = 0;
  HAL_TIM_OC_Start_IT(stepper1.timer, stepper1.timer_channel); // start
}

void stepper_tare(){
  stepper1.step_count = 0;
}

bool stepper_absolute_move(uint16_t steps){
  // check if stepper is in the right state -- must be homed
  if (steps >= stepper1.step_count) {
    stepper1.direction = DIR_CW;
  } else {
    stepper1.direction = DIR_CCW;
  }
  stepper1.step_count_setpoint = steps;
  HAL_TIM_OC_Start_IT(stepper1.timer, stepper1.timer_channel); // start timer interrupt
  // update setpoint  
  return true;
}

bool stepper_emergency_stop(void){
  HAL_TIM_OC_Stop_IT(stepper1.timer, stepper1.timer_channel); // stop timer interrupt
  // update state
  return true;
}

static uint16_t calculate_ccr_increment(stepper_config_t *stepper){
  // 1) Validate configuration and required runtime state.
  if ((stepper == NULL) || (stepper->timer == NULL) || (stepper->timer->Instance == NULL)) {
    return 0U;
  }

  if (stepper->max_steps_per_second == 0U) {
    return 0U;
  }

  // 2) Compute timer tick frequency used for speed <-> CCR conversion.
  uint16_t prescaler = (uint16_t)(stepper->timer->Instance->PSC + 1U);
  uint32_t timer_clock_freq = HAL_RCC_GetPCLK1Freq();
  uint32_t timer_tick_freq = timer_clock_freq / prescaler;
  if (timer_tick_freq == 0U) {
    return 0U;
  }

  // 3) Stop condition for finite (absolute) moves.
  if (stepper->step_count == stepper->step_count_setpoint) {
    return 0U;
  }

  // 4) Remaining distance in steps for finite moves.
  uint32_t remaining_steps = 0U;
  if (stepper->step_count_setpoint >= stepper->step_count) {
    remaining_steps = stepper->step_count_setpoint - stepper->step_count;
  } else {
    remaining_steps = stepper->step_count - stepper->step_count_setpoint;
  }

  // 5) Derive current speed from previous CCR increment.
  float current_speed = 0.0f;
  if (stepper->ccr_increment > 0U) {
    current_speed = (float)timer_tick_freq / (float)stepper->ccr_increment;
  }

  // 6) Compute desired target speed (velocity limit + stop-distance limit).
  float target_speed = (float)stepper->max_steps_per_second;
  if (stepper->max_steps_per_second2 == 0U) {
    target_speed = (float)stepper->max_steps_per_second;
  } else {
    float max_speed_for_stop = sqrtf(2.0f * (float)stepper->max_steps_per_second2 * (float)remaining_steps);
    if (max_speed_for_stop < target_speed) {
      target_speed = max_speed_for_stop;
    }
  }
  

  // 7) Apply acceleration-constrained speed update per step.
  float new_speed = current_speed;
  if (target_speed <= 0.0f) {
    return 0U;
  }

  if (stepper->max_steps_per_second2 == 0U) {
    new_speed = target_speed;
  } else if (current_speed <= 0.0f) {
    float start_speed = sqrtf(2.0f * (float)stepper->max_steps_per_second2);
    new_speed = (start_speed < target_speed) ? start_speed : target_speed;
  } else {
    float delta_v = (float)stepper->max_steps_per_second2 / current_speed;
    if (target_speed > current_speed) {
      new_speed = current_speed + delta_v;
      if (new_speed > target_speed) {
        new_speed = target_speed;
      }
    } else {
      new_speed = current_speed - delta_v;
      if (new_speed < target_speed) {
        new_speed = target_speed;
      }
    }
  }

  // 8) Convert requested speed back to timer CCR increment.
  if (new_speed <= 0.0f) {
    return 0U;
  }

  float ccr = (float)timer_tick_freq / new_speed;
  if (ccr > 65535.0f) {
    return 65535U;
  }

  return (uint16_t)ccr;
}

void HAL_TIM_OC_DelayElapsedCallback(TIM_HandleTypeDef *htim){
  switch((unsigned long)htim->Instance){
    case TIM3_BASE:
      // A) End finite move when setpoint has been reached.
      if (stepper1.step_count_setpoint == stepper1.step_count) {
        HAL_TIM_OC_Stop_IT(stepper1.timer, stepper1.timer_channel); // stop timer interrupt
        break;
      }

      // B) Compute next interval from acceleration-limited profile.
      stepper1.ccr_increment = calculate_ccr_increment(&stepper1);
      if (stepper1.ccr_increment == 0U) {
        HAL_TIM_OC_Stop_IT(stepper1.timer, stepper1.timer_channel);
        break;
      }

      // C) Update absolute position estimate.
      if (stepper1.direction == DIR_CW) {
        stepper1.step_count++;
      } else if (stepper1.step_count > 0U) {
        stepper1.step_count--;
      }

      // D) Schedule next output compare event.
      htim->Instance->CCR1 += stepper1.ccr_increment;
      break;
    default:
      break;
  }
}