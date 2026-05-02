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
#include "stdio.h"
/* Private includes ----------------------------------------------------------*/
/* USER CODE BEGIN Includes */
#include <ctype.h>
#include "fonts.h"
#include "ssd1306.h"
#include "string.h"

/* USER CODE END Includes */

/* Private typedef -----------------------------------------------------------*/
/* USER CODE BEGIN PTD */

/* USER CODE END PTD */

/* Private define ------------------------------------------------------------*/
/* USER CODE BEGIN PD */
#define PIR_ACTIVE_LEVEL GPIO_PIN_SET
#define PIR_SAMPLE_INTERVAL_MS 50U
#define PIR_CONFIRM_COUNT 3U
#define MQ2_SAMPLE_INTERVAL_MS 500U
#define DHT_SAMPLE_INTERVAL_MS 2000U
#define TELEMETRY_INTERVAL_MS 1000U
#define OLED_UPDATE_INTERVAL_MS 500U
#define OCCUPANCY_OFF_DELAY_MS 120000U
#define INTRUSION_COOLDOWN_MS 10000U
#define BUZZER_INTRUSION_DURATION_MS 1500U
#define BUZZER_INTRUSION_TOGGLE_MS 200U
#define DOOR_UNLOCK_PULSE_MS 5000U
#define FAN_ON_TEMP_C 28.0f
#define FAN_OFF_TEMP_C 26.0f
#define GAS_THRESHOLD_RAW 2300U
#define UART_EMERGENCY_BYTE 0xFFU

#define RELAY_ACTIVE_LEVEL GPIO_PIN_RESET
#define RELAY_INACTIVE_LEVEL GPIO_PIN_SET

#define FAN_RELAY_PORT GPIOA
#define FAN_RELAY_PIN GPIO_PIN_1
#define LIGHT_RELAY_PORT GPIOA
#define LIGHT_RELAY_PIN GPIO_PIN_2
#define DOOR_RELAY_PORT GPIOA
#define DOOR_RELAY_PIN GPIO_PIN_3

#define BUZZER_PORT GPIOB
#define BUZZER_PIN GPIO_PIN_5

#define FIRE_PORT GPIOB
#define FIRE_PIN GPIO_PIN_1
#define FIRE_ACTIVE_LEVEL GPIO_PIN_RESET

#define PIR_PORT GPIOA
#define PIR_PIN GPIO_PIN_8

/* USER CODE END PD */

/* Private macro -------------------------------------------------------------*/
/* USER CODE BEGIN PM */

/* USER CODE END PM */

/* Private variables ---------------------------------------------------------*/
ADC_HandleTypeDef hadc1;
I2C_HandleTypeDef hi2c1;

TIM_HandleTypeDef htim2;
TIM_HandleTypeDef htim4;
UART_HandleTypeDef huart1;

/* USER CODE BEGIN PV */
static uint32_t pir_last_sample_ms = 0U;
static uint8_t pir_candidate_state = 0U;
static uint8_t pir_candidate_count = 0U;
static uint8_t pir_stable_state = 0U;
static char uart_msg[64]; // Buffer for UART messages
static volatile uint8_t uart_cmd_ready = 0U;
static volatile uint8_t uart_rx_cmd_idx = 0U;
static uint8_t uart_rx_byte = 0U;
static char uart_rx_cmd_buf[24];
static char uart_cmd_line[24];

static uint32_t mq2_value = 0U; // Variable to store MQ2 Gas Sensor data
static float dht_hum = 0.0f;
static float dht_temp = 0.0f;
static int dht_error = 0;
static uint8_t last_pir_state = 0U;
static uint32_t last_mq2_ms = 0U;
static uint32_t last_dht_ms = 0U;
static uint32_t last_telemetry_ms = 0U;
static uint32_t last_oled_ms = 0U;
static uint32_t occupancy_last_motion_ms = 0U;
static uint32_t intrusion_last_alert_ms = 0U;
static uint8_t occupancy_enabled = 1U;
static uint8_t climate_enabled = 1U;
static uint8_t lockdown_enabled = 0U;
static uint8_t emergency_latched = 0U;
static volatile uint8_t fire_irq_triggered = 0U;
static uint8_t light_manual_override = 0U;
static uint8_t fan_manual_override = 0U;
static uint8_t light_state = 0U;
static uint8_t fan_state = 0U;
static uint8_t door_unlocked = 0U;
static uint32_t door_unlock_until_ms = 0U;
static uint8_t buzzer_mode = 0U;
static uint8_t buzzer_on = 0U;
static uint32_t buzzer_toggle_ms = 0U;
static uint32_t buzzer_intrusion_until_ms = 0U;

/* USER CODE END PV */

/* Private function prototypes -----------------------------------------------*/
void SystemClock_Config(void);
static void MX_GPIO_Init(void);
static void MX_I2C1_Init(void);
static void MX_TIM2_Init(void);
static void MX_TIM4_Init(void);
static void MX_ADC1_Init(void);
static void MX_USART1_UART_Init(void);
/* USER CODE BEGIN PFP */
void HAL_TIM_MspPostInit(TIM_HandleTypeDef *htim);
static GPIO_PinState PIR_ReadRawState(void);
static uint8_t PIR_MotionDetected(void);
static uint8_t PIR_UpdateState(void);
static void UART_StartReceiveIT(void);
static void UART_PushReceivedByte(uint8_t ch);
static void Process_UartCommand(void);
static void Relay_SetFan(uint8_t on);
static void Relay_SetLight(uint8_t on);
static void Door_SetUnlocked(uint8_t unlocked);
static void Buzzer_Set(uint8_t on);
static void Buzzer_Update(uint32_t now);
static uint8_t Fire_IsActive(void);
static void UART_SendLine(const char *line);
uint8_t DHT11_Read_Float(float *humidity, float *temperature)
{
  /* ------------------- CONFIGURATION ------------------- */
  GPIO_TypeDef *PORT = GPIOB;        // DHT GPIO port
  uint16_t PIN = GPIO_PIN_0;         // DHT GPIO pin
  TIM_HandleTypeDef *TIMER = &htim2; // Timer used for microsecond delay
  /* ----------------------------------------------------- */

  uint8_t data[5] = {0};
  GPIO_InitTypeDef GPIO_InitStruct;

  // ----- 1. Start signal -----
  GPIO_InitStruct.Pin = PIN;
  GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
  HAL_GPIO_Init(PORT, &GPIO_InitStruct);

  HAL_GPIO_WritePin(PORT, PIN, GPIO_PIN_RESET);
  HAL_Delay(20); // =18ms start pulse
  HAL_GPIO_WritePin(PORT, PIN, GPIO_PIN_SET);

  // Inline 30 us delay
  if (TIMER->State == HAL_TIM_STATE_RESET || TIMER->State == HAL_TIM_STATE_READY)
    HAL_TIM_Base_Start(TIMER);
  __HAL_TIM_SET_COUNTER(TIMER, 0);
  while (__HAL_TIM_GET_COUNTER(TIMER) < 30)
    ;

  // ----- 2. Input mode -----
  GPIO_InitStruct.Mode = GPIO_MODE_INPUT;
  GPIO_InitStruct.Pull = GPIO_PULLUP;
  HAL_GPIO_Init(PORT, &GPIO_InitStruct);

  // ----- 3. Wait for sensor response -----
  uint16_t timeout;

  // Wait for LOW
  timeout = 200;
  while (HAL_GPIO_ReadPin(PORT, PIN) != GPIO_PIN_RESET)
  {
    if (--timeout == 0)
      return 3;
    __HAL_TIM_SET_COUNTER(TIMER, 0);
    while (__HAL_TIM_GET_COUNTER(TIMER) < 1)
      ; // 1 us delay
  }

  // Wait for HIGH
  timeout = 200;
  while (HAL_GPIO_ReadPin(PORT, PIN) != GPIO_PIN_SET)
  {
    if (--timeout == 0)
      return 4;
    __HAL_TIM_SET_COUNTER(TIMER, 0);
    while (__HAL_TIM_GET_COUNTER(TIMER) < 1)
      ; // 1 us delay
  }

  // Wait for LOW again
  timeout = 200;
  while (HAL_GPIO_ReadPin(PORT, PIN) != GPIO_PIN_RESET)
  {
    if (--timeout == 0)
      return 5;
    __HAL_TIM_SET_COUNTER(TIMER, 0);
    while (__HAL_TIM_GET_COUNTER(TIMER) < 1)
      ; // 1 us delay
  }

  // ----- 4. Read 40 bits -----
  for (int i = 0; i < 40; i++)
  {
    // Wait for HIGH (start of bit)
    timeout = 200;
    while (HAL_GPIO_ReadPin(PORT, PIN) != GPIO_PIN_SET)
    {
      if (--timeout == 0)
        return 6;
      __HAL_TIM_SET_COUNTER(TIMER, 0);
      while (__HAL_TIM_GET_COUNTER(TIMER) < 1)
        ; // 1 us delay
    }

    // Sample midpoint (≈40 us)
    __HAL_TIM_SET_COUNTER(TIMER, 0);
    while (__HAL_TIM_GET_COUNTER(TIMER) < 40)
      ;

    data[i / 8] <<= 1;
    if (HAL_GPIO_ReadPin(PORT, PIN) == GPIO_PIN_SET)
      data[i / 8] |= 1;

    // Wait for LOW (end of bit)
    timeout = 200;
    while (HAL_GPIO_ReadPin(PORT, PIN) != GPIO_PIN_RESET)
    {
      if (--timeout == 0)
        return 7;
      __HAL_TIM_SET_COUNTER(TIMER, 0);
      while (__HAL_TIM_GET_COUNTER(TIMER) < 1)
        ; // 1 us delay
    }
  }

  // ----- 5. Checksum -----
  if (data[4] != (uint8_t)(data[0] + data[1] + data[2] + data[3]))
    return 2;

  // ----- 6. Return float values -----
  *humidity = (float)data[0] + ((float)data[1] / 10.0f);
  *temperature = (float)data[2] + ((float)data[3] / 10.0f);

  return 0;
}

static GPIO_PinState PIR_ReadRawState(void)
{
  return HAL_GPIO_ReadPin(PIR_PORT, PIR_PIN);
}

static uint8_t PIR_MotionDetected(void)
{
  return (PIR_ReadRawState() == PIR_ACTIVE_LEVEL) ? 1U : 0U;
}

static uint8_t PIR_UpdateState(void)
{
  uint32_t now = HAL_GetTick();
  uint8_t raw_state;

  if ((now - pir_last_sample_ms) < PIR_SAMPLE_INTERVAL_MS)
  {
    return pir_stable_state;
  }

  pir_last_sample_ms = now;
  raw_state = PIR_MotionDetected();

  if (raw_state == pir_candidate_state)
  {
    if (pir_candidate_count < PIR_CONFIRM_COUNT)
    {
      pir_candidate_count++;
    }
  }
  else
  {
    pir_candidate_state = raw_state;
    pir_candidate_count = 1U;
  }

  if (pir_candidate_count >= PIR_CONFIRM_COUNT)
  {
    pir_stable_state = pir_candidate_state;
  }

  return pir_stable_state;
}

static void NormalizeCommand(char *cmd)
{
  size_t start = 0U;
  size_t end;
  size_t len;
  size_t i;

  while ((cmd[start] != '\0') && isspace((unsigned char)cmd[start]))
  {
    start++;
  }

  if (cmd[start] == '/')
  {
    start++;
  }

  while ((cmd[start] != '\0') && isspace((unsigned char)cmd[start]))
  {
    start++;
  }

  end = start;
  while (cmd[end] != '\0')
  {
    end++;
  }

  while ((end > start) && isspace((unsigned char)cmd[end - 1U]))
  {
    end--;
  }

  len = end - start;
  if ((start > 0U) && (len > 0U))
  {
    memmove(cmd, &cmd[start], len);
  }
  cmd[len] = '\0';

  for (i = 0U; i < len; i++)
  {
    cmd[i] = (char)toupper((unsigned char)cmd[i]);
  }
}

// Lock state for OLED feedback
static volatile int lock_state = -1; // -1: unknown, 0: locked, 1: unlocked
static volatile uint32_t lock_state_tick = 0;

static void UART_StartReceiveIT(void)
{
  uart_rx_cmd_idx = 0U;
  uart_cmd_ready = 0U;
  (void)HAL_UART_Receive_IT(&huart1, &uart_rx_byte, 1U);
}

static void UART_PushReceivedByte(uint8_t ch)
{
  if (ch == '\r')
  {
    return;
  }

  if (uart_cmd_ready != 0U)
  {
    return;
  }

  if (ch == '\n')
  {
    if (uart_rx_cmd_idx > 0U)
    {
      uart_rx_cmd_buf[uart_rx_cmd_idx] = '\0';
      memcpy(uart_cmd_line, uart_rx_cmd_buf, sizeof(uart_cmd_line));
      uart_cmd_ready = 1U;
    }
    uart_rx_cmd_idx = 0U;
    return;
  }

  if (uart_rx_cmd_idx < (sizeof(uart_rx_cmd_buf) - 1U))
  {
    uart_rx_cmd_buf[uart_rx_cmd_idx++] = (char)ch;
  }
  else
  {
    uart_rx_cmd_idx = 0U;
  }
}

void HAL_UART_RxCpltCallback(UART_HandleTypeDef *huart)
{
  if (huart->Instance == USART1)
  {
    UART_PushReceivedByte(uart_rx_byte);
    (void)HAL_UART_Receive_IT(&huart1, &uart_rx_byte, 1U);
  }
}

void HAL_UART_ErrorCallback(UART_HandleTypeDef *huart)
{
  if (huart->Instance == USART1)
  {
    __HAL_UART_CLEAR_OREFLAG(&huart1);
    (void)HAL_UART_Receive_IT(&huart1, &uart_rx_byte, 1U);
  }
}

/* USER CODE END PFP */

/* Private user code ---------------------------------------------------------*/
/* USER CODE BEGIN 0 */
static void UART_SendLine(const char *line)
{
  HAL_UART_Transmit(&huart1, (uint8_t *)line, (uint16_t)strlen(line), HAL_MAX_DELAY);
}

static void Relay_SetFan(uint8_t on)
{
  fan_state = on;
  HAL_GPIO_WritePin(FAN_RELAY_PORT, FAN_RELAY_PIN, on ? RELAY_ACTIVE_LEVEL : RELAY_INACTIVE_LEVEL);
}

static void Relay_SetLight(uint8_t on)
{
  light_state = on;
  HAL_GPIO_WritePin(LIGHT_RELAY_PORT, LIGHT_RELAY_PIN, on ? RELAY_ACTIVE_LEVEL : RELAY_INACTIVE_LEVEL);
}

static void Door_SetUnlocked(uint8_t unlocked)
{
  door_unlocked = unlocked;
  HAL_GPIO_WritePin(DOOR_RELAY_PORT, DOOR_RELAY_PIN, unlocked ? RELAY_ACTIVE_LEVEL : RELAY_INACTIVE_LEVEL);
}

static void Buzzer_Set(uint8_t on)
{
  buzzer_on = on;
  HAL_GPIO_WritePin(BUZZER_PORT, BUZZER_PIN, on ? GPIO_PIN_SET : GPIO_PIN_RESET);
}

static void Buzzer_Update(uint32_t now)
{
  if (buzzer_mode == 2U)
  {
    if (buzzer_on == 0U)
    {
      Buzzer_Set(1U);
    }
    return;
  }

  if (buzzer_mode == 1U)
  {
    if (now >= buzzer_intrusion_until_ms)
    {
      buzzer_mode = 0U;
      Buzzer_Set(0U);
      return;
    }

    if ((now - buzzer_toggle_ms) >= BUZZER_INTRUSION_TOGGLE_MS)
    {
      buzzer_toggle_ms = now;
      Buzzer_Set(buzzer_on == 0U ? 1U : 0U);
    }
    return;
  }

  if (buzzer_on != 0U)
  {
    Buzzer_Set(0U);
  }
}

static uint8_t Fire_IsActive(void)
{
  return (HAL_GPIO_ReadPin(FIRE_PORT, FIRE_PIN) == FIRE_ACTIVE_LEVEL) ? 1U : 0U;
}

static void Process_UartCommand(void)
{
  char cmd_buf[24];
  uint32_t now;

  if (uart_cmd_ready == 0U)
  {
    return;
  }

  __disable_irq();
  strncpy(cmd_buf, uart_cmd_line, sizeof(cmd_buf) - 1U);
  cmd_buf[sizeof(cmd_buf) - 1U] = '\0';
  uart_cmd_ready = 0U;
  __enable_irq();

  NormalizeCommand(cmd_buf);

  if (cmd_buf[0] == '\0')
  {
    return;
  }

  now = HAL_GetTick();

  if ((strcmp(cmd_buf, "LOCKDOWN_ON") == 0) || (strcmp(cmd_buf, "LOCKDOWNON") == 0))
  {
    lockdown_enabled = 1U;
    door_unlock_until_ms = 0U;
    UART_SendLine("LOCKDOWN=ON\r\n");
    return;
  }

  if ((strcmp(cmd_buf, "LOCKDOWN_OFF") == 0) || (strcmp(cmd_buf, "LOCKDOWNOFF") == 0))
  {
    lockdown_enabled = 0U;
    UART_SendLine("LOCKDOWN=OFF\r\n");
    return;
  }

  if ((strcmp(cmd_buf, "OCCUPANCY_ON") == 0) || (strcmp(cmd_buf, "OCCUPANCYON") == 0))
  {
    occupancy_enabled = 1U;
    light_manual_override = 0U;
    occupancy_last_motion_ms = now;
    UART_SendLine("OCCUPANCY=ON\r\n");
    return;
  }

  if ((strcmp(cmd_buf, "OCCUPANCY_OFF") == 0) || (strcmp(cmd_buf, "OCCUPANCYOFF") == 0))
  {
    occupancy_enabled = 0U;
    UART_SendLine("OCCUPANCY=OFF\r\n");
    return;
  }

  if ((strcmp(cmd_buf, "CLIMATE_ON") == 0) || (strcmp(cmd_buf, "CLIMATEON") == 0))
  {
    climate_enabled = 1U;
    fan_manual_override = 0U;
    UART_SendLine("CLIMATE=ON\r\n");
    return;
  }

  if ((strcmp(cmd_buf, "CLIMATE_OFF") == 0) || (strcmp(cmd_buf, "CLIMATEOFF") == 0))
  {
    climate_enabled = 0U;
    UART_SendLine("CLIMATE=OFF\r\n");
    return;
  }

  if (strcmp(cmd_buf, "CLEARALARM") == 0)
  {
    if ((emergency_latched == 0U) || ((Fire_IsActive() == 0U) && (mq2_value < GAS_THRESHOLD_RAW)))
    {
      emergency_latched = 0U;
      UART_SendLine("ALARM=CLEARED\r\n");
    }
    else
    {
      UART_SendLine("ALARM=ACTIVE\r\n");
    }
    return;
  }

  if (strcmp(cmd_buf, "LOCK") == 0)
  {
    lock_state = 0;
    lock_state_tick = now;
    UART_SendLine("LOCKED\r\n");
    return;
  }

  if ((strcmp(cmd_buf, "UNLOCK") == 0) || (strcmp(cmd_buf, "UNLOAD") == 0))
  {
    lock_state = 1;
    lock_state_tick = now;
    UART_SendLine("UNLOCKED\r\n");
    return;
  }

  if (strcmp(cmd_buf, "FANON") == 0)
  {
    fan_manual_override = 1U;
    Relay_SetFan(1U);
    UART_SendLine("FAN IS ON\r\n");
    return;
  }

  if (strcmp(cmd_buf, "FANOFF") == 0)
  {
    fan_manual_override = 1U;
    Relay_SetFan(0U);
    UART_SendLine("FAN IS OFF\r\n");
    return;
  }

  if (strcmp(cmd_buf, "LIGHTON") == 0)
  {
    light_manual_override = 1U;
    Relay_SetLight(1U);
    UART_SendLine("LIGHT IS ON\r\n");
    return;
  }

  if (strcmp(cmd_buf, "LIGHTOFF") == 0)
  {
    light_manual_override = 1U;
    Relay_SetLight(0U);
    UART_SendLine("LIGHT IS OFF\r\n");
    return;
  }

  if (strcmp(cmd_buf, "DOORUNLOCK") == 0)
  {
    door_unlock_until_ms = now + DOOR_UNLOCK_PULSE_MS;
    UART_SendLine("DOOR UNLOCKED FOR 5 Seconds\r\n");
  }
}

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
  MX_ADC1_Init(); // Initialize ADC for MQ2
  MX_I2C1_Init();
  MX_TIM2_Init();
  MX_USART1_UART_Init();
  UART_StartReceiveIT();
  SSD1306_Init();
  lock_state = 0;
  lock_state_tick = HAL_GetTick();

  /* USER CODE BEGIN 2 */
  pir_stable_state = PIR_MotionDetected();
  pir_candidate_state = pir_stable_state;
  pir_candidate_count = 0U;
  pir_last_sample_ms = HAL_GetTick();
  last_pir_state = pir_stable_state;
  occupancy_last_motion_ms = HAL_GetTick();
  last_mq2_ms = HAL_GetTick();
  last_dht_ms = HAL_GetTick();
  last_telemetry_ms = HAL_GetTick();
  last_oled_ms = HAL_GetTick();
  Relay_SetFan(0U);
  Relay_SetLight(0U);
  Door_SetUnlocked(0U);
  Buzzer_Set(0U);
  /* USER CODE END 2 */

  /* Infinite loop */
  /* USER CODE BEGIN WHILE */
  while (1)
  {
    uint32_t now;
    uint8_t pir_state;
    uint8_t fire_active;
    int uart_len;
    uint8_t show_lock_popup;
    const char *mode_label;

    now = HAL_GetTick();

    Process_UartCommand();

    if ((now - last_mq2_ms) >= MQ2_SAMPLE_INTERVAL_MS)
    {
      HAL_ADC_Start(&hadc1);
      HAL_ADC_PollForConversion(&hadc1, 10);
      mq2_value = HAL_ADC_GetValue(&hadc1);
      HAL_ADC_Stop(&hadc1);
      last_mq2_ms = now;
    }

    if ((now - last_dht_ms) >= DHT_SAMPLE_INTERVAL_MS)
    {
      dht_error = DHT11_Read_Float(&dht_hum, &dht_temp);
      last_dht_ms = now;
    }

    fire_active = Fire_IsActive();
    if (fire_irq_triggered != 0U)
    {
      fire_irq_triggered = 0U;
      fire_active = 1U;
    }

    pir_state = PIR_UpdateState();
    if (pir_state && !last_pir_state)
    {
      UART_SendLine("PIR=MOTION\r\n");
    }

    if (pir_state != 0U)
    {
      occupancy_last_motion_ms = now;
    }

    if ((lockdown_enabled != 0U) && (emergency_latched == 0U) && pir_state && !last_pir_state)
    {
      if ((now - intrusion_last_alert_ms) >= INTRUSION_COOLDOWN_MS)
      {
        intrusion_last_alert_ms = now;
        buzzer_mode = 1U;
        buzzer_intrusion_until_ms = now + BUZZER_INTRUSION_DURATION_MS;
        buzzer_toggle_ms = now;
        Buzzer_Set(1U);
        UART_SendLine("ALARM=INTRUSION\r\n");
      }
    }

    last_pir_state = pir_state;

    if (((fire_active != 0U) || (mq2_value >= GAS_THRESHOLD_RAW)) && (emergency_latched == 0U))
    {
      uint8_t alert = UART_EMERGENCY_BYTE;
      emergency_latched = 1U;
      HAL_UART_Transmit(&huart1, &alert, 1U, HAL_MAX_DELAY);
      UART_SendLine((fire_active != 0U) ? "ALARM=FIRE\r\n" : "ALARM=GAS\r\n");
    }

    if (emergency_latched != 0U)
    {
      Relay_SetFan(0U);
      Relay_SetLight(1U);
      Door_SetUnlocked(1U);
      buzzer_mode = 2U;
    }
    else if (lockdown_enabled != 0U)
    {
      Relay_SetFan(0U);
      Relay_SetLight(0U);
      Door_SetUnlocked(0U);
      if (buzzer_mode == 2U)
      {
        buzzer_mode = 0U;
      }
    }
    else
    {
      if (buzzer_mode == 2U)
      {
        buzzer_mode = 0U;
      }

      if ((door_unlock_until_ms != 0U) && (now >= door_unlock_until_ms))
      {
        door_unlock_until_ms = 0U;
      }

      if (door_unlock_until_ms != 0U)
      {
        Door_SetUnlocked(1U);
      }
      else
      {
        Door_SetUnlocked((lock_state == 1) ? 1U : 0U);
      }

      if ((occupancy_enabled != 0U) && (light_manual_override == 0U))
      {
        if (pir_state != 0U)
        {
          Relay_SetLight(1U);
        }
        else if ((now - occupancy_last_motion_ms) >= OCCUPANCY_OFF_DELAY_MS)
        {
          Relay_SetLight(0U);
        }
      }

      if ((climate_enabled != 0U) && (fan_manual_override == 0U) && (dht_error == 0))
      {
        if (dht_temp >= FAN_ON_TEMP_C)
        {
          Relay_SetFan(1U);
        }
        else if (dht_temp <= FAN_OFF_TEMP_C)
        {
          Relay_SetFan(0U);
        }
      }
    }

    Buzzer_Update(now);

    if ((now - last_oled_ms) >= OLED_UPDATE_INTERVAL_MS)
    {
      char line[40];

      show_lock_popup = ((lock_state != -1) && ((now - lock_state_tick) < 1200U)) ? 1U : 0U;

      if (emergency_latched != 0U)
      {
        mode_label = "EMERG";
      }
      else if (lockdown_enabled != 0U)
      {
        mode_label = "LOCK";
      }
      else if ((occupancy_enabled != 0U) || (climate_enabled != 0U))
      {
        mode_label = "AUTO";
      }
      else
      {
        mode_label = "MAN";
      }

      SSD1306_Fill(0);
      if (show_lock_popup != 0U)
      {
        SSD1306_GotoXY(0, 0);
        SSD1306_Puts("Cmd OK", &Font_7x10, 1);
        SSD1306_GotoXY(0, 13);
        if (dht_error == 0)
        {
          snprintf(line, sizeof(line), "Temp: %.2f C", dht_temp);
        }
        else
        {
          snprintf(line, sizeof(line), "DHT Err: %d", dht_error);
        }
        SSD1306_Puts(line, &Font_7x10, 1);
      }
      else if (dht_error == 0)
      {
        SSD1306_GotoXY(0, 0);
        snprintf(line, sizeof(line), "Temp: %.2f C", dht_temp);
        SSD1306_Puts(line, &Font_7x10, 1);

        SSD1306_GotoXY(0, 13);
        snprintf(line, sizeof(line), "Hum : %.2f %%", dht_hum);
        SSD1306_Puts(line, &Font_7x10, 1);
      }
      else
      {
        SSD1306_GotoXY(0, 0);
        SSD1306_Puts("DHT Error", &Font_7x10, 1);

        SSD1306_GotoXY(0, 13);
        snprintf(line, sizeof(line), "Code: %d", dht_error);
        SSD1306_Puts(line, &Font_7x10, 1);
      }

      SSD1306_GotoXY(0, 26);
      snprintf(line, sizeof(line), "Motion: %s", pir_state ? "YES" : "NO");
      SSD1306_Puts(line, &Font_7x10, 1);

      SSD1306_GotoXY(0, 39);
      snprintf(line, sizeof(line), "Gas : %lu", (unsigned long)mq2_value);
      SSD1306_Puts(line, &Font_7x10, 1);

      SSD1306_GotoXY(0, 52);
      snprintf(line, sizeof(line), "Mode: %s", mode_label);
      SSD1306_Puts(line, &Font_7x10, 1);

      SSD1306_UpdateScreen();
      last_oled_ms = now;
    }

    if ((now - last_telemetry_ms) >= TELEMETRY_INTERVAL_MS)
    {
      uart_len = snprintf(uart_msg, sizeof(uart_msg),
              "T=%.2f,H=%.2f,M=%u,G=%lu,E=%u,L=%u,O=%u,C=%u\r\n",
              dht_temp,
              dht_hum,
              (unsigned int)pir_state,
              (unsigned long)mq2_value,
              (unsigned int)emergency_latched,
              (unsigned int)lockdown_enabled,
              (unsigned int)occupancy_enabled,
              (unsigned int)climate_enabled);
      if (uart_len > 0)
      {
        HAL_UART_Transmit(&huart1, (uint8_t *)uart_msg, (uint16_t)uart_len, HAL_MAX_DELAY);
      }
      last_telemetry_ms = now;
    }

    /* USER CODE END WHILE */

    /* USER CODE BEGIN 3 */
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

  /** Initializes the RCC Oscillators according to the specified parameters
   * in the RCC_OscInitTypeDef structure.
   */
  RCC_OscInitStruct.OscillatorType = RCC_OSCILLATORTYPE_HSI;
  RCC_OscInitStruct.HSIState = RCC_HSI_ON;
  RCC_OscInitStruct.HSICalibrationValue = RCC_HSICALIBRATION_DEFAULT;
  RCC_OscInitStruct.PLL.PLLState = RCC_PLL_NONE;
  if (HAL_RCC_OscConfig(&RCC_OscInitStruct) != HAL_OK)
  {
    Error_Handler();
  }

  /** Initializes the CPU, AHB and APB buses clocks
   */
  RCC_ClkInitStruct.ClockType = RCC_CLOCKTYPE_HCLK | RCC_CLOCKTYPE_SYSCLK | RCC_CLOCKTYPE_PCLK1 | RCC_CLOCKTYPE_PCLK2;
  RCC_ClkInitStruct.SYSCLKSource = RCC_SYSCLKSOURCE_HSI;
  RCC_ClkInitStruct.AHBCLKDivider = RCC_SYSCLK_DIV1;
  RCC_ClkInitStruct.APB1CLKDivider = RCC_HCLK_DIV1;
  RCC_ClkInitStruct.APB2CLKDivider = RCC_HCLK_DIV1;

  if (HAL_RCC_ClockConfig(&RCC_ClkInitStruct, FLASH_LATENCY_0) != HAL_OK)
  {
    Error_Handler();
  }
}

/**
 * @brief ADC1 Initialization Function
 * @param None
 * @retval None
 */
static void MX_ADC1_Init(void)
{
  ADC_ChannelConfTypeDef sConfig = {0};

  hadc1.Instance = ADC1;
  hadc1.Init.ScanConvMode = ADC_SCAN_DISABLE;
  hadc1.Init.ContinuousConvMode = DISABLE;
  hadc1.Init.DiscontinuousConvMode = DISABLE;
  hadc1.Init.ExternalTrigConv = ADC_SOFTWARE_START;
  hadc1.Init.DataAlign = ADC_DATAALIGN_RIGHT;
  hadc1.Init.NbrOfConversion = 1;
  if (HAL_ADC_Init(&hadc1) != HAL_OK)
  {
    Error_Handler();
  }

  /** Configure Regular Channel */
  sConfig.Channel = ADC_CHANNEL_0; // PA0
  sConfig.Rank = ADC_REGULAR_RANK_1;
  sConfig.SamplingTime = ADC_SAMPLETIME_71CYCLES_5;
  if (HAL_ADC_ConfigChannel(&hadc1, &sConfig) != HAL_OK)
  {
    Error_Handler();
  }
}

/**
 * @brief ADC MSP Initialization
 * This function configures the hardware resources used in this example
 * @param hadc: ADC handle pointer
 * @retval None
 */
void HAL_ADC_MspInit(ADC_HandleTypeDef* hadc)
{
  GPIO_InitTypeDef GPIO_InitStruct = {0};
  if(hadc->Instance==ADC1)
  {
    /* Peripheral clock enable */
    __HAL_RCC_ADC1_CLK_ENABLE();
    __HAL_RCC_GPIOA_CLK_ENABLE();
  
    /**ADC1 GPIO Configuration    
    PA0-WKUP     ------> ADC1_IN0 
    */
    GPIO_InitStruct.Pin = GPIO_PIN_0;
    GPIO_InitStruct.Mode = GPIO_MODE_ANALOG;
    HAL_GPIO_Init(GPIOA, &GPIO_InitStruct);
  }
}

/**
 * @brief I2C1 Initialization Function
 * @param None
 * @retval None
 */
static void MX_I2C1_Init(void)
{

  /* USER CODE BEGIN I2C1_Init 0 */

  /* USER CODE END I2C1_Init 0 */

  /* USER CODE BEGIN I2C1_Init 1 */

  /* USER CODE END I2C1_Init 1 */
  hi2c1.Instance = I2C1;
  hi2c1.Init.ClockSpeed = 400000;
  hi2c1.Init.DutyCycle = I2C_DUTYCYCLE_2;
  hi2c1.Init.OwnAddress1 = 0;
  hi2c1.Init.AddressingMode = I2C_ADDRESSINGMODE_7BIT;
  hi2c1.Init.DualAddressMode = I2C_DUALADDRESS_DISABLE;
  hi2c1.Init.OwnAddress2 = 0;
  hi2c1.Init.GeneralCallMode = I2C_GENERALCALL_DISABLE;
  hi2c1.Init.NoStretchMode = I2C_NOSTRETCH_DISABLE;
  if (HAL_I2C_Init(&hi2c1) != HAL_OK)
  {
    Error_Handler();
  }
  /* USER CODE BEGIN I2C1_Init 2 */

  /* USER CODE END I2C1_Init 2 */
}

/**
 * @brief TIM2 Initialization Function
 * @param None
 * @retval None
 */
static void MX_TIM2_Init(void)
{

  /* USER CODE BEGIN TIM2_Init 0 */

  /* USER CODE END TIM2_Init 0 */

  TIM_ClockConfigTypeDef sClockSourceConfig = {0};
  TIM_MasterConfigTypeDef sMasterConfig = {0};

  /* USER CODE BEGIN TIM2_Init 1 */

  /* USER CODE END TIM2_Init 1 */
  htim2.Instance = TIM2;
  htim2.Init.Prescaler = 7;
  htim2.Init.CounterMode = TIM_COUNTERMODE_UP;
  htim2.Init.Period = 65535;
  htim2.Init.ClockDivision = TIM_CLOCKDIVISION_DIV1;
  htim2.Init.AutoReloadPreload = TIM_AUTORELOAD_PRELOAD_DISABLE;
  if (HAL_TIM_Base_Init(&htim2) != HAL_OK)
  {
    Error_Handler();
  }
  sClockSourceConfig.ClockSource = TIM_CLOCKSOURCE_INTERNAL;
  if (HAL_TIM_ConfigClockSource(&htim2, &sClockSourceConfig) != HAL_OK)
  {
    Error_Handler();
  }
  sMasterConfig.MasterOutputTrigger = TIM_TRGO_RESET;
  sMasterConfig.MasterSlaveMode = TIM_MASTERSLAVEMODE_DISABLE;
  if (HAL_TIMEx_MasterConfigSynchronization(&htim2, &sMasterConfig) != HAL_OK)
  {
    Error_Handler();
  }
  /* USER CODE BEGIN TIM2_Init 2 */

  /* USER CODE END TIM2_Init 2 */
}

/**
 * @brief TIM4 Initialization Function
 * @param None
 * @retval None
 */
static void MX_TIM4_Init(void)
{

  /* USER CODE BEGIN TIM4_Init 0 */

  /* USER CODE END TIM4_Init 0 */

  TIM_ClockConfigTypeDef sClockSourceConfig = {0};
  TIM_MasterConfigTypeDef sMasterConfig = {0};
  TIM_OC_InitTypeDef sConfigOC = {0};

  /* USER CODE BEGIN TIM4_Init 1 */

  /* USER CODE END TIM4_Init 1 */
  htim4.Instance = TIM4;
  htim4.Init.Prescaler = 7;
  htim4.Init.CounterMode = TIM_COUNTERMODE_UP;
  htim4.Init.Period = 19999;
  htim4.Init.ClockDivision = TIM_CLOCKDIVISION_DIV1;
  htim4.Init.AutoReloadPreload = TIM_AUTORELOAD_PRELOAD_DISABLE;
  if (HAL_TIM_Base_Init(&htim4) != HAL_OK)
  {
    Error_Handler();
  }
  sClockSourceConfig.ClockSource = TIM_CLOCKSOURCE_INTERNAL;
  if (HAL_TIM_ConfigClockSource(&htim4, &sClockSourceConfig) != HAL_OK)
  {
    Error_Handler();
  }
  if (HAL_TIM_PWM_Init(&htim4) != HAL_OK)
  {
    Error_Handler();
  }
  sMasterConfig.MasterOutputTrigger = TIM_TRGO_RESET;
  sMasterConfig.MasterSlaveMode = TIM_MASTERSLAVEMODE_DISABLE;
  if (HAL_TIMEx_MasterConfigSynchronization(&htim4, &sMasterConfig) != HAL_OK)
  {
    Error_Handler();
  }
  sConfigOC.OCMode = TIM_OCMODE_PWM1;
  sConfigOC.Pulse = 0;
  sConfigOC.OCPolarity = TIM_OCPOLARITY_HIGH;
  sConfigOC.OCFastMode = TIM_OCFAST_DISABLE;
  if (HAL_TIM_PWM_ConfigChannel(&htim4, &sConfigOC, TIM_CHANNEL_4) != HAL_OK)
  {
    Error_Handler();
  }
  HAL_TIM_MspPostInit(&htim4);
  /* USER CODE BEGIN TIM4_Init 2 */

  /* USER CODE END TIM4_Init 2 */
}

/**
 * @brief USART1 Initialization Function
 * @param None
 * @retval None
 */
static void MX_USART1_UART_Init(void)
{

  /* USER CODE BEGIN USART1_Init 0 */

  /* USER CODE END USART1_Init 0 */

  /* USER CODE BEGIN USART1_Init 1 */

  /* USER CODE END USART1_Init 1 */
  huart1.Instance = USART1;
  huart1.Init.BaudRate = 9600;
  huart1.Init.WordLength = UART_WORDLENGTH_8B;
  huart1.Init.StopBits = UART_STOPBITS_1;
  huart1.Init.Parity = UART_PARITY_NONE;
  huart1.Init.Mode = UART_MODE_TX_RX;
  huart1.Init.HwFlowCtl = UART_HWCONTROL_NONE;
  huart1.Init.OverSampling = UART_OVERSAMPLING_16;
  if (HAL_UART_Init(&huart1) != HAL_OK)
  {
    Error_Handler();
  }
  /* USER CODE BEGIN USART1_Init 2 */

  /* USER CODE END USART1_Init 2 */
}

/**
 * @brief GPIO Initialization Function
 * @param None
 * @retval None
 */
static void MX_GPIO_Init(void)
{
  GPIO_InitTypeDef GPIO_InitStruct = {0};
  /* USER CODE BEGIN MX_GPIO_Init_1 */

  /* USER CODE END MX_GPIO_Init_1 */

  /* GPIO Ports Clock Enable */
  __HAL_RCC_GPIOA_CLK_ENABLE();
  __HAL_RCC_GPIOB_CLK_ENABLE();

  /*Configure GPIO pin Output Level */
  HAL_GPIO_WritePin(GPIOB, GPIO_PIN_0 | BUZZER_PIN, GPIO_PIN_RESET);
  HAL_GPIO_WritePin(GPIOA, FAN_RELAY_PIN | LIGHT_RELAY_PIN | DOOR_RELAY_PIN, RELAY_INACTIVE_LEVEL);

  /*Configure GPIO pin : PA8 */
  GPIO_InitStruct.Pin = PIR_PIN;
  GPIO_InitStruct.Mode = GPIO_MODE_INPUT;
  GPIO_InitStruct.Pull = GPIO_PULLDOWN;
  HAL_GPIO_Init(PIR_PORT, &GPIO_InitStruct);

  /*Configure GPIO pin : PB0 */
  GPIO_InitStruct.Pin = GPIO_PIN_0;
  GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
  HAL_GPIO_Init(GPIOB, &GPIO_InitStruct);

  /* Configure GPIO pin : PA1 for Relay/Fan */
  GPIO_InitStruct.Pin = FAN_RELAY_PIN;
  GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
  HAL_GPIO_Init(FAN_RELAY_PORT, &GPIO_InitStruct);

  /* Optional: Set initial state to OFF */
  // For Active Low Relay: SET = OFF
  HAL_GPIO_WritePin(FAN_RELAY_PORT, FAN_RELAY_PIN, RELAY_INACTIVE_LEVEL);
  
  GPIO_InitStruct.Pin = LIGHT_RELAY_PIN;
  GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
  HAL_GPIO_Init(LIGHT_RELAY_PORT, &GPIO_InitStruct);

  /* Optional: Set initial state to OFF */
  // For Active Low Relay: SET = OFF
  HAL_GPIO_WritePin(LIGHT_RELAY_PORT, LIGHT_RELAY_PIN, RELAY_INACTIVE_LEVEL);

  GPIO_InitStruct.Pin = DOOR_RELAY_PIN;
  GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
  HAL_GPIO_Init(DOOR_RELAY_PORT, &GPIO_InitStruct);

  /* Optional: Set initial state to OFF */
  // For Active Low Relay: SET = OFF
  HAL_GPIO_WritePin(DOOR_RELAY_PORT, DOOR_RELAY_PIN, RELAY_INACTIVE_LEVEL);

  /* Configure GPIO pin : PB1 for IR Fire Sensor (EXTI) */
  GPIO_InitStruct.Pin = FIRE_PIN;
  GPIO_InitStruct.Mode = GPIO_MODE_IT_FALLING;
  GPIO_InitStruct.Pull = GPIO_PULLUP;
  HAL_GPIO_Init(FIRE_PORT, &GPIO_InitStruct);

  /* Configure GPIO pin : PB5 for Buzzer */
  GPIO_InitStruct.Pin = BUZZER_PIN;
  GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
  HAL_GPIO_Init(BUZZER_PORT, &GPIO_InitStruct);

  HAL_NVIC_SetPriority(EXTI1_IRQn, 0, 0);
  HAL_NVIC_EnableIRQ(EXTI1_IRQn);

  /* USER CODE BEGIN MX_GPIO_Init_2 */

  /* USER CODE END MX_GPIO_Init_2 */
}

/* USER CODE BEGIN 4 */
void HAL_GPIO_EXTI_Callback(uint16_t GPIO_Pin)
{
  if (GPIO_Pin == FIRE_PIN)
  {
    fire_irq_triggered = 1U;
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