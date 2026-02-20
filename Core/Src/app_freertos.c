/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * File Name          : app_freertos.c
  * Description        : Code for freertos applications
  ******************************************************************************
  * @attention
  *
  * Copyright (c) 2026 STMicroelectronics.
  * All rights reserved.
  *
  * This software is licensed under terms that can be found in the LICENSE file
  * in the root directory of this software component.
  * If no LICENSE file comes with this software, it is provided AS-IS.
  *
  ******************************************************************************
  */
/* USER CODE END Header */

/* Includes ------------------------------------------------------------------*/
#include "FreeRTOS.h"
#include "task.h"
#include "main.h"
#include "cmsis_os.h"

#include "usb_device.h"

/* Private includes ----------------------------------------------------------*/
/* USER CODE BEGIN Includes */
#include "heartbeat_service.h"
#include "elbow_service.h"
/* USER CODE END Includes */

/* Private typedef -----------------------------------------------------------*/
/* USER CODE BEGIN PTD */

/* USER CODE END PTD */

/* Private define ------------------------------------------------------------*/
/* USER CODE BEGIN PD */
/* USER CODE END PD */

/* Private macro -------------------------------------------------------------*/
/* USER CODE BEGIN PM */

/* USER CODE END PM */

/* Private variables ---------------------------------------------------------*/
/* USER CODE BEGIN Variables */

/* USER CODE END Variables */
/* Definitions for default_task */
osThreadId_t default_taskHandle;
const osThreadAttr_t default_task_attributes = {
  .name = "default_task",
  .priority = (osPriority_t) osPriorityNormal,
  .stack_size = 128 * 4
};
/* Definitions for elbow_service */
osThreadId_t elbow_serviceHandle;
const osThreadAttr_t elbow_service_attributes = {
  .name = "elbow_service",
  .priority = (osPriority_t) osPriorityNormal,
  .stack_size = 128 * 4
};
/* Definitions for hb_service */
osThreadId_t hb_serviceHandle;
const osThreadAttr_t hb_service_attributes = {
  .name = "hb_service",
  .priority = (osPriority_t) osPriorityNormal,
  .stack_size = 128 * 4
};
/* Definitions for limitswitch1_queue */
osMessageQueueId_t limitswitch1_queueHandle;
const osMessageQueueAttr_t limitswitch1_queue_attributes = {
  .name = "limitswitch1_queue"
};

/* Private function prototypes -----------------------------------------------*/
/* USER CODE BEGIN FunctionPrototypes */

/* USER CODE END FunctionPrototypes */

void start_default_task(void *argument);
extern void start_elbow_service(void *argument);
extern void start_heartbeat_service(void *argument);

void MX_FREERTOS_Init(void); /* (MISRA C 2004 rule 8.1) */

/**
  * @brief  FreeRTOS initialization
  * @param  None
  * @retval None
  */
void MX_FREERTOS_Init(void) {
  /* USER CODE BEGIN Init */

  /* USER CODE END Init */

  /* USER CODE BEGIN RTOS_MUTEX */
  /* add mutexes, ... */
  /* USER CODE END RTOS_MUTEX */

  /* USER CODE BEGIN RTOS_SEMAPHORES */
  /* add semaphores, ... */
  /* USER CODE END RTOS_SEMAPHORES */

  /* USER CODE BEGIN RTOS_TIMERS */
  /* start timers, add new ones, ... */
  /* USER CODE END RTOS_TIMERS */

  /* Create the queue(s) */
  /* creation of limitswitch1_queue */
  limitswitch1_queueHandle = osMessageQueueNew (1, sizeof(uint16_t), &limitswitch1_queue_attributes);

  /* USER CODE BEGIN RTOS_QUEUES */
  /* add queues, ... */
  init_elbow_service(limitswitch1_queueHandle);
  /* USER CODE END RTOS_QUEUES */

  /* Create the thread(s) */
  /* creation of default_task */
  default_taskHandle = osThreadNew(start_default_task, NULL, &default_task_attributes);

  /* creation of elbow_service */
  elbow_serviceHandle = osThreadNew(start_elbow_service, NULL, &elbow_service_attributes);

  /* creation of hb_service */
  hb_serviceHandle = osThreadNew(start_heartbeat_service, NULL, &hb_service_attributes);

  /* USER CODE BEGIN RTOS_THREADS */
  /* add threads, ... */
  /* USER CODE END RTOS_THREADS */

  /* USER CODE BEGIN RTOS_EVENTS */
  /* add events, ... */
  /* USER CODE END RTOS_EVENTS */

}

/* USER CODE BEGIN Header_start_default_task */
/**
  * @brief  Function implementing the default_task thread.
  * @param  argument: Not used
  * @retval None
  */
/* USER CODE END Header_start_default_task */
void start_default_task(void *argument)
{
  /* init code for USB_Device */
  MX_USB_Device_Init();
  /* USER CODE BEGIN start_default_task */
  /* Infinite loop */
  for(;;)
  {
    osDelay(1);
  }
  /* USER CODE END start_default_task */
}

/* Private application code --------------------------------------------------*/
/* USER CODE BEGIN Application */

/* USER CODE END Application */

