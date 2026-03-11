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
extern "C"
{
#endif

#include "cmsis_os2.h"

    /* Serial to Elbow Message Passing       -------------------------------------*/
    typedef enum
    {
        CMD_ELBOW_HOME = 0,
        CMD_ELBOW_MOVE = 1,
        CMD_ELBOW_STOP = 2
    } serial_to_elbow_cmd_t;

    typedef struct
    {
        serial_to_elbow_cmd_t command;
        float value;
    } serial_to_elbow_msg_t;

    extern osMessageQueueId_t serial_to_elbowHandle;

    /* Elbow to Serial Message Passing       -------------------------------------*/

    typedef enum
    {
        STATUS_ELBOW_NEEDS_HOME = 0,
        STATUS_ELBOW_HOMING = 1,
        STATUS_ELBOW_HOME_ERROR = 2,
        STATUS_ELBOW_HOME_SUCCESS = 3,
        STATUS_ELBOW_MOVING = 4,
        STATUS_ELBOW_MOVE_SUCCESS = 5,
        STATUS_ELBOW_MOVE_ERROR = 6,
    } elbow_to_serial_status_t;

    typedef struct
    {
        elbow_to_serial_status_t status;
        float value;
    } elbow_to_serial_msg_t;

    extern osMessageQueueId_t elbow_to_serialHandle;

    /* Serial to Precharge Message Passing       -------------------------------------*/

    typedef enum
    {
        CMD_PRECHARGE_OFF = 0,
        CMD_PRECHARGE_ON = 1
    } serial_to_precharge_cmd_t;

    typedef struct
    {
        serial_to_precharge_cmd_t command;
    } serial_to_precharge_msg_t;

    extern osMessageQueueId_t serial_to_prechargeHandle;

    /* Precharge to Serial Message Passing       -------------------------------------*/

    typedef enum
    {
        STATUS_PRECHARGE_OFF = 0,
        STATUS_PRECHARGE_PRECHARGE_ON = 1,
        STATUS_PRECHARGE_MAIN_POWER_ON = 2
    } precharge_to_serial_status_t;

    typedef struct
    {
        precharge_to_serial_status_t status;
    } precharge_to_serial_msg_t;

    extern osMessageQueueId_t precharge_to_serialHandle;

    /* Precharge to LED Message Passing       -------------------------------------*/

    typedef enum
    {
        CMD_LED_OFF = 0,
        CMD_LED_ON = 1
    } precharge_to_led_cmd_t;

    typedef struct
    {
        precharge_to_led_cmd_t command;
    } precharge_to_led_msg_t;

    extern osMessageQueueId_t precharge_to_ledHandle;

#ifdef __cplusplus
}
#endif

#endif /* __FREERTOS_HANDLES_H__ */
