/* USER CODE BEGIN Header */
/**
 ******************************************************************************
 * @file    loader_service.h
 * @brief   This file contains all the function prototypes for
 *          the loader_service.c file
 ******************************************************************************
 */
/* USER CODE END Header */
/* Define to prevent recursive inclusion -------------------------------------*/
#ifndef __LOADER_SERVICE_H__
#define __LOADER_SERVICE_H__

#ifdef __cplusplus
extern "C"
{
#endif

#include "cmsis_os2.h"

  void start_loader_service(void *argument);

#ifdef __cplusplus
}
#endif

#endif /* __LOADER_SERVICE_H__ */
