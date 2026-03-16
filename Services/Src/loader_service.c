#include "loader_service.h"
#include <stdbool.h>
#include "freertos_handles.h"
#include "servo_driver.h"
#include "tim.h"

#define LOADER_SERVO_REST_ANGLE_DEG (0.0f)
#define LOADER_SERVO_LOAD_ANGLE_DEG (180.0f)
#define LOADER_SERVO_TRAVEL_TIME_MS (1000U)

typedef enum
{
    LOADER_IDLE,
    LOADER_LOADING,
} loader_state_t;

static loader_state_t state = LOADER_IDLE;

static void publish_loader_status(loader_to_serial_status_t status)
{
    loader_to_serial_msg_t msg = {
        .status = status};
    osMessageQueuePut(loader_to_serialHandle, &msg, 0, 0);
}

static loader_state_t init_loader_service(void)
{
    servo_init(&htim15, TIM_CHANNEL_2);
    (void)servo_set_angle_deg(LOADER_SERVO_REST_ANGLE_DEG);
    publish_loader_status(STATUS_LOADER_SERIAL_IDLE);
    return LOADER_IDLE;
}

static loader_state_t handle_loader_idle(void)
{
    serial_to_loader_msg_t msg;

    if (osMessageQueueGet(serial_to_loaderHandle, &msg, NULL, 0) != osOK)
    {
        return LOADER_IDLE;
    }

    if (msg.command == CMD_SERIAL_LOADER_LOAD)
    {
        publish_loader_status(STATUS_LOADER_SERIAL_LOADING);
        return LOADER_LOADING;
    }

    return LOADER_IDLE;
}

static loader_state_t handle_loader_loading(void)
{
    (void)servo_set_angle_deg(LOADER_SERVO_LOAD_ANGLE_DEG);
    osDelay(LOADER_SERVO_TRAVEL_TIME_MS);
    (void)servo_set_angle_deg(LOADER_SERVO_REST_ANGLE_DEG);
    osDelay(LOADER_SERVO_TRAVEL_TIME_MS);

    publish_loader_status(STATUS_LOADER_SERIAL_IDLE);

    return LOADER_IDLE;
}

static loader_state_t state_machine(loader_state_t current_state)
{
    switch (current_state)
    {
    case LOADER_IDLE:
        return handle_loader_idle();
    case LOADER_LOADING:
        return handle_loader_loading();
    default:
        return LOADER_IDLE;
    }
}

void start_loader_service(void *argument)
{
    (void)argument;
    state = init_loader_service();

    while (true)
    {
        state = state_machine(state);
        osDelay(10);
    }
}
