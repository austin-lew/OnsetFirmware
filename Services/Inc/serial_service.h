/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file    serial_service.h
  * @brief   This file contains all the function prototypes for
  *          the serial_service.c file
  ******************************************************************************
  */
/* USER CODE END Header */
/* Define to prevent recursive inclusion -------------------------------------*/
#ifndef __SERIAL_SERVICE_H__
#define __SERIAL_SERVICE_H__

#ifdef __cplusplus
extern "C" {
#endif

#include "cmsis_os2.h"

void start_serial_service(void *argument);

#ifdef __cplusplus
}
#endif

#endif /* __SERIAL_SERVICE_H__ */


