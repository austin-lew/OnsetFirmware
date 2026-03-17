/**
 ******************************************************************************
 * @file           : led_driver.c
 * @brief          : WS2812B driver — streaming circular DMA, no framebuffer.
 *
 * Instead of storing a full per-LED colour array and a full-frame DMA symbol
 * buffer, the driver uses a tiny ping-pong DMA buffer
 * (2 × LED_DRIVER_CHUNK_LEDS × 24 uint32_t = 1 536 B) and a pixel callback
 * that is invoked from the DMA half/complete ISR to fill each half on-the-fly.
 * This keeps RAM usage flat regardless of LED count, enabling 256 LEDs on a
 * 32 KB STM32G431.
 *
 * Streaming stop logic
 * --------------------
 * After all LED chunks are encoded, two zero-filled halves are issued to
 * produce the WS2812 reset pulse (≥ 50 µs low; each half = 8×24×1.25 µs =
 * 240 µs >> 50 µs).  Once both zero halves have finished playing the DMA is
 * aborted from within the ISR via HAL_TIM_PWM_Stop_DMA (which uses
 * HAL_DMA_Abort_IT internally — safe to call from a DMA callback).
 ******************************************************************************
 */

/* Includes ------------------------------------------------------------------*/
#include "led_driver.h"
#include "main.h"

/* WS2812B protocol constants (implementation detail, not in public header) */
#define WS2812_BITS_PER_LED        24U
#define WS2812_TIMER_PERIOD_TICKS  180U
#define WS2812_T0H_TICKS           58U
#define WS2812_T1H_TICKS           115U

/* Ping-pong DMA buffer sizing */
#define LED_DRIVER_HALF_SYMBOLS  (LED_DRIVER_CHUNK_LEDS * WS2812_BITS_PER_LED)  /* 192 */
#define LED_DRIVER_BUF_SYMBOLS   (LED_DRIVER_HALF_SYMBOLS * 2U)                 /* 384 */

/* Index into htim->hdma[] for each capture/compare channel */
#define TIM_DMA_IDX_CC1  1U
#define TIM_DMA_IDX_CC2  2U
#define TIM_DMA_IDX_CC3  3U
#define TIM_DMA_IDX_CC4  4U

typedef struct
{
    TIM_HandleTypeDef *timer;
    uint32_t           timer_channel;
    uint32_t           active_channel;
    uint16_t           dma_idx;
    bool               initialized;
} led_driver_config_t;

typedef struct { uint8_t r; uint8_t g; uint8_t b; } led_rgb_t;

/* ── Static storage ──────────────────────────────────────────────────────── */

/* Ping-pong DMA buffer: 2 halves × 8 LEDs × 24 bits × 4 B = 1 536 B */
static uint32_t s_pwm_symbols[LED_DRIVER_BUF_SYMBOLS];

/* Pixel source */
static led_pixel_fn_t s_pixel_fn;
static led_rgb_t      s_uniform_color;

/* Streaming state — only ever written from DMA ISR (single IRQ level),
 * except s_tx_busy which is also read from task context.              */
static uint16_t      s_next_led;
static uint8_t       s_post_led_halves;
static volatile bool s_tx_busy;

static uint16_t            s_led_count;
static led_driver_config_t s_led_driver = {0};

/* ── Internal helpers ────────────────────────────────────────────────────── */

static uint32_t timer_channel_to_active_channel(uint32_t ch)
{
    switch (ch)
    {
    case TIM_CHANNEL_1: return HAL_TIM_ACTIVE_CHANNEL_1;
    case TIM_CHANNEL_2: return HAL_TIM_ACTIVE_CHANNEL_2;
    case TIM_CHANNEL_3: return HAL_TIM_ACTIVE_CHANNEL_3;
    case TIM_CHANNEL_4: return HAL_TIM_ACTIVE_CHANNEL_4;
    default:            return HAL_TIM_ACTIVE_CHANNEL_CLEARED;
    }
}

static uint16_t timer_channel_to_dma_idx(uint32_t ch)
{
    switch (ch)
    {
    case TIM_CHANNEL_1: return TIM_DMA_IDX_CC1;
    case TIM_CHANNEL_2: return TIM_DMA_IDX_CC2;
    case TIM_CHANNEL_3: return TIM_DMA_IDX_CC3;
    case TIM_CHANNEL_4: return TIM_DMA_IDX_CC4;
    default:            return TIM_DMA_IDX_CC1;
    }
}

static uint16_t clamp_led_count(uint16_t requested)
{
    if (requested == 0U)                 return 1U;
    if (requested > LED_DRIVER_MAX_LEDS) return LED_DRIVER_MAX_LEDS;
    return requested;
}

/* Built-in callback that returns the same colour for every LED */
static void uniform_pixel_fn(uint16_t index, uint8_t *r, uint8_t *g, uint8_t *b)
{
    (void)index;
    *r = s_uniform_color.r;
    *g = s_uniform_color.g;
    *b = s_uniform_color.b;
}

/*
 * fill_half — encode the next chunk of LEDs into one half of the ping-pong
 * buffer.  When all LEDs have been encoded, fill with zeros (reset pulse)
 * and increment s_post_led_halves.
 */
static void fill_half(uint8_t half)
{
    uint32_t *buf = &s_pwm_symbols[(uint32_t)half * LED_DRIVER_HALF_SYMBOLS];

    if (s_next_led >= s_led_count)
    {
        for (uint16_t i = 0U; i < LED_DRIVER_HALF_SYMBOLS; i++)
        {
            buf[i] = 0U;
        }
        s_post_led_halves++;
        return;
    }

    uint16_t chunk_end = s_next_led + LED_DRIVER_CHUNK_LEDS;
    if (chunk_end > s_led_count)
    {
        chunk_end = s_led_count;
    }

    uint16_t idx = 0U;
    for (uint16_t led = s_next_led; led < chunk_end; led++)
    {
        uint8_t r, g, b;
        s_pixel_fn(led, &r, &g, &b);
        uint8_t grb[3] = {g, r, b};
        for (uint8_t byte_idx = 0U; byte_idx < 3U; byte_idx++)
        {
            for (int8_t bit = 7; bit >= 0; bit--)
            {
                buf[idx++] = ((grb[byte_idx] >> bit) & 1U) != 0U
                                 ? WS2812_T1H_TICKS
                                 : WS2812_T0H_TICKS;
            }
        }
    }

    /* Pad unused symbols in the last chunk with zeros */
    while (idx < LED_DRIVER_HALF_SYMBOLS)
    {
        buf[idx++] = 0U;
    }

    s_next_led = chunk_end;
}

/* Abort the circular DMA from within a DMA ISR callback.
 * HAL_TIM_PWM_Stop_DMA internally calls HAL_DMA_Abort_IT which is ISR-safe. */
static void stop_streaming(void)
{
    (void)HAL_TIM_PWM_Stop_DMA(s_led_driver.timer, s_led_driver.timer_channel);
    __HAL_TIM_SET_COMPARE(s_led_driver.timer, s_led_driver.timer_channel, 0U);
    s_tx_busy = false;
}

/* Common body for both DMA half-complete and full-complete callbacks.
 * `half`: the half that just finished playing (0 = first, 1 = second). */
static void streaming_isr(uint8_t half)
{
    if (!s_led_driver.initialized)
    {
        return;
    }

    /* Two zero halves have finished playing — reset pulse delivered */
    if (s_post_led_halves >= 2U)
    {
        stop_streaming();
        return;
    }

    fill_half(half);
}

/* ── Public API ──────────────────────────────────────────────────────────── */

void led_driver_init(TIM_HandleTypeDef *timer, uint32_t timer_channel, uint16_t led_count)
{
    s_led_count                 = clamp_led_count(led_count);
    s_led_driver.timer          = timer;
    s_led_driver.timer_channel  = timer_channel;
    s_led_driver.active_channel = timer_channel_to_active_channel(timer_channel);
    s_led_driver.dma_idx        = timer_channel_to_dma_idx(timer_channel);
    s_led_driver.initialized    = false;

    if ((s_led_driver.timer == NULL) ||
        (s_led_driver.timer->Instance == NULL) ||
        (s_led_driver.active_channel == HAL_TIM_ACTIVE_CHANNEL_CLEARED))
    {
        return;
    }

    s_led_driver.timer->Init.Period = WS2812_TIMER_PERIOD_TICKS - 1U;
    __HAL_TIM_SET_AUTORELOAD(s_led_driver.timer, WS2812_TIMER_PERIOD_TICKS - 1U);
    __HAL_TIM_SET_COMPARE(s_led_driver.timer, s_led_driver.timer_channel, 0U);
    __HAL_TIM_ENABLE_OCxPRELOAD(s_led_driver.timer, s_led_driver.timer_channel);

    /* Switch the TIM's DMA channel to circular mode so the ping-pong buffer
     * is refilled continuously via the half/complete callbacks.             */
    DMA_HandleTypeDef *hdma = s_led_driver.timer->hdma[s_led_driver.dma_idx];
    if (hdma != NULL)
    {
        hdma->Init.Mode = DMA_CIRCULAR;
        (void)HAL_DMA_Init(hdma);
    }

    s_led_driver.initialized = true;
    led_clear();
}

void led_set_pixel_fn(led_pixel_fn_t fn)
{
    s_pixel_fn = fn;
}

void led_set_all(uint8_t r, uint8_t g, uint8_t b)
{
    s_uniform_color.r = r;
    s_uniform_color.g = g;
    s_uniform_color.b = b;
    s_pixel_fn        = uniform_pixel_fn;
}

void led_clear(void)
{
    led_set_all(0U, 0U, 0U);
}

HAL_StatusTypeDef led_transmit(void)
{
    if (!s_led_driver.initialized)
    {
        return HAL_ERROR;
    }

    if (s_tx_busy)
    {
        return HAL_BUSY;
    }

    /* Reset streaming state and pre-fill both halves before starting DMA */
    s_next_led        = 0U;
    s_post_led_halves = 0U;
    fill_half(0U);
    fill_half(1U);

    s_tx_busy = true;
    __HAL_TIM_SET_COMPARE(s_led_driver.timer, s_led_driver.timer_channel, 0U);

    HAL_StatusTypeDef status = HAL_TIM_PWM_Start_DMA(
        s_led_driver.timer, s_led_driver.timer_channel,
        s_pwm_symbols, LED_DRIVER_BUF_SYMBOLS);

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

/* ── DMA callbacks ───────────────────────────────────────────────────────── */

/* Half-complete: half 0 just finished playing; DMA is now in half 1.
 * Refill half 0 with the next chunk.                                  */
void HAL_TIM_PWM_PulseFinishedHalfCpltCallback(TIM_HandleTypeDef *htim)
{
    if ((htim == s_led_driver.timer) && (htim->Channel == s_led_driver.active_channel))
    {
        streaming_isr(0U);
    }
}

/* Full-complete (circular wrap): half 1 just finished; DMA wrapped to half 0.
 * Refill half 1 with the next chunk.                                         */
void HAL_TIM_PWM_PulseFinishedCallback(TIM_HandleTypeDef *htim)
{
    if ((htim == s_led_driver.timer) && (htim->Channel == s_led_driver.active_channel))
    {
        streaming_isr(1U);
    }
}

/* ── GPIO helpers ────────────────────────────────────────────────────────── */

void led_enable(void)
{
    HAL_GPIO_WritePin(LED_EN_GPIO_Port, LED_EN_Pin, GPIO_PIN_SET);
}

void led_disable(void)
{
    HAL_GPIO_WritePin(LED_EN_GPIO_Port, LED_EN_Pin, GPIO_PIN_RESET);
}