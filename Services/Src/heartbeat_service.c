#include "stm32g4xx_hal.h"
#include "main.h"
#include "cmsis_os2.h"
#include <stdbool.h>

#define HEARTBEAT_PERIOD_TICKS ((uint32_t)1000)

void start_heartbeat_service(void *argument)
{
  while(true)
  {
    HAL_GPIO_TogglePin(HEARTBEAT_LED_GPIO_Port, HEARTBEAT_LED_Pin);
    osDelay(HEARTBEAT_PERIOD_TICKS/2);
  }
}