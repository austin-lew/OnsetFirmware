#include <stdbool.h>
#include "elbow_service.h"
#include "stepper_driver.h"
#include "limitswitch_driver.h"
#include "cmsis_os2.h"
#include "main.h"
#include "freertos_handles.h"
#include <math.h>

#define MICROSTEP (16)
#define ELBOW_REDUCTION (12)
#define STEPS_PER_REV (200 * MICROSTEP * ELBOW_REDUCTION)

static elbow_state_t state;
typedef struct {
    uint16_t move_target_steps;
} elbow_service_ctx_t;

static elbow_service_ctx_t service_ctx;
volatile limitswitch_event_t switch1_state;

static void limitswitch1_event_callback(limitswitch_event_t event){
    switch1_state = event;
}

static elbow_state_t init_elbow_service(){
    limitswitch_config_t limitswitch1_config = {
        .gpio_port = LIMIT_SW_1_GPIO_Port,
        .gpio_pin = LIMIT_SW_1_Pin,
        .pressed_state = GPIO_PIN_RESET,
        .callback = limitswitch1_event_callback
    };

    register_limitswitch(LIMITSWITCH_1, limitswitch1_config);

    return NEEDS_HOME;
}

static uint16_t rads_to_steps(float rads){
    float steps = rads * (STEPS_PER_REV / (2 * M_PI));
    return (uint16_t)steps;
}

static float steps_to_rads(uint16_t steps){
    float rads = steps * ((2 * M_PI) / STEPS_PER_REV);
    return rads;
}

static serial_to_elbow_msg_t get_serial_msg(){
    serial_to_elbow_msg_t msg;
    osMessageQueueGet(serial_to_elbowHandle, &msg, NULL, osWaitForever);
    return msg;
}

static void send_serial_msg(elbow_to_serial_status_t status, float value){
    elbow_to_serial_msg_t msg = {
        .status = status,
        .value = value
    };
    osMessageQueuePut(elbow_to_serialHandle, &msg, 0, 0);
}

static elbow_state_t handle_needs_home(void){
    serial_to_elbow_msg_t msg = get_serial_msg();

    if (msg.command == CMD_HOME) {
        send_serial_msg(STATUS_HOMING, 0);
        return HOMING;
    }

    send_serial_msg(STATUS_NEEDS_HOME_ERROR, 0);
    return NEEDS_HOME;
}

static elbow_state_t handle_homing(void){
    stepper_home();
    while (switch1_state != LIMITSWITCH_PRESSED) {
        osDelay(10);
    }
    stepper_emergency_stop();
    osDelay(100);
    stepper_tare();
    send_serial_msg(STATUS_HOME_SUCCESS, 0);
    return IDLE;
}

static elbow_state_t handle_idle(elbow_service_ctx_t *ctx){
    serial_to_elbow_msg_t msg = get_serial_msg();

    if (msg.command == CMD_MOVE) {
        if (msg.value < 0) {
            send_serial_msg(STATUS_MOVE_ERROR, msg.value);
            return IDLE;
        }

        ctx->move_target_steps = rads_to_steps(msg.value);
        send_serial_msg(STATUS_MOVING, msg.value);
        return MOVING;
    }

    if (msg.command == CMD_HOME) {
        send_serial_msg(STATUS_HOMING, 0);
        return HOMING;
    }

    return IDLE;
}

static elbow_state_t handle_moving(const elbow_service_ctx_t *ctx){
    bool move_started = stepper_absolute_move(ctx->move_target_steps);

    if (!move_started) {
        send_serial_msg(STATUS_MOVE_ERROR, steps_to_rads(ctx->move_target_steps));
        return IDLE;
    }

    send_serial_msg(STATUS_MOVE_SUCCESS, steps_to_rads(ctx->move_target_steps));
    return IDLE;
}

static elbow_state_t state_machine(elbow_state_t state){
    switch (state){
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

void start_elbow_service(void *argument){
    state = init_elbow_service();
    service_ctx.move_target_steps = 0;
    while (true) {
        state = state_machine(state);
        osDelay(10);
    }
}

