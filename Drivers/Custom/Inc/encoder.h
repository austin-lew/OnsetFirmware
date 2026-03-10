/* USER CODE BEGIN Header */
/**
 ******************************************************************************
 * @file    encoder.h
 * @brief   This file contains all the function prototypes for
 *          the encoder.c file
 ******************************************************************************
 */
/* USER CODE END Header */
/* Define to prevent recursive inclusion -------------------------------------*/
#ifndef __ENCODER_H__
#define __ENCODER_H__

#ifdef __cplusplus
extern "C"
{
#endif

#include <stdint.h>
#include "stm32g4xx_hal.h"
#include <stdbool.h>

void encoder_init(TIM_HandleTypeDef *htim);

int16_t encoder_get_position(TIM_HandleTypeDef *htim);

void encoder_set_position(TIM_HandleTypeDef *htim, int16_t position);

#ifdef __cplusplus
}
#endif

#endif /* __ENCODER_H__ */
