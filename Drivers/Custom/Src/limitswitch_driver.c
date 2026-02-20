/**
  ******************************************************************************
  * @file           : limitswitch_driver.c
  * @brief          : Implements the interrupt handlers for the limit switches
  ******************************************************************************
  */

/* Includes ------------------------------------------------------------------*/
#include "limitswitch_driver.h"
#include <stdint.h>
#include "main.h"

static limitswitch_callback_t limitswitch1_callback = NULL;

bool register_limitswitch_callback(limitswitch_id_t limitswitch_id, limitswitch_callback_t callback){
    switch(limitswitch_id){
        case LIMITSWITCH_1:
            limitswitch1_callback = callback;
            return true;
        default:
            return false;
    }
}

static limitswitch_event_t get_limitswitch_event(uint16_t GPIO_PIN){
    if (HAL_GPIO_ReadPin(LIMIT_SW_1_GPIO_Port, GPIO_PIN) == GPIO_PIN_RESET){
        return LIMITSWITCH_PRESSED;
    } else {
        return LIMITSWITCH_RELEASED;
    }
}

void HAL_GPIO_EXTI_Callback(uint16_t GPIO_Pin)
{
    // Check which limit switch triggered. For the switch that triggered:
        // 1. Set an OS flag to stop the the corresponding motor 
    switch (GPIO_Pin) {
        case LIMIT_SW_1_Pin:
            if(limitswitch1_callback != NULL){
                limitswitch_event_t event = get_limitswitch_event(LIMIT_SW_1_Pin);
                limitswitch1_callback(event);
            }
            break;
        case LIMIT_SW_2_Pin:
            // Handle limit switch 2 event
            break;
        case LIMIT_SW_3_Pin:
            // Handle limit switch 3 event
            break;
        case LIMIT_SW_4_Pin:
            // Handle limit switch 4 event
            break;
        default:
            // Handle unexpected GPIO pin interrupt
            break;
    }
}