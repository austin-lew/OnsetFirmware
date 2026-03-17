#include "stepper_driver.h"

#include <math.h>

typedef struct
{
	TIM_HandleTypeDef *timer;
	uint32_t timer_channel;
	uint32_t active_channel;
	GPIO_TypeDef *dir_gpio_port;
	uint16_t dir_gpio_pin;

	uint32_t timer_tick_hz;
	uint32_t timer_arr;

	volatile bool initialized;
	volatile bool is_moving;
	volatile bool smooth_stop_requested;
	volatile bool step_pin_high;

	int32_t current_position_steps;
	int32_t target_position_steps;
	uint32_t remaining_steps;
	int8_t direction_sign;

	float max_steps_per_second;
	float accel_steps_per_second2;
	float current_steps_per_second;
	float min_realizable_steps_per_second;
	uint32_t half_period_ticks;
} stepper_ctx_t;

static stepper_ctx_t g_stepper = {0};

static uint32_t stepper_channel_to_active(uint32_t channel)
{
	switch (channel)
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

static bool stepper_timer_on_apb2(TIM_TypeDef *instance)
{
	return (instance == TIM1) || (instance == TIM8) || (instance == TIM15) || (instance == TIM16) || (instance == TIM17);
}

static uint32_t stepper_get_timer_input_clock_hz(TIM_HandleTypeDef *timer)
{
	RCC_ClkInitTypeDef clk_config;
	uint32_t flash_latency = 0U;
	uint32_t pclk_hz;
	uint32_t apb_divider;

	HAL_RCC_GetClockConfig(&clk_config, &flash_latency);

	if (stepper_timer_on_apb2(timer->Instance))
	{
		pclk_hz = HAL_RCC_GetPCLK2Freq();
		apb_divider = clk_config.APB2CLKDivider;
	}
	else
	{
		pclk_hz = HAL_RCC_GetPCLK1Freq();
		apb_divider = clk_config.APB1CLKDivider;
	}

	if (apb_divider == RCC_HCLK_DIV1)
	{
		return pclk_hz;
	}

	return (2U * pclk_hz);
}

static uint32_t stepper_get_compare(uint32_t channel)
{
	return __HAL_TIM_GET_COMPARE(g_stepper.timer, channel);
}

static void stepper_set_compare(uint32_t channel, uint32_t value)
{
	__HAL_TIM_SET_COMPARE(g_stepper.timer, channel, value);
}

static uint32_t stepper_speed_to_half_period_ticks(float steps_per_second)
{
	float ticks_f;
	uint32_t ticks;

	if (steps_per_second <= 0.0f)
	{
		return g_stepper.timer_arr;
	}

	ticks_f = ((float)g_stepper.timer_tick_hz) / (2.0f * steps_per_second);
	if (ticks_f < 1.0f)
	{
		return 1U;
	}

	if (ticks_f > (float)g_stepper.timer_arr)
	{
		return g_stepper.timer_arr;
	}

	ticks = (uint32_t)(ticks_f + 0.5f);
	if (ticks == 0U)
	{
		ticks = 1U;
	}

	return ticks;
}

static float stepper_ticks_to_steps_per_second(uint32_t half_period_ticks)
{
	if (half_period_ticks == 0U)
	{
		return 0.0f;
	}

	return ((float)g_stepper.timer_tick_hz) / (2.0f * (float)half_period_ticks);
}

static void stepper_stop_output(void)
{
	if (g_stepper.timer != NULL)
	{
		(void)HAL_TIM_OC_Stop_IT(g_stepper.timer, g_stepper.timer_channel);
	}

	g_stepper.is_moving = false;
	g_stepper.smooth_stop_requested = false;
	g_stepper.step_pin_high = false;
	g_stepper.half_period_ticks = 0U;
	g_stepper.current_steps_per_second = 0.0f;
	g_stepper.remaining_steps = 0U;
}

static bool stepper_start_move_to_target(int32_t target_steps)
{
	int32_t delta;
	uint32_t abs_steps;
	uint32_t current_compare;
	uint32_t cnt;

	if (!g_stepper.initialized || g_stepper.timer == NULL)
	{
		return false;
	}

	if (g_stepper.is_moving)
	{
		return false;
	}

	delta = target_steps - g_stepper.current_position_steps;
	if (delta == 0)
	{
		return true;
	}

	abs_steps = (delta > 0) ? (uint32_t)delta : (uint32_t)(-delta);
	g_stepper.direction_sign = (delta > 0) ? 1 : -1;
	HAL_GPIO_WritePin(g_stepper.dir_gpio_port,
					  g_stepper.dir_gpio_pin,
					  (g_stepper.direction_sign > 0) ? GPIO_PIN_SET : GPIO_PIN_RESET);

	g_stepper.target_position_steps = target_steps;
	g_stepper.remaining_steps = abs_steps;
	g_stepper.smooth_stop_requested = false;
	g_stepper.step_pin_high = false;
	g_stepper.is_moving = true;

	g_stepper.current_steps_per_second = g_stepper.min_realizable_steps_per_second;
	if (g_stepper.current_steps_per_second > g_stepper.max_steps_per_second)
	{
		g_stepper.current_steps_per_second = g_stepper.max_steps_per_second;
	}

	g_stepper.half_period_ticks = stepper_speed_to_half_period_ticks(g_stepper.current_steps_per_second);
	g_stepper.current_steps_per_second = stepper_ticks_to_steps_per_second(g_stepper.half_period_ticks);

	(void)HAL_TIM_OC_Stop_IT(g_stepper.timer, g_stepper.timer_channel);

	cnt = __HAL_TIM_GET_COUNTER(g_stepper.timer);
	current_compare = cnt + g_stepper.half_period_ticks;
	stepper_set_compare(g_stepper.timer_channel, current_compare);

	__HAL_TIM_CLEAR_FLAG(g_stepper.timer, TIM_FLAG_CC1 | TIM_FLAG_CC2 | TIM_FLAG_CC3 | TIM_FLAG_CC4);

	if (HAL_TIM_OC_Start_IT(g_stepper.timer, g_stepper.timer_channel) != HAL_OK)
	{
		stepper_stop_output();
		return false;
	}

	return true;
}

void stepper_init(TIM_HandleTypeDef *timer,
				  uint32_t timer_channel,
				  uint32_t max_steps_per_second,
				  uint32_t max_steps_per_second2,
				  GPIO_TypeDef *dir_gpio_port,
				  uint16_t dir_gpio_pin)
{
	uint32_t input_clock_hz;
	uint32_t psc;

	g_stepper.initialized = false;

	if ((timer == NULL) || (dir_gpio_port == NULL) || (max_steps_per_second == 0U) || (max_steps_per_second2 == 0U))
	{
		return;
	}

	g_stepper.timer = timer;
	g_stepper.timer_channel = timer_channel;
	g_stepper.active_channel = stepper_channel_to_active(timer_channel);
	g_stepper.dir_gpio_port = dir_gpio_port;
	g_stepper.dir_gpio_pin = dir_gpio_pin;

	input_clock_hz = stepper_get_timer_input_clock_hz(timer);
	psc = timer->Init.Prescaler + 1U;
	if (psc == 0U)
	{
		psc = 1U;
	}

	g_stepper.timer_tick_hz = input_clock_hz / psc;
	g_stepper.timer_arr = __HAL_TIM_GET_AUTORELOAD(timer);
	if (g_stepper.timer_arr == 0U)
	{
		g_stepper.timer_arr = 1U;
	}

	g_stepper.max_steps_per_second = (float)max_steps_per_second;
	g_stepper.accel_steps_per_second2 = (float)max_steps_per_second2;

	g_stepper.min_realizable_steps_per_second =
		((float)g_stepper.timer_tick_hz) / (2.0f * (float)g_stepper.timer_arr);

	if (g_stepper.max_steps_per_second < g_stepper.min_realizable_steps_per_second)
	{
		g_stepper.max_steps_per_second = g_stepper.min_realizable_steps_per_second;
	}

	g_stepper.current_position_steps = 0;
	g_stepper.target_position_steps = 0;
	g_stepper.direction_sign = 1;

	stepper_stop_output();
	HAL_GPIO_WritePin(g_stepper.dir_gpio_port, g_stepper.dir_gpio_pin, GPIO_PIN_RESET);

	g_stepper.initialized = true;
}

void stepper_tare(void)
{
	uint32_t primask = __get_PRIMASK();
	__disable_irq();

	if (!g_stepper.is_moving)
	{
		g_stepper.current_position_steps = 0;
		g_stepper.target_position_steps = 0;
	}

	if (primask == 0U)
	{
		__enable_irq();
	}
}

void stepper_set_max_steps_per_second(uint32_t max_steps_per_second)
{
	float new_max;

	if ((max_steps_per_second == 0U) || !g_stepper.initialized)
	{
		return;
	}

	new_max = (float)max_steps_per_second;
	if (new_max < g_stepper.min_realizable_steps_per_second)
	{
		new_max = g_stepper.min_realizable_steps_per_second;
	}

	g_stepper.max_steps_per_second = new_max;
}

bool stepper_relative_move(int32_t delta_steps)
{
	uint32_t primask;
	bool started;

	primask = __get_PRIMASK();
	__disable_irq();
	started = stepper_start_move_to_target(g_stepper.current_position_steps + delta_steps);
	if (primask == 0U)
	{
		__enable_irq();
	}

	return started;
}

bool stepper_absolute_move(uint16_t steps)
{
	uint32_t primask;
	bool started;

	primask = __get_PRIMASK();
	__disable_irq();
	started = stepper_start_move_to_target((int32_t)steps);
	if (primask == 0U)
	{
		__enable_irq();
	}

	return started;
}

bool stepper_smooth_stop(void)
{
	if (!g_stepper.initialized)
	{
		return false;
	}

	g_stepper.smooth_stop_requested = true;
	return true;
}

bool stepper_emergency_stop(void)
{
	uint32_t primask;

	if (!g_stepper.initialized)
	{
		return false;
	}

	primask = __get_PRIMASK();
	__disable_irq();
	stepper_stop_output();
	if (primask == 0U)
	{
		__enable_irq();
	}

	return true;
}

bool stepper_is_moving(void)
{
	return g_stepper.is_moving;
}

void HAL_TIM_OC_DelayElapsedCallback(TIM_HandleTypeDef *htim)
{
	uint32_t current_compare;
	uint32_t next_compare;
	bool decelerate;
	float stop_distance_steps;
	float v;
	float next_v;

	if (!g_stepper.initialized || !g_stepper.is_moving)
	{
		return;
	}

	if (htim != g_stepper.timer)
	{
		return;
	}

	if (htim->Channel != g_stepper.active_channel)
	{
		return;
	}

	g_stepper.step_pin_high = !g_stepper.step_pin_high;

	if (g_stepper.step_pin_high)
	{
		if (g_stepper.direction_sign > 0)
		{
			g_stepper.current_position_steps++;
		}
		else
		{
			g_stepper.current_position_steps--;
		}

		if (g_stepper.remaining_steps > 0U)
		{
			g_stepper.remaining_steps--;
		}

		if (g_stepper.remaining_steps == 0U)
		{
			g_stepper.target_position_steps = g_stepper.current_position_steps;
			stepper_stop_output();
			return;
		}

		v = g_stepper.current_steps_per_second;
		if (v < g_stepper.min_realizable_steps_per_second)
		{
			v = g_stepper.min_realizable_steps_per_second;
		}

		stop_distance_steps = (v * v) / (2.0f * g_stepper.accel_steps_per_second2);
		decelerate = g_stepper.smooth_stop_requested || (stop_distance_steps >= (float)g_stepper.remaining_steps);

		if (decelerate)
		{
			float v2 = (v * v) - (2.0f * g_stepper.accel_steps_per_second2);
			if (v2 <= 0.0f)
			{
				next_v = 0.0f;
			}
			else
			{
				next_v = sqrtf(v2);
			}

			if (g_stepper.smooth_stop_requested && (next_v <= g_stepper.min_realizable_steps_per_second))
			{
				stepper_stop_output();
				return;
			}

			if (next_v < g_stepper.min_realizable_steps_per_second)
			{
				next_v = g_stepper.min_realizable_steps_per_second;
			}
		}
		else
		{
			float v2 = (v * v) + (2.0f * g_stepper.accel_steps_per_second2);
			next_v = sqrtf(v2);
			if (next_v > g_stepper.max_steps_per_second)
			{
				next_v = g_stepper.max_steps_per_second;
			}
		}

		g_stepper.current_steps_per_second = next_v;
		g_stepper.half_period_ticks = stepper_speed_to_half_period_ticks(g_stepper.current_steps_per_second);
		g_stepper.current_steps_per_second = stepper_ticks_to_steps_per_second(g_stepper.half_period_ticks);
	}

	current_compare = stepper_get_compare(g_stepper.timer_channel);
	next_compare = current_compare + g_stepper.half_period_ticks;
	stepper_set_compare(g_stepper.timer_channel, next_compare);
}