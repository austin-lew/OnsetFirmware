/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file    freertos_handles.h
  * @brief   This file contains all the rtos handles for the project
  ******************************************************************************
  */
/* USER CODE END Header */
/* Define to prevent recursive inclusion -------------------------------------*/
#ifndef __FREERTOS_HANDLES_H__
#define __FREERTOS_HANDLES_H__

#ifdef __cplusplus
extern "C" {
#endif

#include "cmsis_os2.h"


/* Serial to Elbow Message Passing       -------------------------------------*/
typedef enum{
    CMD_HOME=0,
    CMD_MOVE=1,
    CMD_STOP=2
} serial_to_elbow_cmd_t;

typedef struct{
    serial_to_elbow_cmd_t command;
    float value;
} serial_to_elbow_msg_t;

extern osMessageQueueId_t serial_to_elbowHandle;

/* Elbow to Serial Message Passing       -------------------------------------*/

typedef enum{
    STATUS_NEEDS_HOME=0,
    STATUS_HOMING=1,
    STATUS_HOME_ERROR=2,
    STATUS_HOME_SUCCESS=3,
    STATUS_MOVING=4,
    STATUS_MOVE_SUCCESS=5,
    STATUS_MOVE_ERROR=6,
} elbow_to_serial_status_t;

typedef struct{
    elbow_to_serial_status_t status;
    float value;
} elbow_to_serial_msg_t;

extern osMessageQueueId_t elbow_to_serialHandle;


#ifdef __cplusplus
}
#endif

#endif /* __FREERTOS_HANDLES_H__ */


