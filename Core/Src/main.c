/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file           : main.c
  * @brief          : Main program body
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
#include "main.h"
#include "usart.h"
#include "gpio.h"

/* Private includes ----------------------------------------------------------*/
/* USER CODE BEGIN Includes */
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include "stepper_motor.h"

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

/* USER CODE BEGIN PV */
StepperMotor_t stepper_motor = {
  .step_gpio_port = GPIOB,
  .step_gpio_pin = GPIO_PIN_1,
  .dir_gpio_port = GPIOB,
  .dir_gpio_pin = GPIO_PIN_2
};

#define RX_BUFFER_SIZE 64
volatile uint8_t rx_buffer[RX_BUFFER_SIZE];
volatile uint16_t rx_index = 0;
volatile bool frame_in_progress = false;

typedef enum {
  ELBOW_STATUS_NEEDS_HOME = 0,
  ELBOW_STATUS_HOMING = 1,
  ELBOW_STATUS_HOME_ERROR = 2,
  ELBOW_STATUS_HOME_SUCCESS = 3,
  ELBOW_STATUS_MOVING = 4,
  ELBOW_STATUS_MOVE_SUCCESS = 5,
  ELBOW_STATUS_MOVE_ERROR = 6
} ElbowStatus_t;

typedef struct {
  uint8_t sw2;
  uint8_t sw3;
  uint8_t elbow_status;
  uint8_t precharge_status;
} StatePacket_t;

static volatile uint8_t sw1_state = 0;
static volatile uint8_t sw2_state = 0;
static volatile uint8_t sw3_state = 0;
static volatile uint8_t elbow_status = ELBOW_STATUS_NEEDS_HOME;
static volatile bool is_homed = false;
static volatile bool homing_in_progress = false;
static const uint8_t precharge_status = 1;
static StatePacket_t last_sent_packet = {255U, 255U, 255U, 255U};

#define STEPPER_FULL_STEPS_PER_REV 200.0f
#define STEPPER_MICROSTEP_DIV 16.0f
#define STEPPER_GEAR_RATIO 7.0f
#define TWO_PI_F 6.28318530718f
#define HALF_PI_F 1.57079632679f
#define ELBOW_MICROSTEPS_PER_REV (STEPPER_FULL_STEPS_PER_REV * STEPPER_MICROSTEP_DIV * STEPPER_GEAR_RATIO)

#define PROFILE_MAX_STEPS_PER_SEC 9600U
#define PROFILE_ACCEL_STEPS_PER_SEC2 24000U
#define HOMING_SPEED_DIVISOR 5U
#define LIMIT_SWITCH_DEBOUNCE_MS 20U

static volatile uint8_t sw1_candidate_state = 0;
static volatile uint8_t sw2_candidate_state = 0;
static volatile uint8_t sw3_candidate_state = 0;
static volatile uint32_t sw1_last_change_tick = 0;
static volatile uint32_t sw2_last_change_tick = 0;
static volatile uint32_t sw3_last_change_tick = 0;
static volatile bool switches_debounce_initialized = false;

/* USER CODE END PV */

/* Private function prototypes -----------------------------------------------*/
void SystemClock_Config(void);
/* USER CODE BEGIN PFP */
void Print_Welcome_Message(void);
void Process_Command(const char* frame);
void UART_SendString(const char* str);
void Refresh_SwitchStates(void);
void SendStatePacketNow(void);
void Maybe_SendStatePacket(void);
void Motion_ServiceCallback(void);
bool Is_HomeFrame(const char* frame);
bool Execute_Homing(void);
bool Parse_MoveFrame(const char* frame, float* radians_out);
int32_t RadiansToMicrosteps(float radians);
float ClampTargetRadians(float radians);
uint8_t ReadSwitchPressed(GPIO_TypeDef* gpio_port, uint16_t gpio_pin);

/* USER CODE END PFP */

/* Private user code ---------------------------------------------------------*/
/* USER CODE BEGIN 0 */

/* USER CODE END 0 */

/**
  * @brief  The application entry point.
  * @retval int
  */
int main(void)
{

  /* USER CODE BEGIN 1 */

  /* USER CODE END 1 */

  /* MCU Configuration--------------------------------------------------------*/

  /* Reset of all peripherals, Initializes the Flash interface and the Systick. */
  HAL_Init();

  /* USER CODE BEGIN Init */

  /* USER CODE END Init */

  /* Configure the system clock */
  SystemClock_Config();

  /* USER CODE BEGIN SysInit */

  /* USER CODE END SysInit */

  /* Initialize all configured peripherals */
  MX_GPIO_Init();
  MX_USART2_UART_Init();
  /* USER CODE BEGIN 2 */
  Stepper_Init(&stepper_motor);
  Stepper_SetServiceCallback(Motion_ServiceCallback);
  Refresh_SwitchStates();
  Print_Welcome_Message();
  Maybe_SendStatePacket();
  /* USER CODE END 2 */

  /* Infinite loop */
  /* USER CODE BEGIN WHILE */
  while (1)
  {
    /* USER CODE END WHILE */

    /* USER CODE BEGIN 3 */
    uint8_t rx_char;
    
    /* Frame parser for packets of format: <M,VALUE> */
    if (HAL_UART_Receive(&huart2, &rx_char, 1, 50) == HAL_OK) {
      if (rx_char == '<') {
        rx_index = 0;
        frame_in_progress = true;
        rx_buffer[rx_index++] = rx_char;
      } else if (frame_in_progress) {
        if (rx_index < RX_BUFFER_SIZE - 1) {
          rx_buffer[rx_index++] = rx_char;

          if (rx_char == '>') {
            rx_buffer[rx_index] = '\0';
            Process_Command((const char*)rx_buffer);
            frame_in_progress = false;
            rx_index = 0;
          }
        } else {
          frame_in_progress = false;
          rx_index = 0;
        }
      }
    }

    Refresh_SwitchStates();
    Maybe_SendStatePacket();
  }
  /* USER CODE END 3 */
}

/**
  * @brief System Clock Configuration
  * @retval None
  */
void SystemClock_Config(void)
{
  RCC_OscInitTypeDef RCC_OscInitStruct = {0};
  RCC_ClkInitTypeDef RCC_ClkInitStruct = {0};

  /** Configure the main internal regulator output voltage
  */
  __HAL_RCC_PWR_CLK_ENABLE();
  __HAL_PWR_VOLTAGESCALING_CONFIG(PWR_REGULATOR_VOLTAGE_SCALE1);

  /** Initializes the RCC Oscillators according to the specified parameters
  * in the RCC_OscInitTypeDef structure.
  */
  RCC_OscInitStruct.OscillatorType = RCC_OSCILLATORTYPE_HSI;
  RCC_OscInitStruct.HSIState = RCC_HSI_ON;
  RCC_OscInitStruct.HSICalibrationValue = RCC_HSICALIBRATION_DEFAULT;
  RCC_OscInitStruct.PLL.PLLState = RCC_PLL_ON;
  RCC_OscInitStruct.PLL.PLLSource = RCC_PLLSOURCE_HSI;
  RCC_OscInitStruct.PLL.PLLM = 16;
  RCC_OscInitStruct.PLL.PLLN = 336;
  RCC_OscInitStruct.PLL.PLLP = RCC_PLLP_DIV4;
  RCC_OscInitStruct.PLL.PLLQ = 4;
  if (HAL_RCC_OscConfig(&RCC_OscInitStruct) != HAL_OK)
  {
    Error_Handler();
  }

  /** Initializes the CPU, AHB and APB buses clocks
  */
  RCC_ClkInitStruct.ClockType = RCC_CLOCKTYPE_HCLK|RCC_CLOCKTYPE_SYSCLK
                              |RCC_CLOCKTYPE_PCLK1|RCC_CLOCKTYPE_PCLK2;
  RCC_ClkInitStruct.SYSCLKSource = RCC_SYSCLKSOURCE_PLLCLK;
  RCC_ClkInitStruct.AHBCLKDivider = RCC_SYSCLK_DIV1;
  RCC_ClkInitStruct.APB1CLKDivider = RCC_HCLK_DIV2;
  RCC_ClkInitStruct.APB2CLKDivider = RCC_HCLK_DIV1;

  if (HAL_RCC_ClockConfig(&RCC_ClkInitStruct, FLASH_LATENCY_2) != HAL_OK)
  {
    Error_Handler();
  }
}

/* USER CODE BEGIN 4 */

/**
 * @brief Send a string over UART
 */
void UART_SendString(const char* str)
{
  if (str == NULL) return;
  HAL_UART_Transmit(&huart2, (uint8_t*)str, strlen(str), 1000);
}

/**
 * @brief Print welcome message
 */
void Print_Welcome_Message(void)
{
  UART_SendString("\r\n");
  UART_SendString("========================================\r\n");
  UART_SendString("  STM32F411 ELBOW MOTION CONTROLLER\r\n");
  UART_SendString("========================================\r\n");
  UART_SendString("RX Commands: <M,VALUE>, <H>\r\n");
  UART_SendString("TX State: <SW2,SW3,ELBOW_STATUS,PRECHARGE_STATUS>\r\n");
  UART_SendString("USART2 @ 115200 baud\r\n\r\n");
}

/**
 * @brief Convert radians to signed microstep target
 */
int32_t RadiansToMicrosteps(float radians)
{
  float scaled = radians * (ELBOW_MICROSTEPS_PER_REV / TWO_PI_F);
  if (scaled >= 0.0f) {
    return (int32_t)(scaled + 0.5f);
  }
  return (int32_t)(scaled - 0.5f);
}

/**
 * @brief Clamp target angle to supported range [0, pi/2]
 */
float ClampTargetRadians(float radians)
{
  if (radians < 0.0f) {
    return 0.0f;
  }
  if (radians > HALF_PI_F) {
    return HALF_PI_F;
  }
  return radians;
}

/**
 * @brief Read a limit switch in normalized pressed/released form
 */
uint8_t ReadSwitchPressed(GPIO_TypeDef* gpio_port, uint16_t gpio_pin)
{
  return (HAL_GPIO_ReadPin(gpio_port, gpio_pin) == GPIO_PIN_RESET) ? 1U : 0U;
}

/**
 * @brief Check if a frame is the homing command <H>
 */
bool Is_HomeFrame(const char* frame)
{
  if (frame == NULL) {
    return false;
  }

  return (strcmp(frame, "<H>") == 0) || (strcmp(frame, "<h>") == 0);
}

/**
 * @brief Parse frame formatted as <M,VALUE>
 */
bool Parse_MoveFrame(const char* frame, float* radians_out)
{
  if (frame == NULL || radians_out == NULL) {
    return false;
  }

  size_t len = strlen(frame);
  if (len < 5 || frame[0] != '<' || frame[len - 1] != '>') {
    return false;
  }

  char payload[RX_BUFFER_SIZE];
  size_t payload_len = len - 2;
  if (payload_len >= sizeof(payload)) {
    return false;
  }
  memcpy(payload, &frame[1], payload_len);
  payload[payload_len] = '\0';

  if (payload[0] != 'M' && payload[0] != 'm') {
    return false;
  }
  if (payload[1] != ',') {
    return false;
  }

  char* end_ptr = NULL;
  float value = strtof(&payload[2], &end_ptr);
  if (end_ptr == &payload[2] || *end_ptr != '\0') {
    return false;
  }

  *radians_out = value;
  return true;
}

/**
 * @brief Read and normalize switch states
 */
void Refresh_SwitchStates(void)
{
  uint32_t now = HAL_GetTick();
  uint8_t sw1_pressed = ReadSwitchPressed(SW1_GPIO_Port, SW1_Pin);
  uint8_t sw2_pressed = ReadSwitchPressed(SW2_GPIO_Port, SW2_Pin);
  uint8_t sw3_pressed = ReadSwitchPressed(SW3_GPIO_Port, SW3_Pin);

  if (!switches_debounce_initialized) {
    sw1_state = sw1_pressed;
    sw2_state = sw2_pressed;
    sw3_state = sw3_pressed;
    sw1_candidate_state = sw1_pressed;
    sw2_candidate_state = sw2_pressed;
    sw3_candidate_state = sw3_pressed;
    sw1_last_change_tick = now;
    sw2_last_change_tick = now;
    sw3_last_change_tick = now;
    switches_debounce_initialized = true;
    return;
  }

  if (sw1_pressed != sw1_candidate_state) {
    sw1_candidate_state = sw1_pressed;
    sw1_last_change_tick = now;
  }

  if (sw2_pressed != sw2_candidate_state) {
    sw2_candidate_state = sw2_pressed;
    sw2_last_change_tick = now;
  }

  if (sw3_pressed != sw3_candidate_state) {
    sw3_candidate_state = sw3_pressed;
    sw3_last_change_tick = now;
  }

  if (sw1_state != sw1_candidate_state && (now - sw1_last_change_tick) >= LIMIT_SWITCH_DEBOUNCE_MS) {
    sw1_state = sw1_candidate_state;
  }

  if (sw2_state != sw2_candidate_state && (now - sw2_last_change_tick) >= LIMIT_SWITCH_DEBOUNCE_MS) {
    sw2_state = sw2_candidate_state;
  }

  if (sw3_state != sw3_candidate_state && (now - sw3_last_change_tick) >= LIMIT_SWITCH_DEBOUNCE_MS) {
    sw3_state = sw3_candidate_state;
  }
}

/**
 * @brief Transmit current state packet unconditionally
 */
void SendStatePacketNow(void)
{
  StatePacket_t current = {
    .sw2 = sw2_state,
    .sw3 = sw3_state,
    .elbow_status = elbow_status,
    .precharge_status = precharge_status
  };

  char packet[48];
  snprintf(packet, sizeof(packet), "<%u,%u,%u,%u>\r\n",
           current.sw2,
           current.sw3,
           current.elbow_status,
           current.precharge_status);
  UART_SendString(packet);
  last_sent_packet = current;
}

/**
 * @brief Transmit state packet if any tracked field changed
 */
void Maybe_SendStatePacket(void)
{
  StatePacket_t current = {
    .sw2 = sw2_state,
    .sw3 = sw3_state,
    .elbow_status = elbow_status,
    .precharge_status = precharge_status
  };

  bool state_changed =
      (current.sw2 != last_sent_packet.sw2) ||
      (current.sw3 != last_sent_packet.sw3) ||
      (current.elbow_status != last_sent_packet.elbow_status) ||
      (current.precharge_status != last_sent_packet.precharge_status);

  if (state_changed) {
    char packet[48];
    snprintf(packet, sizeof(packet), "<%u,%u,%u,%u>\r\n",
             current.sw2,
             current.sw3,
             current.elbow_status,
             current.precharge_status);
    UART_SendString(packet);
    last_sent_packet = current;
  }
}

/**
 * @brief Service callback during motion profile execution
 */
void Motion_ServiceCallback(void)
{
  Refresh_SwitchStates();

  if (homing_in_progress && sw1_state == 1U) {
    Stepper_Stop(&stepper_motor);
  }

  Maybe_SendStatePacket();
}

/**
 * @brief Execute homing: seek SW1 in negative direction, then back off until release
 */
bool Execute_Homing(void)
{
  uint32_t homing_max_speed = PROFILE_MAX_STEPS_PER_SEC / HOMING_SPEED_DIVISOR;
  uint32_t homing_accel = PROFILE_ACCEL_STEPS_PER_SEC2 / HOMING_SPEED_DIVISOR;
  uint32_t homing_travel_limit_steps = (uint32_t)RadiansToMicrosteps(HALF_PI_F);

  if (homing_max_speed == 0U) {
    homing_max_speed = 1U;
  }
  if (homing_accel == 0U) {
    homing_accel = 1U;
  }

  Refresh_SwitchStates();

  if (sw1_state != 1U) {
    homing_in_progress = true;
    int32_t home_seek_target = stepper_motor.current_position_microsteps - (int32_t)homing_travel_limit_steps;
    (void)Stepper_MoveToPositionMicrosteps(&stepper_motor,
                                           home_seek_target,
                                           homing_max_speed,
                                           homing_accel);
    homing_in_progress = false;
    Refresh_SwitchStates();
  }

  if (sw1_state != 1U) {
    return false;
  }

  Stepper_SetDirection(&stepper_motor, STEPPER_DIR_CLOCKWISE);
  Stepper_SetSpeed(&stepper_motor, homing_max_speed);

  uint32_t backoff_steps = 0;
  while (ReadSwitchPressed(SW1_GPIO_Port, SW1_Pin) == 1U && backoff_steps < homing_travel_limit_steps) {
    Stepper_MoveSteps(&stepper_motor, 1);
    backoff_steps++;
  }

  Refresh_SwitchStates();
  if (ReadSwitchPressed(SW1_GPIO_Port, SW1_Pin) == 1U) {
    return false;
  }

  stepper_motor.current_position_microsteps = 0;
  is_homed = true;
  return true;
}

/**
 * @brief Process received command frame
 */
void Process_Command(const char* frame)
{
  if (frame == NULL) {
    return;
  }

  if (stepper_motor.is_moving) {
    return;
  }

  if (Is_HomeFrame(frame)) {
    elbow_status = ELBOW_STATUS_HOMING;
    Maybe_SendStatePacket();

    if (Execute_Homing()) {
      elbow_status = ELBOW_STATUS_HOME_SUCCESS;
    } else {
      is_homed = false;
      elbow_status = ELBOW_STATUS_HOME_ERROR;
    }
    SendStatePacketNow();
    return;
  }

  float target_radians = 0.0f;
  if (!Parse_MoveFrame(frame, &target_radians)) {
    return;
  }

  if (!is_homed) {
    elbow_status = ELBOW_STATUS_NEEDS_HOME;
    SendStatePacketNow();
    return;
  }

  target_radians = ClampTargetRadians(target_radians);

  elbow_status = ELBOW_STATUS_MOVING;
  Maybe_SendStatePacket();

  int32_t target_microsteps = RadiansToMicrosteps(target_radians);
  bool move_ok = Stepper_MoveToPositionMicrosteps(&stepper_motor,
                                                  target_microsteps,
                                                  PROFILE_MAX_STEPS_PER_SEC,
                                                  PROFILE_ACCEL_STEPS_PER_SEC2);

  elbow_status = move_ok ? ELBOW_STATUS_MOVE_SUCCESS : ELBOW_STATUS_MOVE_ERROR;
  Maybe_SendStatePacket();
}

void HAL_GPIO_EXTI_Callback(uint16_t GPIO_Pin)
{
  uint32_t now = HAL_GetTick();

  if (GPIO_Pin == SW1_Pin) {
    sw1_candidate_state = ReadSwitchPressed(SW1_GPIO_Port, SW1_Pin);
    sw1_last_change_tick = now;

    if (homing_in_progress && sw1_candidate_state == 1U) {
      Stepper_Stop(&stepper_motor);
    }
  }

  if (GPIO_Pin == SW2_Pin) {
    sw2_candidate_state = ReadSwitchPressed(SW2_GPIO_Port, SW2_Pin);
    sw2_last_change_tick = now;
  }

  if (GPIO_Pin == SW3_Pin) {
    sw3_candidate_state = ReadSwitchPressed(SW3_GPIO_Port, SW3_Pin);
    sw3_last_change_tick = now;
  }
}

/* USER CODE END 4 */

/**
  * @brief  This function is executed in case of error occurrence.
  * @retval None
  */
void Error_Handler(void)
{
  /* USER CODE BEGIN Error_Handler_Debug */
  /* User can add his own implementation to report the HAL error return state */
  __disable_irq();
  while (1)
  {
  }
  /* USER CODE END Error_Handler_Debug */
}
#ifdef USE_FULL_ASSERT
/**
  * @brief  Reports the name of the source file and the source line number
  *         where the assert_param error has occurred.
  * @param  file: pointer to the source file name
  * @param  line: assert_param error line source number
  * @retval None
  */
void assert_failed(uint8_t *file, uint32_t line)
{
  /* USER CODE BEGIN 6 */
  /* User can add his own implementation to report the file name and line number,
     ex: printf("Wrong parameters value: file %s on line %d\r\n", file, line) */
  /* USER CODE END 6 */
}
#endif /* USE_FULL_ASSERT */
