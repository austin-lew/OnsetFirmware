/**
  ******************************************************************************
  * @file           : stepper_driver.c
  * @brief          : Implements the functions for controlling stepper motors
  ******************************************************************************
  */

/* Includes ------------------------------------------------------------------*/
#include "stepper_driver.h"

/**
  * @brief  Moves the stepper motor to an absolute position defined by the number of steps from the home position.
  * @note   Asserts that the stepper is in the IDLE or MOVING state
  * @param  stepper: Pointer to the stepper motor struct
  * @param  steps: The step position to move to. Zero is the home position.
  * @retval True if the move command was successfully issued, false otherwise
  */
bool stepper_absolute_move(stepper_t* stepper, uint16_t steps){
  // check if stepper is in the right state
  HAL_TIM_OC_Start_IT(stepper->timer, stepper->timer_channel); // start timer interrupt
  // update setpoint  
  return true;
}

/**
 * @brief  Immediately stops the stepper motor and clears the current move command.
 * @param  stepper: Pointer to the stepper motor struct
 * @retval True if the stop command was successfully issued, false otherwise
 */
bool stepper_emergency_stop(stepper_t* stepper){
  HAL_TIM_OC_Stop_IT(stepper->timer, stepper->timer_channel); // stop timer interrupt
  // update state
  return true;
}

/**
 * @brief Callback function for when the stepper output compare interrupt is triggered.
 * @note This function should be called in the HAL_TIM_OC_DelayElapsedCallback function in stm32g4xx_it.c
 * @param htim: pointer to the timer handle that triggered the interrupt
 * @retval None
 */
void HAL_TIM_OC_DelayElapsedCallback(TIM_HandleTypeDef *htim){
  switch((unsigned long)htim->Instance){
    case TIM3_BASE:
      // calculate next step pulse timing and update CCR
      break;
    default:
      break;
  }
}