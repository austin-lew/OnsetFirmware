#include "precharge_service.h"
#include <stdbool.h>
#include "freertos_handles.h"
#include "main.h"

#define PRECHARGE_DURATION_MS (1000) // Duration to wait in PRECHARGE_ON state before allowing MAIN_POWER_ON
// Temporary bypass mode for load-related reset investigation.
// Set to 0 to restore the normal precharge state machine behavior.
#define PRECHARGE_TEMP_BYPASS_MODE (1)

typedef enum
{
    POWER_OFF,
    PRECHARGE_ON,
    MAIN_POWER_ON,
} precharge_state_t;

static precharge_state_t state = POWER_OFF;

static void precharge_on()
{
    HAL_GPIO_WritePin(PRECHRG_EN_GPIO_Port, PRECHRG_EN_Pin, GPIO_PIN_RESET);
}

static void precharge_off()
{
    HAL_GPIO_WritePin(PRECHRG_EN_GPIO_Port, PRECHRG_EN_Pin, GPIO_PIN_SET);
}

static void main_power_on()
{
    HAL_GPIO_WritePin(BATT_MAIN_EN_L_GPIO_Port, BATT_MAIN_EN_L_Pin, GPIO_PIN_RESET);
}

static void main_power_off()
{
    HAL_GPIO_WritePin(BATT_MAIN_EN_L_GPIO_Port, BATT_MAIN_EN_L_Pin, GPIO_PIN_SET);
}

static void drain_ignored_precharge_commands()
{
    serial_to_precharge_msg_t msg;
    while (osMessageQueueGet(serial_to_prechargeHandle, &msg, NULL, 0) == osOK)
    {
        (void)msg;
    }
}

static void send_led_command(precharge_to_led_cmd_t command)
{
    precharge_to_led_msg_t msg = {
        .command = command};
    osMessageQueuePut(precharge_to_ledHandle, &msg, 0, 0);
}

static precharge_state_t init_precharge_service()
{
    #if PRECHARGE_TEMP_BYPASS_MODE
    osMessageQueuePut(precharge_to_serialHandle, &(precharge_to_serial_msg_t){STATUS_PRECHARGE_MAIN_POWER_ON}, 0, 0);
    send_led_command(CMD_LED_ON);
    return MAIN_POWER_ON;
    #else
    osMessageQueuePut(precharge_to_serialHandle, &(precharge_to_serial_msg_t){STATUS_PRECHARGE_OFF}, 0, 0);
    return POWER_OFF;
    #endif
}

precharge_state_t handle_power_off_state()
{
    precharge_off();
    main_power_off();

    serial_to_precharge_msg_t msg;
    if (osMessageQueueGet(serial_to_prechargeHandle, &msg, NULL, 0) == osOK)
    {
        if (msg.command == CMD_PRECHARGE_ON)
        {
            osMessageQueuePut(precharge_to_serialHandle, &(precharge_to_serial_msg_t){STATUS_PRECHARGE_PRECHARGE_ON}, 0, 0);
            return PRECHARGE_ON;
        }
    }
    return POWER_OFF;
}


precharge_state_t handle_precharge_on_state()
{
    precharge_on();
    main_power_off();
    serial_to_precharge_msg_t msg;
    if (osMessageQueueGet(serial_to_prechargeHandle, &msg, NULL, PRECHARGE_DURATION_MS) == osOK)
    {
        if (msg.command == CMD_PRECHARGE_OFF)
        {
            precharge_off();
            osMessageQueuePut(precharge_to_serialHandle, &(precharge_to_serial_msg_t){STATUS_PRECHARGE_OFF}, 0, 0);
            return POWER_OFF;
        }
    }
    osMessageQueuePut(precharge_to_serialHandle, &(precharge_to_serial_msg_t){STATUS_PRECHARGE_MAIN_POWER_ON}, 0, 0);
    send_led_command(CMD_LED_ON);
    return MAIN_POWER_ON;
}

precharge_state_t handle_main_power_on_state()
{
    #if PRECHARGE_TEMP_BYPASS_MODE
    drain_ignored_precharge_commands();
    precharge_off();
    main_power_off();
    return MAIN_POWER_ON;
    #else
    precharge_on();
    main_power_on();
    serial_to_precharge_msg_t msg;
    if (osMessageQueueGet(serial_to_prechargeHandle, &msg, NULL, 0) == osOK)
    {
        if (msg.command == CMD_PRECHARGE_OFF)
        {
            main_power_off();
            osMessageQueuePut(precharge_to_serialHandle, &(precharge_to_serial_msg_t){STATUS_PRECHARGE_OFF}, 0, 0);
            send_led_command(CMD_LED_OFF);
            return POWER_OFF;
        }
    }
    return MAIN_POWER_ON;
    #endif
}

static precharge_state_t state_machine(precharge_state_t state)
{
    switch (state)
    {
    case POWER_OFF:
        return handle_power_off_state();
        break;
    case PRECHARGE_ON:
        return handle_precharge_on_state();
        break;
    case MAIN_POWER_ON:
        return handle_main_power_on_state();
        break;
    default:
        return POWER_OFF;
    }
}

void start_precharge_service(void *argument)
{
    (void)argument;
    state = init_precharge_service();
    while (true)
    {
        state = state_machine(state);
        osDelay(100);
    }
}
