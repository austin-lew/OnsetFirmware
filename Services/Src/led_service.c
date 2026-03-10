#include "led_service.h"
#include "led_driver.h"
#include "cmsis_os2.h"
#include <stdbool.h>

typedef enum
{
    LED_OFF,
    LED_IDLE,
    LED_UPDATE
} led_state_t;

static led_state_t state;

static led_state_t init_led_service()
{
    led_enable();
    return LED_IDLE;
}

static led_state_t state_machine(led_state_t state)
{
    switch (state)
    {
    case LED_OFF:
        return LED_OFF;
    case LED_IDLE:
        return LED_IDLE;
    case LED_UPDATE:
        return LED_IDLE;
    default:
        return LED_IDLE;
    }
}

void start_led_service(void *argument)
{
    (void)argument;
    state = init_led_service();
    while (true)
    {
        state = state_machine(state);
        osDelay(50);
    }
}
