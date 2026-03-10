#include "encoder.h"

void encoder_init(TIM_HandleTypeDef *htm)
{
    HAL_TIM_Encoder_Start(htm, TIM_CHANNEL_ALL);
}

int16_t encoder_get_position(TIM_HandleTypeDef *htim)
{
    // Read the current count value from the timer's CNT register
    return (int16_t)(htim->Instance->CNT);
}

void encoder_set_position(TIM_HandleTypeDef *htim, int16_t position)
{
    // Set the timer's CNT register to the desired position
    htim->Instance->CNT = position;
}