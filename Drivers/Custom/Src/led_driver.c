/**
 ******************************************************************************
 * @file           : led_driver.c
 * @brief          : Implements the functions for controlling LEDs
 ******************************************************************************
 */

/* Includes ------------------------------------------------------------------*/
#include "led_driver.h"
#include "main.h"

typedef struct
{
    TIM_HandleTypeDef *timer;
    uint32_t timer_channel;
    uint32_t active_channel;
    bool initialized;
} led_driver_config_t;

static led_rgb_t s_led_colors[LED_DRIVER_MAX_LEDS];
static uint32_t s_pwm_symbols[(LED_DRIVER_MAX_LEDS * WS2812_BITS_PER_LED) + WS2812_RESET_SLOTS];
static uint16_t s_led_count = 1U;
static uint16_t s_symbol_count = 0U;
static volatile bool s_tx_busy = false;
static led_driver_config_t s_led_driver = {0};

static uint32_t timer_channel_to_active_channel(uint32_t timer_channel)
{
    switch (timer_channel)
    {
    case TIM_CHANNEL_1:
        return HAL_TIM_ACTIVE_CHANNEL_1;
    case TIM_CHANNEL_2:
        return HAL_TIM_ACTIVE_CHANNEL_2;
    case TIM_CHANNEL_3:
        return HAL_TIM_ACTIVE_CHANNEL_3;
    case TIM_CHANNEL_4:
        return HAL_TIM_ACTIVE_CHANNEL_4;
    default:
        return HAL_TIM_ACTIVE_CHANNEL_CLEARED;
    }
}

static uint16_t clamp_led_count(uint16_t requested)
{
    if (requested == 0U)
    {
        return 1U;
    }

    if (requested > LED_DRIVER_MAX_LEDS)
    {
        return LED_DRIVER_MAX_LEDS;
    }

    return requested;
}

static void encode_frame(void)
{
    uint16_t symbol_index = 0U;

    for (uint16_t led = 0U; led < s_led_count; ++led)
    {
        uint8_t grb[3] = {s_led_colors[led].g, s_led_colors[led].r, s_led_colors[led].b};

        for (uint8_t byte_index = 0U; byte_index < 3U; ++byte_index)
        {
            for (int8_t bit = 7; bit >= 0; --bit)
            {
                s_pwm_symbols[symbol_index++] = ((grb[byte_index] >> bit) & 0x01U) != 0U
                                                    ? WS2812_T1H_TICKS
                                                    : WS2812_T0H_TICKS;
            }
        }
    }

    for (uint16_t i = 0U; i < WS2812_RESET_SLOTS; ++i)
    {
        s_pwm_symbols[symbol_index++] = 0U;
    }

    s_symbol_count = symbol_index;
}

void led_driver_init(TIM_HandleTypeDef *timer, uint32_t timer_channel, uint16_t led_count)
{
    s_led_count = clamp_led_count(led_count);
    s_led_driver.timer = timer;
    s_led_driver.timer_channel = timer_channel;
    s_led_driver.active_channel = timer_channel_to_active_channel(timer_channel);
    s_led_driver.initialized = false;

    if ((s_led_driver.timer == NULL) ||
        (s_led_driver.timer->Instance == NULL) ||
        (s_led_driver.active_channel == HAL_TIM_ACTIVE_CHANNEL_CLEARED))
    {
        led_clear();
        return;
    }

    s_led_driver.timer->Init.Period = WS2812_TIMER_PERIOD_TICKS - 1U;
    __HAL_TIM_SET_AUTORELOAD(s_led_driver.timer, WS2812_TIMER_PERIOD_TICKS - 1U);
    __HAL_TIM_SET_COMPARE(s_led_driver.timer, s_led_driver.timer_channel, 0U);
    __HAL_TIM_ENABLE_OCxPRELOAD(s_led_driver.timer, s_led_driver.timer_channel);
    s_led_driver.initialized = true;

    led_clear();
}

HAL_StatusTypeDef led_set_pixel(uint16_t index, uint8_t r, uint8_t g, uint8_t b)
{
    if (index >= s_led_count)
    {
        return HAL_ERROR;
    }

    s_led_colors[index].r = r;
    s_led_colors[index].g = g;
    s_led_colors[index].b = b;
    return HAL_OK;
}

void led_set_all(uint8_t r, uint8_t g, uint8_t b)
{
    for (uint16_t i = 0U; i < s_led_count; ++i)
    {
        s_led_colors[i].r = r;
        s_led_colors[i].g = g;
        s_led_colors[i].b = b;
    }
}

void led_clear(void)
{
    led_set_all(0U, 0U, 0U);
}

HAL_StatusTypeDef led_transmit(void)
{
    HAL_StatusTypeDef status;

    if (!s_led_driver.initialized)
    {
        return HAL_ERROR;
    }

    if (s_tx_busy)
    {
        return HAL_BUSY;
    }

    encode_frame();

    s_tx_busy = true;
    __HAL_TIM_SET_COMPARE(s_led_driver.timer, s_led_driver.timer_channel, 0U);
    status = HAL_TIM_PWM_Start_DMA(s_led_driver.timer, s_led_driver.timer_channel, s_pwm_symbols, s_symbol_count);

    if (status != HAL_OK)
    {
        s_tx_busy = false;
    }

    return status;
}

bool led_is_busy(void)
{
    return s_tx_busy;
}

void HAL_TIM_PWM_PulseFinishedCallback(TIM_HandleTypeDef *htim)
{
    if ((htim == s_led_driver.timer) &&
        s_led_driver.initialized &&
        (htim != NULL) &&
        (htim->Channel == s_led_driver.active_channel))
    {
        (void)HAL_TIM_PWM_Stop_DMA(s_led_driver.timer, s_led_driver.timer_channel);
        __HAL_TIM_SET_COMPARE(s_led_driver.timer, s_led_driver.timer_channel, 0U);
        s_tx_busy = false;
    }
}

void led_enable(void)
{
    HAL_GPIO_WritePin(LED_EN_GPIO_Port, LED_EN_Pin, GPIO_PIN_SET);
}

void led_disable(void)
{
    HAL_GPIO_WritePin(LED_EN_GPIO_Port, LED_EN_Pin, GPIO_PIN_RESET);
}