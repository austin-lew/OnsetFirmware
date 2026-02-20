#include <stdbool.h>
#include "elbow_service.h"
#include "stepper_driver.h"
#include "limitswitch_driver.h"
#include "cmsis_os2.h"

elbow_state_t elbow_state;
static osMessageQueueId_t limitswitch1_queue_handle = NULL;

static void limitswitch1_event_callback(limitswitch_event_t event){
    if(limitswitch1_queue_handle != NULL){
        uint16_t queue_msg = (uint16_t)event;
        osMessageQueuePut(limitswitch1_queue_handle, &queue_msg, 0, 0);
    }
}

void init_elbow_service(osMessageQueueId_t queue_handle){
    limitswitch1_queue_handle = queue_handle;
    elbow_state = NEEDS_HOME;
    register_limitswitch_callback(LIMITSWITCH_1, limitswitch1_event_callback);

}

static void state_decoder(uint16_t queue_msg){
    // figure out what state to transition to based on queue message
}

static void output_decoder(elbow_state_t state){
    // figure out what to do based on state
    switch (state){
        case NEEDS_HOME:
            // do nothing
            break;
        case HOMING:
            // move stepper towards home position until limit switch is triggered
            break;
        case IDLE:
            // do nothing
            break;
        case MOVING:
            // move stepper towards setpoint
            break;
    }
}

void start_elbow_service(void *argument){
    uint16_t queue_msg;
    while (true) {
        // Check the queue
        if((limitswitch1_queue_handle != NULL) &&
           (osMessageQueueGet(limitswitch1_queue_handle, &queue_msg, NULL, 0) == osOK)){
            state_decoder(queue_msg);
        }
        output_decoder(elbow_state);
    }
}

