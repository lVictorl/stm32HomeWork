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

/* Private includes ----------------------------------------------------------*/
/* USER CODE BEGIN Includes */
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <ctype.h>
/* USER CODE END Includes */

/* Private typedef -----------------------------------------------------------*/
/* USER CODE BEGIN PTD */

/* USER CODE END PTD */

/* Private define ------------------------------------------------------------*/
/* USER CODE BEGIN PD */
#define CMD_UART_HANDLE     huart3
#define CMD_UART_BAUDRATE   (uint32_t)115200
#define CMD_RX_BUF_SIZE     32

#define LED0_PIN            GPIO_PIN_0
#define LED1_PIN            GPIO_PIN_1
#define LED2_PIN            GPIO_PIN_2
#define LED3_PIN            GPIO_PIN_3
#define LED_GPIO_PORT       GPIOA
#define LED_COUNT           4
#define LED_MIN_INDEX       0
#define LED_MAX_INDEX       3
/* USER CODE END PD */

/* Private macro -------------------------------------------------------------*/
/* USER CODE BEGIN PM */

/* USER CODE END PM */

/* Private variables ---------------------------------------------------------*/
UART_HandleTypeDef huart3;       // <-- Сгенерировано CubeMX, оставляем здесь

/* USER CODE BEGIN PV */
// Убрано повторное объявление huart3
static uint8_t cmd_rx_buf[CMD_RX_BUF_SIZE];
static uint8_t cmd_rx_index = 0;
static uint8_t cmd_rx_complete = 0;
/* USER CODE END PV */

/* Private function prototypes -----------------------------------------------*/
void SystemClock_Config(void);
static void MPU_Config(void);
static void MX_GPIO_Init(void);
static void MX_USART3_UART_Init(void);
/* USER CODE BEGIN PFP */
// Убрано повторное объявление MX_USART3_UART_Init
static void LED_Init(void);
static void UART_Rx_Start(void);
static void ProcessCommand(char *cmd_line);
static void SendResponse(const char *msg);
/* USER CODE END PFP */

/* Private user code ---------------------------------------------------------*/
/* USER CODE BEGIN 0 */

/**
  * @brief  Инициализация линий GPIO для светодиодов (PA0 – PA3).
  *         Все пины конфигурируются как выход Push-Pull, светодиоды выключены.
  */
static void LED_Init(void)
{
    GPIO_InitTypeDef GPIO_InitStruct = {0};
    GPIO_InitStruct.Pin = LED0_PIN | LED1_PIN | LED2_PIN | LED3_PIN;
    GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
    GPIO_InitStruct.Pull = GPIO_NOPULL;
    GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
    HAL_GPIO_Init(LED_GPIO_PORT, &GPIO_InitStruct);
    HAL_GPIO_WritePin(LED_GPIO_PORT,
                      LED0_PIN | LED1_PIN | LED2_PIN | LED3_PIN,
                      GPIO_PIN_RESET);
}

/**
  * @brief  Запуск приёма одного байта по UART в режиме прерывания.
  */
static void UART_Rx_Start(void)
{
    HAL_UART_Receive_IT(&CMD_UART_HANDLE, &cmd_rx_buf[cmd_rx_index], 1);
}

/**
  * @brief  Отправка строки через UART блокирующим способом.
  */
static void SendResponse(const char *msg)
{
    HAL_UART_Transmit(&CMD_UART_HANDLE, (uint8_t *)msg, strlen(msg), HAL_MAX_DELAY);
}

/**
  * @brief  Парсинг и выполнение принятой команды.
  *         Формат: "on <N>\r" или "off <N>\r", где N = 0..3.
  */
static void ProcessCommand(char *cmd_line)
{
    char *token;
    uint8_t led_num;
    char *endptr;
    uint8_t set_on = 0;

    if (cmd_line == NULL || strlen(cmd_line) == 0)
    {
        SendResponse("error invalid arguments\r\n");
        return;
    }

    token = strtok(cmd_line, " ");
    if (token == NULL)
    {
        SendResponse("error invalid arguments\r\n");
        return;
    }

    if (strcmp(token, "on") == 0)
        set_on = 1;
    else if (strcmp(token, "off") == 0)
        set_on = 0;
    else
    {
        SendResponse("error invalid arguments\r\n");
        return;
    }

    token = strtok(NULL, " ");
    if (token == NULL)
    {
        SendResponse("error invalid arguments\r\n");
        return;
    }

    /* Проверка, что номер состоит только из цифр */
    for (char *p = token; *p != '\0'; p++)
    {
        if (!isdigit((unsigned char)*p))
        {
            SendResponse("error invalid arguments\r\n");
            return;
        }
    }

    led_num = (uint8_t)strtoul(token, &endptr, 10);
    if (*endptr != '\0')
    {
        SendResponse("error invalid arguments\r\n");
        return;
    }

    if (led_num > LED_MAX_INDEX)
    {
        SendResponse("error invalid arguments\r\n");
        return;
    }

    /* Проверка на лишние аргументы */
    token = strtok(NULL, " ");
    if (token != NULL)
    {
        SendResponse("error invalid arguments\r\n");
        return;
    }

    uint16_t led_pin = 0;
    switch (led_num)
    {
        case 0: led_pin = LED0_PIN; break;
        case 1: led_pin = LED1_PIN; break;
        case 2: led_pin = LED2_PIN; break;
        case 3: led_pin = LED3_PIN; break;
        default: break;
    }

    HAL_GPIO_WritePin(LED_GPIO_PORT, led_pin, set_on ? GPIO_PIN_SET : GPIO_PIN_RESET);
    SendResponse("ok\r\n");
}

/**
  * @brief  Callback-функция по завершении приёма байта по UART.
  */
void HAL_UART_RxCpltCallback(UART_HandleTypeDef *huart)
{
    if (huart->Instance == USART3)
    {
        uint8_t received_byte = cmd_rx_buf[cmd_rx_index];
        if (received_byte == '\r')
        {
            cmd_rx_index++;
            cmd_rx_complete = 1;
        }
        else if (received_byte == '\n')
        {
            HAL_UART_Receive_IT(&CMD_UART_HANDLE, &cmd_rx_buf[cmd_rx_index], 1);
        }
        else
        {
            if (cmd_rx_index < (CMD_RX_BUF_SIZE - 2))
            {
                cmd_rx_index++;
                HAL_UART_Receive_IT(&CMD_UART_HANDLE, &cmd_rx_buf[cmd_rx_index], 1);
            }
            else
            {
                cmd_rx_index = 0;
                memset(cmd_rx_buf, 0, CMD_RX_BUF_SIZE);
                HAL_UART_Receive_IT(&CMD_UART_HANDLE, &cmd_rx_buf[cmd_rx_index], 1);
            }
        }
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

  /* MPU Configuration--------------------------------------------------------*/
  MPU_Config();

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
  MX_USART3_UART_Init();
  /* USER CODE BEGIN 2 */
  LED_Init();
  UART_Rx_Start();
  /* USER CODE END 2 */

  /* Infinite loop */
  /* USER CODE BEGIN WHILE */
  while (1)
  {
      if (cmd_rx_complete)
      {
          cmd_rx_buf[cmd_rx_index - 1] = '\0';
          ProcessCommand((char *)cmd_rx_buf);
          memset(cmd_rx_buf, 0, CMD_RX_BUF_SIZE);
          cmd_rx_index = 0;
          cmd_rx_complete = 0;
          UART_Rx_Start();
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

  /** Configure the main internal regulator output voltage
  */
  __HAL_RCC_PWR_CLK_ENABLE();
  __HAL_PWR_VOLTAGESCALING_CONFIG(PWR_REGULATOR_VOLTAGE_SCALE3);

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
  RCC_ClkInitStruct.ClockType = RCC_CLOCKTYPE_HCLK|RCC_CLOCKTYPE_SYSCLK
                              |RCC_CLOCKTYPE_PCLK1|RCC_CLOCKTYPE_PCLK2;
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
  * @brief USART3 Initialization Function
  * @param None
  * @retval None
  */
static void MX_USART3_UART_Init(void)
{

  /* USER CODE BEGIN USART3_Init 0 */

  /* USER CODE END USART3_Init 0 */

  /* USER CODE BEGIN USART3_Init 1 */

  /* USER CODE END USART3_Init 1 */
  huart3.Instance = USART3;
  huart3.Init.BaudRate = 115200;
  huart3.Init.WordLength = UART_WORDLENGTH_8B;
  huart3.Init.StopBits = UART_STOPBITS_1;
  huart3.Init.Parity = UART_PARITY_NONE;
  huart3.Init.Mode = UART_MODE_TX_RX;
  huart3.Init.HwFlowCtl = UART_HWCONTROL_NONE;
  huart3.Init.OverSampling = UART_OVERSAMPLING_16;
  huart3.Init.OneBitSampling = UART_ONE_BIT_SAMPLE_DISABLE;
  huart3.AdvancedInit.AdvFeatureInit = UART_ADVFEATURE_NO_INIT;
  if (HAL_UART_Init(&huart3) != HAL_OK)
  {
    Error_Handler();
  }
  /* USER CODE BEGIN USART3_Init 2 */

  /* USER CODE END USART3_Init 2 */

}

/**
  * @brief GPIO Initialization Function
  * @param None
  * @retval None
  */
static void MX_GPIO_Init(void)
{
/* USER CODE BEGIN MX_GPIO_Init_1 */
/* USER CODE END MX_GPIO_Init_1 */

  /* GPIO Ports Clock Enable */
  __HAL_RCC_GPIOB_CLK_ENABLE();
  __HAL_RCC_GPIOA_CLK_ENABLE();

/* USER CODE BEGIN MX_GPIO_Init_2 */
/* USER CODE END MX_GPIO_Init_2 */
}

/* USER CODE BEGIN 4 */

/* USER CODE END 4 */

 /* MPU Configuration */

void MPU_Config(void)
{
  MPU_Region_InitTypeDef MPU_InitStruct = {0};

  /* Disables the MPU */
  HAL_MPU_Disable();

  /** Initializes and configures the Region and the memory to be protected
  */
  MPU_InitStruct.Enable = MPU_REGION_ENABLE;
  MPU_InitStruct.Number = MPU_REGION_NUMBER0;
  MPU_InitStruct.BaseAddress = 0x0;
  MPU_InitStruct.Size = MPU_REGION_SIZE_4GB;
  MPU_InitStruct.SubRegionDisable = 0x87;
  MPU_InitStruct.TypeExtField = MPU_TEX_LEVEL0;
  MPU_InitStruct.AccessPermission = MPU_REGION_NO_ACCESS;
  MPU_InitStruct.DisableExec = MPU_INSTRUCTION_ACCESS_DISABLE;
  MPU_InitStruct.IsShareable = MPU_ACCESS_SHAREABLE;
  MPU_InitStruct.IsCacheable = MPU_ACCESS_NOT_CACHEABLE;
  MPU_InitStruct.IsBufferable = MPU_ACCESS_NOT_BUFFERABLE;

  HAL_MPU_ConfigRegion(&MPU_InitStruct);
  /* Enables the MPU */
  HAL_MPU_Enable(MPU_PRIVILEGED_DEFAULT);

}

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

#ifdef  USE_FULL_ASSERT
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
