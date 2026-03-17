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

/*
 * Maximum number of LEDs configurable at led_driver_init() time.
 * No per-LED colour array is stored; colour is produced on-demand via a
 * pixel callback, so this constant no longer drives buffer sizing.
 */
#define LED_DRIVER_MAX_LEDS    256U

/*
 * Number of LEDs encoded per DMA ping-pong half.
 * DMA symbol buffer = 2 x LED_DRIVER_CHUNK_LEDS x 24 x 4 B = 1 536 B,
 * regardless of LED_DRIVER_MAX_LEDS.
 */
#define LED_DRIVER_CHUNK_LEDS  8U

/*
 * Pixel-source callback invoked from the DMA ISR for each LED during
 * transmission.  Must be fast and must not block.
 */
typedef void (*led_pixel_fn_t)(uint16_t index, uint8_t *r, uint8_t *g, uint8_t *b);

void led_driver_init(TIM_HandleTypeDef *timer, uint32_t timer_channel, uint16_t led_count);

/* Set a custom per-pixel colour source for all future transmissions. */
void led_set_pixel_fn(led_pixel_fn_t fn);

/* Convenience: set every LED to the same colour (sets the pixel callback internally). */
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
