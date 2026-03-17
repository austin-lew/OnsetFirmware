/**
 ******************************************************************************
 * @file           : limitswitch_driver.c
 * @brief          : Implements the interrupt handlers for the limit switches
 ******************************************************************************
 */

/* Includes ------------------------------------------------------------------*/
#include "limitswitch_driver.h"
#include <stdint.h>

#define NUM_SWITCHES (3)
#define LIMITSWITCH_DEBOUNCE_MS (20U)

typedef struct
{
    limitswitch_event_t stable_event;
    uint32_t last_event_tick_ms;
    bool initialized;
} limitswitch_state_t;

static limitswitch_config_t limitswitch_configs[NUM_SWITCHES] = {0};
static limitswitch_state_t limitswitch_states[NUM_SWITCHES] = {0};

bool register_limitswitch(limitswitch_id_t limitswitch_id, limitswitch_config_t config)
{
    if ((limitswitch_id == 0U) || (limitswitch_id > NUM_SWITCHES))
    {
        return false;
    }

    if ((config.gpio_port == NULL) || (config.gpio_pin == 0U) || (config.callback == NULL))
    {
        return false;
    }

    limitswitch_configs[limitswitch_id - 1U] = config;

    limitswitch_state_t *state = &limitswitch_states[limitswitch_id - 1U];
    state->stable_event = get_limitswitch_event(&config);
    state->last_event_tick_ms = HAL_GetTick();
    state->initialized = true;

    return true;
}

limitswitch_event_t get_limitswitch_event(const limitswitch_config_t *config)
{
    if (HAL_GPIO_ReadPin(config->gpio_port, config->gpio_pin) == config->pressed_state)
    {
        return LIMITSWITCH_PRESSED;
    }
    else
    {
        return LIMITSWITCH_RELEASED;
    }
}

void HAL_GPIO_EXTI_Callback(uint16_t GPIO_Pin)
{
    for (uint8_t index = 0; index < NUM_SWITCHES; index++)
    {
        limitswitch_config_t *config = &limitswitch_configs[index];
        limitswitch_state_t *state = &limitswitch_states[index];

        if (config->gpio_pin == GPIO_Pin)
        {
            if ((config->callback != NULL) && state->initialized)
            {
                uint32_t now_ms = HAL_GetTick();
                if ((now_ms - state->last_event_tick_ms) < LIMITSWITCH_DEBOUNCE_MS)
                {
                    continue;
                }

                limitswitch_event_t event = get_limitswitch_event(config);
                if (event != state->stable_event)
                {
                    state->stable_event = event;
                    state->last_event_tick_ms = now_ms;
                    config->callback(event);
                }
            }
        }
    }
}