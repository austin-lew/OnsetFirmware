/* USER CODE BEGIN Header */
/**
 ******************************************************************************
 * @file    led_driver.h
 * @brief   This file contains all the function prototypes for
 *          the led_driver.c file
 ******************************************************************************
 */
/* USER CODE END Header */
/* Define to prevent recursive inclusion -------------------------------------*/
#ifndef __LED_DRIVER_H__
#define __LED_DRIVER_H__

#ifdef __cplusplus
extern "C"
{
#endif

#include <stdbool.h>
#include <stdint.h>
#include "stm32g4xx_hal.h"

/* WS2812B frame format is 24 bits per LED in GRB order. */
#define LED_DRIVER_MAX_LEDS        64U
#define WS2812_BITS_PER_LED        24U
#define WS2812_RESET_SLOTS         64U
#define WS2812_TIMER_PERIOD_TICKS  180U
#define WS2812_T0H_TICKS           58U
#define WS2812_T1H_TICKS           115U

typedef struct
{
	uint8_t r;
	uint8_t g;
	uint8_t b;
} led_rgb_t;

void led_driver_init(uint16_t led_count);
HAL_StatusTypeDef led_set_pixel(uint16_t index, uint8_t r, uint8_t g, uint8_t b);
void led_set_all(uint8_t r, uint8_t g, uint8_t b);
void led_clear(void);
HAL_StatusTypeDef led_transmit(void);
bool led_is_busy(void);

void led_disable(void);
void led_enable(void);

#ifdef __cplusplus
}
#endif

#endif /* __LED_DRIVER_H__ */
