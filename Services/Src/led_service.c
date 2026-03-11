#include "led_service.h"
#include "led_driver.h"
#include "freertos_handles.h"
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
    led_driver_init(64);
    led_clear();
    (void)led_transmit();
    led_disable();
    return LED_OFF;
}

static led_state_t handle_led_off(void)
{
    precharge_to_led_msg_t msg;
    if (osMessageQueueGet(precharge_to_ledHandle, &msg, NULL, 0) == osOK)
    {
        if (msg.command == CMD_LED_ON)
        {
            osDelay(1000);
            led_enable();
            led_set_all(0U, 255U, 0U);
            (void)led_transmit();
            return LED_IDLE;
        }
    }
    return LED_OFF;
}

static led_state_t handle_led_idle(void)
{
    precharge_to_led_msg_t msg;
    if (osMessageQueueGet(precharge_to_ledHandle, &msg, NULL, 0) == osOK)
    {
        if (msg.command == CMD_LED_OFF)
        {
            led_clear();
            (void)led_transmit();
            led_disable();
            return LED_OFF;
        }
    }
    return LED_IDLE;
}

static led_state_t handle_led_update(void)
{
    return LED_IDLE;
}

static led_state_t state_machine(led_state_t state)
{
    switch (state)
    {
    case LED_OFF:
        return handle_led_off();
    case LED_IDLE:
        return handle_led_idle();
    case LED_UPDATE:
        return handle_led_update();
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
