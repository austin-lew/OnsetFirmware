/**
  ******************************************************************************
  * @file           : limitswitch_driver.c
  * @brief          : Implements the interrupt handlers for the limit switches
  ******************************************************************************
  */

/* Includes ------------------------------------------------------------------*/
#include "limitswitch_driver.h"
#include <stdint.h>

#define NUM_SWITCHES (4)

static limitswitch_config_t limitswitch_configs[NUM_SWITCHES] = {0};

bool register_limitswitch(limitswitch_id_t limitswitch_id, limitswitch_config_t config)
{
    if (limitswitch_id >= NUM_SWITCHES) {
        return false;
    }

    if ((config.gpio_port == NULL) || (config.gpio_pin == 0U) || (config.callback == NULL)) {
        return false;
    }

    limitswitch_configs[limitswitch_id] = config;
    return true;
}

static limitswitch_event_t get_limitswitch_event(const limitswitch_config_t *config)
{
    if (HAL_GPIO_ReadPin(config->gpio_port, config->gpio_pin) == config->pressed_state){
        return LIMITSWITCH_PRESSED;
    } else {
        return LIMITSWITCH_RELEASED;
    }
}

void HAL_GPIO_EXTI_Callback(uint16_t GPIO_Pin)
{
    for (uint8_t index = 0; index < NUM_SWITCHES; index++) {
        limitswitch_config_t *config = &limitswitch_configs[index];
        if (config->gpio_pin == GPIO_Pin) {
            if (config->callback != NULL) {
                limitswitch_event_t event = get_limitswitch_event(config);
                config->callback(event);
            }
        }
    }
}