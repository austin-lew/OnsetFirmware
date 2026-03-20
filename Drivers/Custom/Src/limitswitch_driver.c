/**
 ******************************************************************************
 * @file           : limitswitch_driver.c
 * @brief          : Implements a polling/busy-wait driver for the limit switches
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
    limitswitch_event_t last_sampled_event;
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

    if ((config.gpio_port == NULL) || (config.gpio_pin == 0U))
    {
        return false;
    }

    limitswitch_configs[limitswitch_id - 1U] = config;

    limitswitch_state_t *state = &limitswitch_states[limitswitch_id - 1U];
    state->stable_event = get_limitswitch_event(&config);
    state->last_sampled_event = state->stable_event;
    state->last_event_tick_ms = HAL_GetTick();
    state->initialized = true;

    return true;
}

static void limitswitch_poll(void)
{
    uint32_t now_ms = HAL_GetTick();

    for (uint32_t index = 0U; index < NUM_SWITCHES; index++)
    {
        limitswitch_config_t *config = &limitswitch_configs[index];
        limitswitch_state_t *state = &limitswitch_states[index];

        if (!state->initialized)
        {
            continue;
        }

        limitswitch_event_t sampled = get_limitswitch_event(config);

        /* Debounce: require the sampled value to be stable for a short time */
        if (sampled != state->last_sampled_event)
        {
            state->last_sampled_event = sampled;
            state->last_event_tick_ms = now_ms;
            continue;
        }

        if ((sampled != state->stable_event) &&
            ((now_ms - state->last_event_tick_ms) >= LIMITSWITCH_DEBOUNCE_MS))
        {
            state->stable_event = sampled;
            state->last_event_tick_ms = now_ms;
        }
    }
}

limitswitch_event_t limitswitch_get_state(limitswitch_id_t limitswitch_id)
{
    /* Ensure state is up-to-date before returning it. */
    limitswitch_poll();

    if ((limitswitch_id == 0U) || (limitswitch_id > NUM_SWITCHES))
    {
        return LIMITSWITCH_RELEASED;
    }

    limitswitch_state_t *state = &limitswitch_states[limitswitch_id - 1U];
    if (!state->initialized)
    {
        return LIMITSWITCH_RELEASED;
    }

    return state->stable_event;
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

