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
#include <stdbool.h>
#include <stdio.h>
#include <string.h>
#include "sht40.h"
/* USER CODE END Includes */

/* Private typedef -----------------------------------------------------------*/
/* USER CODE BEGIN PTD */
typedef enum
{
	NODE_BOOT = 0,
	NODE_OK,
	NODE_SENSOR_ERROR,
	NODE_LOG_ERROR
} node_status_t;

typedef enum
{
	PAGE_LIVE_CLIMATE = 0,
	PAGE_LIVE_AIR,
	PAGE_LIVE_SOIL,
	PAGE_STATUS,
	PAGE_MAINTENANCE
} ui_page_t;
/* USER CODE END PTD */

/* Private define ------------------------------------------------------------*/
/* USER CODE BEGIN PD */
#define LED_PIN 5U
#define BUTTON_PIN 13U
#define BUTTON_DEBOUNCE_MS 50U
#define BUTTON_LONG_MS 800U
#define UI_PAGE_COUNT 5U

/* USER CODE END PD */

/* Private macro -------------------------------------------------------------*/
/* USER CODE BEGIN PM */

/* USER CODE END PM */

/* Private variables ---------------------------------------------------------*/
I2C_HandleTypeDef hi2c1;

UART_HandleTypeDef huart2;

/* USER CODE BEGIN PV */
volatile uint32_t button_irq_count = 0; //how many interrupt events happened
volatile uint32_t button_irq_pending = 0; //flag
static node_status_t node_status = NODE_BOOT;
static ui_page_t ui_page = PAGE_LIVE_CLIMATE;
static sensor_sample_t latest_ok = {0};
static sht40_status_t last_sht40_status = SHT40_OK; /* last read result (ok or error) */

static volatile uint8_t btn_press_armed = 0;
static volatile uint8_t btn_long_fired = 0;
static volatile uint32_t btn_press_start_ms = 0;
static volatile uint32_t btn_last_edge_ms = 0;
/* USER CODE END PV */

/* Private function prototypes -----------------------------------------------*/
void SystemClock_Config(void);
static void MX_GPIO_Init(void);
static void MX_USART2_UART_Init(void);
static void MX_I2C1_Init(void);
/* USER CODE BEGIN PFP */
/* USER CODE END PFP */

/* Private user code ---------------------------------------------------------*/
/* USER CODE BEGIN 0 */
static void register_led_init(void)
{
	//peripheral RCC -> register AHB1ENR OR= mask for GPIOAEN
	RCC->AHB1ENR |= RCC_AHB1ENR_GPIOAEN;
	(void)RCC->AHB1ENR;

	// MODER has 2 bits per pin:
	    // 00=input, 01=output, 10=alternate function, 11=analog.
	GPIOA->MODER &= ~(3U << (LED_PIN * 2U)); //reset pin 5 mode bits
	GPIOA->MODER |= (1U << (LED_PIN * 2U));	// set pin 5 to output

	// Output type: 0=push-pull, 1=open-drain.
	GPIOA->OTYPER &= ~(1U << LED_PIN);

	// Output speed: 00=low speed.
	GPIOA->OSPEEDR &= ~(3U << (LED_PIN * 2U));

	// Pull-up/pull-down: 00=no pull.
	GPIOA->PUPDR &= ~(3U << (LED_PIN * 2U));

	// Start with LED off.
	GPIOA->BSRR = (1U << (LED_PIN + 16U));
}

static void register_led_toggle(void)
{
	GPIOA->ODR ^= (1U << LED_PIN);
}

static void register_led_on(void)
{
	GPIOA->BSRR = (1U << (LED_PIN));
}

static void register_led_off(void)
{
	GPIOA->BSRR = (1U << (LED_PIN + 16U));
}

static void led_status_update(uint32_t now)
{
	switch(node_status)
	{
	case NODE_BOOT:
		register_led_on();
		break;

	case NODE_OK:
	{
		//1Hz blinking
		if ((now % 1000U) < 500U)
			register_led_on();
		else
			register_led_off();
		break;
	}

	case NODE_SENSOR_ERROR:
	{
		//5Hz blink
		if ((now % 200U) < 100U)
			register_led_on();
		else
			register_led_off();
		break;
	}

	case NODE_LOG_ERROR:
	{
		//double blink
		uint32_t phase = now % 1000U;
		if (phase < 100U)
			register_led_on();
		else if (phase < 200U)
			register_led_off();
		else if (phase < 300U)
			register_led_on();
		else
			register_led_off();
		break;
	}

	default:
		register_led_off();
		break;
	}
}

static void uart_write(const char *message)
{
	HAL_UART_Transmit(&huart2, (uint8_t*)message, strlen(message), HAL_MAX_DELAY);
}

static void i2c_scan(void)
{
	char line[64];
	uint8_t found = 0;
	uart_write("\r\nI2C Scan Start\r\n");
	for (uint8_t addr = 1; addr < 127; addr++)
	{
		if (HAL_I2C_IsDeviceReady(&hi2c1, (uint8_t)(addr<<1), 1, 10) == HAL_OK)
		{
			snprintf(line, sizeof(line), "Found device at 0x%02X\r\n", addr);
			uart_write(line);
			found++;
		}
	}

	if (found == 0)
	{
		uart_write("No I2C devices were found.\r\n");
	}
	else
	{
		snprintf(line, sizeof(line), "I2C Scan done. Found %u device(s).\r\n", found);
		uart_write(line);
	}
}

static bool button_is_pressed(void)
{
	return (GPIOC->IDR & (1U << BUTTON_PIN)) == 0U;
}

static void ui_show_page(void)
{
	char title[20];
	char data[20];

	switch(ui_page)
	{
	case PAGE_LIVE_CLIMATE:
		snprintf(title, sizeof(title), "LIVE CLIMATE");
		snprintf(data, sizeof(data), "%4.1fC   %4.1f%%RH",
				latest_ok.temp_c, latest_ok.rh_pct);
		break;

	case PAGE_LIVE_AIR:
		snprintf(title, sizeof(title), "LIVE AIR");
		snprintf(data, sizeof(data), "Lx --  CO2 --");
		break;

	case PAGE_LIVE_SOIL:
		snprintf(title, sizeof(title), "LIVE SOIL");
		snprintf(data, sizeof(data), "Soil --");
		break;

	case PAGE_STATUS:
		snprintf(title, sizeof(title), "STATUS");
		if (node_status == NODE_OK)
			snprintf(data, sizeof(data), "OK");
		else if (node_status == NODE_SENSOR_ERROR)
			snprintf(data, sizeof(data), "ERR %-3s",
					sht40_status_str(last_sht40_status));
		else if (node_status == NODE_LOG_ERROR)
			snprintf(data, sizeof(data), "ERR LOG");
		else
			snprintf(data, sizeof(data), "BOOT");
		break;

	case PAGE_MAINTENANCE:
	default:
		snprintf(title, sizeof(title), "MAINT");
		snprintf(data, sizeof(data), "Hold=heater");
		break;
	}

	uart_write(title);
	uart_write("\r\n");
	uart_write(data);
	uart_write("\r\n");
}

static void ui_short_press(void)
{
	ui_page = (ui_page_t)(((unsigned)ui_page + 1U) % UI_PAGE_COUNT);
	ui_show_page();
}

/* Jump to STATUS on a new sensor fault (or if user left STATUS while still failing). */
static void ui_report_sensor_error(sht40_status_t err)
{
	const uint8_t already_err = (node_status == NODE_SENSOR_ERROR) ? 1U : 0U;

	last_sht40_status = err;
	node_status = NODE_SENSOR_ERROR;

	if (already_err == 0U || ui_page != PAGE_STATUS)
	{
		ui_page = PAGE_STATUS;
		ui_show_page();
	}
}

static void ui_heater_once(sensor_sample_t *sample)
{
	char line[48];
	sht40_status_t rch = sht40_read_heater_sample(sample);

	if (rch == SHT40_OK)
	{
		node_status = NODE_OK;
		last_sht40_status = SHT40_OK;
		latest_ok = *sample;
		snprintf(line, sizeof(line),
				 "[heater] %4.1fC %4.1f%%RH\r\n",
				 sample->temp_c,
				 sample->rh_pct);
		uart_write(line);
	}
	else
	{
		ui_report_sensor_error(rch);
		snprintf(line, sizeof(line),
				 "SHT40 error: %s\r\n", sht40_status_str(rch));
		uart_write(line);
	}
}

/* Must come after ui_short_press / ui_heater_once (or declare prototypes above). */
static void button_ui_process(uint32_t now, sensor_sample_t *sample)
{
	if (btn_press_armed == 0U)
		return;

	uint32_t held = now - btn_press_start_ms;

	if (button_is_pressed())
	{
		if (btn_long_fired == 0U && held >= BUTTON_LONG_MS)
		{
			btn_long_fired = 1U;
			if (ui_page == PAGE_MAINTENANCE)
				ui_heater_once(sample);
		}
		return;
	}

	btn_press_armed = 0U;

	if (btn_long_fired != 0U)
		return;

	if (held < BUTTON_DEBOUNCE_MS)
		return;

	ui_short_press();
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
  MX_USART2_UART_Init();
  MX_I2C1_Init();
  /* USER CODE BEGIN 2 */
  register_led_init();
  register_led_on();
  uart_write("\r\nSTM32 plant node boot\r\n");
  ui_show_page();
  i2c_scan();
  node_status = NODE_OK;
  /* USER CODE END 2 */

  /* Infinite loop */
  /* USER CODE BEGIN WHILE */
  uint32_t last_sensor_ms = 0;
  char line[80];
  sensor_sample_t sample = {0};
  while (1)
  {
    /* USER CODE END WHILE */

    /* USER CODE BEGIN 3 */
	  uint32_t now = HAL_GetTick();
	  led_status_update(now);
	  button_ui_process(now, &sample);

	  if ((now - last_sensor_ms) >= 1000)
	  {
		  last_sensor_ms = now;

		  sht40_status_t rc = sht40_read_normal_sample(&sample);

		  if (rc == SHT40_OK)
		  {
			  node_status = NODE_OK;
			  last_sht40_status = SHT40_OK;
			  latest_ok = sample;
			  ui_show_page();
			  /*snprintf(line, sizeof(line),
					   "t=%lu ms  T=%.2f C  rh=%.2f %%\r\n",
					   (unsigned long)sample.tick_ms,
					   sample.temp_c,
					   sample.rh_pct);
			  uart_write(line);*/
		  }
		  else
		  {
			  ui_report_sensor_error(rc);
			  snprintf(line, sizeof(line), "SHT40 error: %s\r\n", sht40_status_str(rc));
			  uart_write(line);
		  }
	  }
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
  hi2c1.Init.ClockSpeed = 100000;
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
  * @brief USART2 Initialization Function
  * @param None
  * @retval None
  */
static void MX_USART2_UART_Init(void)
{

  /* USER CODE BEGIN USART2_Init 0 */

  /* USER CODE END USART2_Init 0 */

  /* USER CODE BEGIN USART2_Init 1 */

  /* USER CODE END USART2_Init 1 */
  huart2.Instance = USART2;
  huart2.Init.BaudRate = 115200;
  huart2.Init.WordLength = UART_WORDLENGTH_8B;
  huart2.Init.StopBits = UART_STOPBITS_1;
  huart2.Init.Parity = UART_PARITY_NONE;
  huart2.Init.Mode = UART_MODE_TX_RX;
  huart2.Init.HwFlowCtl = UART_HWCONTROL_NONE;
  huart2.Init.OverSampling = UART_OVERSAMPLING_16;
  if (HAL_UART_Init(&huart2) != HAL_OK)
  {
    Error_Handler();
  }
  /* USER CODE BEGIN USART2_Init 2 */

  /* USER CODE END USART2_Init 2 */

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
  __HAL_RCC_GPIOC_CLK_ENABLE();
  __HAL_RCC_GPIOH_CLK_ENABLE();
  __HAL_RCC_GPIOA_CLK_ENABLE();
  __HAL_RCC_GPIOB_CLK_ENABLE();

  /*Configure GPIO pin : PC13 */
  GPIO_InitStruct.Pin = GPIO_PIN_13;
  GPIO_InitStruct.Mode = GPIO_MODE_IT_FALLING;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  HAL_GPIO_Init(GPIOC, &GPIO_InitStruct);

  /*Configure GPIO pin : LD2_Pin */
  GPIO_InitStruct.Pin = LD2_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
  HAL_GPIO_Init(LD2_GPIO_Port, &GPIO_InitStruct);

  /* EXTI interrupt init*/
  HAL_NVIC_SetPriority(EXTI15_10_IRQn, 0, 0);
  HAL_NVIC_EnableIRQ(EXTI15_10_IRQn);

  /* USER CODE BEGIN MX_GPIO_Init_2 */

  /* USER CODE END MX_GPIO_Init_2 */
}

/* USER CODE BEGIN 4 */
void HAL_GPIO_EXTI_Callback(uint16_t GPIO_pin)
{
	if(GPIO_pin != GPIO_PIN_13)
		return;
	uint32_t now = HAL_GetTick();
	if ((now - btn_last_edge_ms) < BUTTON_DEBOUNCE_MS)
		return;

	btn_last_edge_ms = now;
	btn_press_start_ms = now;
	btn_press_armed = 1U;
	btn_long_fired = 0U;
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
