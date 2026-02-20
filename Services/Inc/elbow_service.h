/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file    elbow_service.h
  * @brief   This file contains all the function prototypes for
  *          the elbow_service.c file
  ******************************************************************************
  */
/* USER CODE END Header */
/* Define to prevent recursive inclusion -------------------------------------*/
#ifndef __ELBOW_SERVICE_H__
#define __ELBOW_SERVICE_H__

#ifdef __cplusplus
extern "C" {
#endif

#include "cmsis_os2.h"

// Enum for elbow motor states
typedef enum{
  NEEDS_HOME,
  HOMING,
  IDLE,
  MOVING
} elbow_state_t;

extern elbow_state_t elbow_state;

void init_elbow_service(osMessageQueueId_t queue_handle);
void start_elbow_service(void *argument);

#ifdef __cplusplus
}
#endif

#endif /* __ELBOW_SERVICE_H__ */


