/**
 ******************************************************************************
 * @file           : led_driver.c
 * @brief          : Implements the functions for controlling LEDs
 ******************************************************************************
 */

/* Includes ------------------------------------------------------------------*/
#include "led_driver.h"
#include "main.h"

void led_enable(void)
{
    HAL_GPIO_WritePin(LED_EN_GPIO_Port, LED_EN_Pin, GPIO_PIN_SET);
}

void led_disable(void)
{
    HAL_GPIO_WritePin(LED_EN_GPIO_Port, LED_EN_Pin, GPIO_PIN_RESET);
}