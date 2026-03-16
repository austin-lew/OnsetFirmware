#include <stdbool.h>
#include "elbow_service.h"
#include "stepper_driver.h"
#include "limitswitch_driver.h"
#include "cmsis_os2.h"
#include "main.h"
#include "freertos_handles.h"
#include <math.h>
#include "tim.h"
#include "encoder.h"

#define MICROSTEP (16)
#define ELBOW_REDUCTION (7)
#define STEPS_PER_REV (200 * MICROSTEP * ELBOW_REDUCTION)
#define ELBOW_MAX_STEPS_PER_SECOND (5600U)
#define ELBOW_MAX_STEPS_PER_SECOND2 (5600U)
#define ELBOW_HOMING_SPEED_DIVISOR (10U)
#define ELBOW_HOMING_MAX_TRAVEL_RAD (M_PI / 2.0f)

static float clamp_elbow_angle_rads(float rads)
{
    if (rads < 0.0f)
    {
        return 0.0f;
    }

    if (rads > ELBOW_HOMING_MAX_TRAVEL_RAD)
    {
        return ELBOW_HOMING_MAX_TRAVEL_RAD;
    }

    return rads;
}

// Enum for elbow motor states
typedef enum
{
    NEEDS_HOME,
    HOMING,
    IDLE,
    MOVING
} elbow_state_t;

static elbow_state_t state;
typedef struct
{
    uint16_t move_target_steps;
} elbow_service_ctx_t;

static elbow_service_ctx_t service_ctx;
volatile limitswitch_event_t switch1_state;

static void limitswitch1_event_callback(limitswitch_event_t event)
{
    switch1_state = event;
}

static elbow_state_t init_elbow_service()
{
    encoder_init(&ENC_1_TIM);

    stepper_init(&htim15, TIM_CHANNEL_1, ELBOW_MAX_STEPS_PER_SECOND, ELBOW_MAX_STEPS_PER_SECOND2, ELBOW_DIR_GPIO_Port, ELBOW_DIR_Pin);
    limitswitch_config_t limitswitch1_config = {
        .gpio_port = LIMIT_SW_1_GPIO_Port,
        .gpio_pin = LIMIT_SW_1_Pin,
        .pressed_state = GPIO_PIN_RESET,
        .callback = limitswitch1_event_callback};

    register_limitswitch(LIMITSWITCH_1, limitswitch1_config);
    switch1_state = get_limitswitch_event(&limitswitch1_config);

    return NEEDS_HOME;
}

static uint16_t rads_to_steps(float rads)
{
    float steps = rads * (STEPS_PER_REV / (2 * M_PI));
    return (uint16_t)steps;
}

static float steps_to_rads(uint16_t steps)
{
    float rads = steps * ((2 * M_PI) / STEPS_PER_REV);
    return rads;
}

static serial_to_elbow_msg_t get_serial_msg()
{
    serial_to_elbow_msg_t msg;
    osMessageQueueGet(serial_to_elbowHandle, &msg, NULL, osWaitForever);
    return msg;
}

static void send_serial_msg(elbow_to_serial_status_t status, float value)
{
    elbow_to_serial_msg_t msg = {
        .status = status,
        .value = value};
    osMessageQueuePut(elbow_to_serialHandle, &msg, 0, 0);
}

static elbow_state_t handle_needs_home(void)
{
    serial_to_elbow_msg_t msg = get_serial_msg();

    if (msg.command == CMD_SERIAL_ELBOW_HOME)
    {
        send_serial_msg(STATUS_ELBOW_SERIAL_HOMING, 0);
        return HOMING;
    }

    send_serial_msg(STATUS_ELBOW_SERIAL_NEEDS_HOME, 0);
    osDelay(250);
    return NEEDS_HOME;
}

static elbow_state_t handle_homing(void)
{
    uint32_t homing_speed = ELBOW_MAX_STEPS_PER_SECOND / ELBOW_HOMING_SPEED_DIVISOR;
    int32_t homing_max_steps = (int32_t)rads_to_steps(ELBOW_HOMING_MAX_TRAVEL_RAD);

    if (homing_speed == 0U)
    {
        homing_speed = 1U;
    }

    if (homing_max_steps <= 0)
    {
        send_serial_msg(STATUS_ELBOW_SERIAL_HOME_ERROR, ELBOW_HOMING_MAX_TRAVEL_RAD);
        return NEEDS_HOME;
    }

    stepper_set_max_steps_per_second(homing_speed);

    stepper_relative_move(-homing_max_steps);
    while (switch1_state != LIMITSWITCH_PRESSED)
    {
        if (!stepper_is_moving())
        {
            send_serial_msg(STATUS_ELBOW_SERIAL_HOME_ERROR, ELBOW_HOMING_MAX_TRAVEL_RAD);
            stepper_set_max_steps_per_second(ELBOW_MAX_STEPS_PER_SECOND);
            return NEEDS_HOME;
        }
        osDelay(10);
    }

    stepper_smooth_stop();
    while (stepper_is_moving())
    {
        osDelay(250);
    }

    stepper_relative_move(homing_max_steps);
    while (switch1_state != LIMITSWITCH_RELEASED)
    {
        if (!stepper_is_moving())
        {
            send_serial_msg(STATUS_ELBOW_SERIAL_HOME_ERROR, ELBOW_HOMING_MAX_TRAVEL_RAD);
            stepper_set_max_steps_per_second(ELBOW_MAX_STEPS_PER_SECOND);
            return NEEDS_HOME;
        }
        osDelay(10);
    }

    stepper_smooth_stop();
    while (stepper_is_moving())
    {
        osDelay(250);
    }

    encoder_set_position(&ENC_1_TIM, 0);
    stepper_tare();
    stepper_set_max_steps_per_second(ELBOW_MAX_STEPS_PER_SECOND);
    send_serial_msg(STATUS_ELBOW_SERIAL_HOME_SUCCESS, 0);
    return IDLE;
}

static elbow_state_t handle_idle(elbow_service_ctx_t *ctx)
{
    serial_to_elbow_msg_t msg = get_serial_msg();

    if (msg.command == CMD_SERIAL_ELBOW_MOVE)
    {
        float target_rads = clamp_elbow_angle_rads(msg.value);

        ctx->move_target_steps = rads_to_steps(target_rads);
        send_serial_msg(STATUS_ELBOW_SERIAL_MOVING, target_rads);
        return MOVING;
    }

    if (msg.command == CMD_SERIAL_ELBOW_HOME)
    {
        send_serial_msg(STATUS_ELBOW_SERIAL_HOMING, 0);
        return HOMING;
    }

    osDelay(250);
    return IDLE;
}

static elbow_state_t handle_moving(const elbow_service_ctx_t *ctx)
{
    bool move_started = stepper_absolute_move(ctx->move_target_steps);

    if (!move_started)
    {
        send_serial_msg(STATUS_ELBOW_SERIAL_MOVE_ERROR, steps_to_rads(ctx->move_target_steps));
        return IDLE;
    }

    while (stepper_is_moving())
    {
        osDelay(250);
    }

    send_serial_msg(STATUS_ELBOW_SERIAL_MOVE_SUCCESS, steps_to_rads(ctx->move_target_steps));
    return IDLE;
}

static elbow_state_t state_machine(elbow_state_t state)
{
    switch (state)
    {
    case NEEDS_HOME:
        return handle_needs_home();
    case HOMING:
        return handle_homing();
    case IDLE:
        return handle_idle(&service_ctx);
    case MOVING:
        return handle_moving(&service_ctx);
    default:
        return state;
    }
}

void start_elbow_service(void *argument)
{
    state = init_elbow_service();
    service_ctx.move_target_steps = 0;
    while (true)
    {
        state = state_machine(state);
        osDelay(10);
    }
}
