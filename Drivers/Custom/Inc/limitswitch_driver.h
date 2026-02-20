/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file    limitswitch_driver.h
  * @brief   This file contains all the function prototypes for
  *          the limitswitch_driver.c file
  ******************************************************************************
  */
/* USER CODE END Header */
/* Define to prevent recursive inclusion -------------------------------------*/
#ifndef __LIMITSWITCH_DRIVER_H__
#define __LIMITSWITCH_DRIVER_H__

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>
#include "stm32g4xx_hal.h"
#include <stdbool.h>

typedef enum{
    LIMITSWITCH_PRESSED,
    LIMITSWITCH_RELEASED
}limitswitch_event_t;

typedef enum{
  LIMITSWITCH_1 = 0,
  LIMITSWITCH_2,
  LIMITSWITCH_3,
  LIMITSWITCH_4
} limitswitch_id_t;

typedef void (*limitswitch_callback_t)(limitswitch_event_t event);

bool register_limitswitch_callback(limitswitch_id_t limitswitch_id, limitswitch_callback_t callback);

#ifdef __cplusplus
}
#endif

#endif /* __LIMITSWITCH_DRIVER_H__ */


