#include "led_service.h"
#include "led_driver.h"
#include "freertos_handles.h"
#include "cmsis_os2.h"
#include <stdbool.h>
#include "tim.h"

#define NUM_LEDS                128U
#define LAUNCH_LED_SEGMENTS     128U
#define LAUNCH_PHASE_DURATION   3000U
#define LAUNCH_LED_R            255U
#define LAUNCH_LED_G            100U
#define LAUNCH_LED_B            0U

/* Number of lit LEDs used by launch_pixel_fn; written from the LED task,
 * read from the DMA ISR — keep as volatile.                            */
static volatile uint16_t s_launch_lit_leds;

static void launch_pixel_fn(uint16_t index, uint8_t *r, uint8_t *g, uint8_t *b)
{
    if (index < s_launch_lit_leds)
    {
        *r = LAUNCH_LED_R;
        *g = LAUNCH_LED_G;
        *b = LAUNCH_LED_B;
    }
    else
    {
        *r = 0U;
        *g = 0U;
        *b = 0U;
    }
}

typedef enum
{
    LED_DISABLED,
    LED_READY,
    LED_SINGLE_COLOUR,
    LED_LAUNCH
} led_state_t;

static led_state_t state;

static void publish_led_status(led_to_serial_status_t status)
{
    osMessageQueuePut(led_to_serialHandle, &(led_to_serial_msg_t){status}, 0, 0);
}

static led_state_t init_led_service()
{
    led_driver_init(&htim2, TIM_CHANNEL_4, NUM_LEDS);
    led_disable();
    return LED_DISABLED;
}

static led_state_t handle_led_disabled(void)
{
    precharge_to_led_msg_t precharge_msg;
    if (osMessageQueueGet(precharge_to_ledHandle, &precharge_msg, NULL, 0) == osOK)
    {
        if (precharge_msg.status == STATUS_PRECHARGE_LED_ON)
        {
            osDelay(1000);
            publish_led_status(STATUS_LED_SERIAL_OFF);
            return LED_READY;
        }
    }

    serial_to_led_msg_t serial_msg;
    if (osMessageQueueGet(serial_to_ledHandle, &serial_msg, NULL, 0) == osOK)
    {
        publish_led_status(STATUS_LED_SERIAL_DISABLED);
        return LED_DISABLED;
    }

    osDelay(250);
    return LED_DISABLED;
}

static led_state_t handle_led_ready(void)
{
    serial_to_led_msg_t serial_msg;
    if (osMessageQueueGet(serial_to_ledHandle, &serial_msg, NULL, 0) == osOK)
    {
        switch (serial_msg.command)
        {
        case CMD_SERIAL_LED_LAUNCH:
            led_enable();
            osDelay(10);
            led_clear();
            led_transmit();
            publish_led_status(STATUS_LED_SERIAL_LAUNCH);
            return LED_LAUNCH;
        case CMD_SERIAL_LED_SINGLE_COLOUR:
            led_enable();
            osDelay(10);
            led_set_all(serial_msg.r, serial_msg.g, serial_msg.b);
            led_transmit();
            publish_led_status(STATUS_LED_SERIAL_SINGLE_COLOUR);
            return LED_SINGLE_COLOUR;
        default:
            return LED_READY;
        }
    }

    precharge_to_led_msg_t precharge_msg;
    if (osMessageQueueGet(precharge_to_ledHandle, &precharge_msg, NULL, 0) == osOK)
    {
        if (precharge_msg.status == STATUS_PRECHARGE_LED_OFF)
        {
            led_disable();
            publish_led_status(STATUS_LED_SERIAL_DISABLED);
            return LED_DISABLED;
        }
    }

    osDelay(250);
    return LED_READY;
}

static led_state_t handle_led_single_colour(void)
{
    precharge_to_led_msg_t precharge_msg;
    if (osMessageQueueGet(precharge_to_ledHandle, &precharge_msg, NULL, 0) == osOK)
    {
        if (precharge_msg.status == STATUS_PRECHARGE_LED_OFF)
        {
            led_disable();
            publish_led_status(STATUS_LED_SERIAL_DISABLED);
            return LED_DISABLED;
        }
    }

    serial_to_led_msg_t serial_msg;
    if (osMessageQueueGet(serial_to_ledHandle, &serial_msg, NULL, 0) == osOK)
    {
        switch (serial_msg.command)
        {
        case CMD_SERIAL_LED_LAUNCH:
            led_enable();
            publish_led_status(STATUS_LED_SERIAL_LAUNCH);
            return LED_LAUNCH;
        case CMD_SERIAL_LED_SINGLE_COLOUR:
            led_enable();
            led_set_all(serial_msg.r, serial_msg.g, serial_msg.b);
            led_transmit();
            publish_led_status(STATUS_LED_SERIAL_SINGLE_COLOUR);
            return LED_SINGLE_COLOUR;
        case CMD_SERIAL_LED_OFF:
            led_disable();
            publish_led_status(STATUS_LED_SERIAL_OFF);
            return LED_READY;
        }
    }

    osDelay(250);
    return LED_SINGLE_COLOUR;
}

static led_state_t handle_led_launch(void)
{
    led_set_pixel_fn(launch_pixel_fn);

    s_launch_lit_leds = NUM_LEDS;
    while (led_transmit() == HAL_BUSY)
    {
        osDelay(1);
    }

    /* Phase 1: turn off each segment from furthest to closest over 6 seconds */
    const uint32_t phase1_delay_ms = LAUNCH_PHASE_DURATION / LAUNCH_LED_SEGMENTS;
    const uint16_t step = NUM_LEDS / LAUNCH_LED_SEGMENTS;
    for (uint16_t seg = 0U; seg < LAUNCH_LED_SEGMENTS; seg++)
    {
        osDelay(phase1_delay_ms);
        s_launch_lit_leds = (uint16_t)(NUM_LEDS - (uint16_t)((seg + 1U) * step));
        while (led_transmit() == HAL_BUSY)
        {
            osDelay(1);
        }
    }

    led_set_all(LAUNCH_LED_R, LAUNCH_LED_G, LAUNCH_LED_B);

    publish_led_status(STATUS_LED_SERIAL_SINGLE_COLOUR);
    return LED_SINGLE_COLOUR;
}

static led_state_t state_machine(led_state_t state)
{
    switch (state)
    {
    case LED_DISABLED:
        return handle_led_disabled();
    case LED_READY:
        return handle_led_ready();
    case LED_SINGLE_COLOUR:
        return handle_led_single_colour();
    case LED_LAUNCH:
        return handle_led_launch();
    default:
        return LED_DISABLED;
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
