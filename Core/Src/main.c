/* USER CODE BEGIN Header */
	/**
	  ******************************************************************************
	  * @file           : main.c
	  * @brief          : Main program body
	  ******************************************************************************
	  * @attention
	  *
	  * <h2><center>&copy; Copyright (c) 2026 STMicroelectronics.
	  * All rights reserved.</center></h2>
	  *
	  * This software component is licensed by ST under BSD 3-Clause license,
	  * the "License"; You may not use this file except in compliance with the
	  * License. You may obtain a copy of the License at:
	  *                        opensource.org/licenses/BSD-3-Clause
	  *
	  ******************************************************************************
	  */
/* USER CODE END Header */
/* Includes ------------------------------------------------------------------*/
#include "main.h"
#include "gc9a01a.h"

/* Private includes ----------------------------------------------------------*/
/* USER CODE BEGIN Includes */

/* USER CODE END Includes */

/* Private typedef -----------------------------------------------------------*/
/* USER CODE BEGIN PTD */
typedef enum {
    STATE_STOP,
    STATE_RUN,
    STATE_LAP_VIEW
} SystemState_t;

typedef enum {
    EVENT_NONE,
    EVENT_BTN_RED,
    EVENT_BTN_BLUE,
    EVENT_BTN_YELLOW
} SystemEvent_t;

typedef struct {
    uint8_t min;
    uint8_t sec;
    uint8_t ms;
} LapRecord_t;

/* USER CODE END PTD */

/* Private define ------------------------------------------------------------*/
/* USER CODE BEGIN PD */
#define MAX_LAPS 20

#define BTN_BLUE_PORT GPIOA
#define BTN_BLUE_PIN GPIO_PIN_8

#define BTN_YELLOW_PORT GPIOA
#define BTN_YELLOW_PIN GPIO_PIN_9

#define BTN_RED_PORT GPIOA
#define BTN_RED_PIN GPIO_PIN_10

#define BUZZER_PORT GPIOB
#define BUZZER_PIN GPIO_PIN_3

/* USER CODE END PD */

/* Private macro -------------------------------------------------------------*/
/* USER CODE BEGIN PM */

/* USER CODE END PM */

/* Private variables ---------------------------------------------------------*/
RTC_HandleTypeDef hrtc;

SPI_HandleTypeDef hspi1;

TIM_HandleTypeDef htim2;

/* USER CODE BEGIN PV */
GC9A01A my_tft;
volatile uint8_t debounce_flag = 0;

  //Biến quản lý thời gian
volatile uint16_t milliseconds = 0;
volatile uint8_t seconds = 0;
volatile uint8_t minutes = 0;
volatile uint8_t show_dots = 1;
volatile uint8_t buzzer_timeout = 0;

//Biến quản lý FSM và hệ thống
volatile uint8_t update_display = 1;
volatile uint8_t is_running = 0;
volatile SystemEvent_t pending_event = EVENT_NONE;
SystemState_t current_state = STATE_STOP;

//Biến quản lý LAP
LapRecord_t lap_array[MAX_LAPS];
uint8_t lap_count = 0;
uint8_t current_lap_view = 0;
/* USER CODE END PV */

/* Private function prototypes -----------------------------------------------*/
void SystemClock_Config(void);
static void MX_GPIO_Init(void);
static void MX_TIM2_Init(void);
static void MX_SPI1_Init(void);
static void MX_RTC_Init(void);
/* USER CODE BEGIN PFP */

/* USER CODE END PFP */

/* Private user code ---------------------------------------------------------*/
/* USER CODE BEGIN 0 */
const uint8_t seg_map[10] = {
    0x3F, 0x06, 0x5B, 0x4F, 0x66, 0x6D, 0x7D, 0x07, 0x7F, 0x6F
};

void Beep(void)
{
	HAL_GPIO_WritePin(BUZZER_PORT, BUZZER_PIN, GPIO_PIN_SET);
	buzzer_timeout = 5;
}

void draw_segment(uint16_t x, uint16_t y, uint16_t w, uint16_t h, uint16_t color) {
    GC9A01A_start_spi_transaction(&my_tft);
    GC9A01A_set_addr_window(&my_tft, x, y, w, h);
    GC9A01A_set_spi_datasize(&my_tft, SPI_DATASIZE_16BIT);
    uint32_t total_pixel = w * h;
    for (uint32_t i = 0; i < total_pixel; ++i) {
        HAL_SPI_Transmit(&hspi1, (uint8_t *)&color, 1, 10);
    }
    GC9A01A_end_spi_transaction(&my_tft);
}

// Vẽ LED 7 đoạn với kích thước thu nhỏ
void Display_Digit(uint8_t num, uint16_t ox, uint16_t oy, uint16_t color, uint16_t bg_color) {
    if (num > 9) return;
    uint8_t code = seg_map[num];
    uint16_t sw = 20; //Cao thanh ngang
    uint16_t sh = 20; //Cao thanh dọc
    uint16_t th = 4;  //Độ dày thanh

    draw_segment(ox + th,     oy,              sw, th, (code & 0x01) ? color : bg_color); // a
    draw_segment(ox + sw+th,  oy + th,         th, sh, (code & 0x02) ? color : bg_color); // b
    draw_segment(ox + sw+th,  oy + 2*th + sh,  th, sh, (code & 0x04) ? color : bg_color); // c
    draw_segment(ox + th,     oy + 2*sh + 2*th,sw, th, (code & 0x08) ? color : bg_color); // d
    draw_segment(ox,          oy + 2*th + sh,  th, sh, (code & 0x10) ? color : bg_color); // e
    draw_segment(ox,          oy + th,         th, sh, (code & 0x20) ? color : bg_color); // f
    draw_segment(ox + th,     oy + th + sh,    sw, th, (code & 0x40) ? color : bg_color); // g
}

void Display_Small_Digit(uint8_t num, uint16_t ox, uint16_t oy, uint16_t color, uint16_t bg_color) {
    if (num > 9) return;
    uint8_t code = seg_map[num];
    uint16_t sw = 10; //Cao thanh ngang
    uint16_t sh = 12; //Cao thanh dọc
    uint16_t th = 3;  //Độ dày thanh

    draw_segment(ox + th,      oy,              sw, th, (code & 0x01) ? color : bg_color); // a
    draw_segment(ox + sw+th,   oy + th,         th, sh, (code & 0x02) ? color : bg_color); // b
    draw_segment(ox + sw+th,   oy + 2*th + sh,  th, sh, (code & 0x04) ? color : bg_color); // c
    draw_segment(ox + th,      oy + 2*sh + 2*th,sw, th, (code & 0x08) ? color : bg_color); // d
    draw_segment(ox,           oy + 2*th + sh,  th, sh, (code & 0x10) ? color : bg_color); // e
    draw_segment(ox,           oy + th,         th, sh, (code & 0x20) ? color : bg_color); // f
    draw_segment(ox + th,      oy + th + sh,    sw, th, (code & 0x40) ? color : bg_color); // g
}

void FSM_Update(SystemEvent_t event) {
	SystemState_t old_state = current_state;
    switch (current_state) {

        case STATE_STOP:
            if (event == EVENT_BTN_RED) {
                is_running = 1;
                current_state = STATE_RUN;
                update_display = 1;
            }
            else if (event == EVENT_BTN_BLUE) {
                minutes = 0; seconds = 0; milliseconds = 0;
                lap_count = 0;
                update_display = 1;
            }
            else if (event == EVENT_BTN_YELLOW) {
                if (lap_count > 0) {
                    current_lap_view = lap_count - 1;
                    current_state = STATE_LAP_VIEW;
                    update_display = 1;
                }
            }
            break;

        case STATE_RUN:
            if (event == EVENT_BTN_RED) {
                is_running = 0;
                current_state = STATE_STOP;
                update_display = 1;
            }
            else if (event == EVENT_BTN_BLUE) {
                if (lap_count < MAX_LAPS) {
                    lap_array[lap_count].min = minutes;
                    lap_array[lap_count].sec = seconds;
                    lap_array[lap_count].ms = milliseconds;
                    lap_count++;
                    update_display = 1;
                }
            }
            else if (event == EVENT_BTN_YELLOW) {
                if (lap_count > 0) {
                    current_lap_view = lap_count - 1;
                    current_state = STATE_LAP_VIEW;
                    update_display = 1;
                }
            }
            break;

        case STATE_LAP_VIEW:
            if (event == EVENT_BTN_RED) {
                if (current_lap_view > 0) {
                    current_lap_view--;
                    update_display = 1;
                }
            }
            else if (event == EVENT_BTN_BLUE) {
                if (current_lap_view < lap_count - 1) {
                    current_lap_view++;
                    update_display = 1;
                }
            }
            else if (event == EVENT_BTN_YELLOW) {
                current_state = (is_running == 1) ? STATE_RUN : STATE_STOP;
                update_display = 1;
            }
            break;
    }

    if (current_state != old_state)
    {
    	Beep();
    }
    else if (event == EVENT_BTN_BLUE)
    {
    	Beep();
    }
    else if (event == EVENT_BTN_RED && current_state == STATE_LAP_VIEW)
        {
            Beep();
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
  MX_TIM2_Init();
  MX_SPI1_Init();
  MX_RTC_Init();
  /* USER CODE BEGIN 2 */
	  // Reset LCD
	  HAL_GPIO_WritePin(GPIOB, GPIO_PIN_0, GPIO_PIN_RESET);
	  HAL_Delay(200);
	  HAL_GPIO_WritePin(GPIOB, GPIO_PIN_0, GPIO_PIN_SET);
	  HAL_Delay(200);

	  GC9A01A_init(&my_tft,
					 &hspi1,
					 CS_GPIO_Port, CS_Pin,
					 GPIOB, DC_Pin,
					 NULL, 0,
					 GPIOB, RST_Pin);

	  //==================
	    GC9A01A_start_spi_transaction(&my_tft);
	    GC9A01A_set_addr_window(&my_tft, 0, 0, 240, 240);
	    GC9A01A_set_spi_datasize(&my_tft, SPI_DATASIZE_16BIT);
	    uint16_t black = GC9A01A_BLACK;
	    for (uint32_t i = 0; i < 57600; ++i)
	    {
	        HAL_SPI_Transmit(&hspi1, (uint8_t *)&black, 1, 10);
	    }
	    GC9A01A_end_spi_transaction(&my_tft);

	  uint16_t oy = 95;
		uint16_t yellow = GC9A01A_YELLOW;

		Display_Digit(0, 9,   oy, yellow, black); // Phút hàng chục
		Display_Digit(0, 42,  oy, yellow, black); // Phút hàng đơn vị
		Display_Digit(0, 94,  oy, yellow, black); // Giây hàng chục
		Display_Digit(0, 127, oy, yellow, black); // Giây hàng đơn vị

		uint16_t oy_small = oy + 18;
		Display_Small_Digit(0, 180, oy_small, yellow, black); // Phần trăm giây chục
		Display_Small_Digit(0, 205, oy_small, yellow, black); // Phần trăm giây đơn vị

		// Vẽ 2 dấu chấm tĩnh
		draw_segment(80, oy + 15, 4, 4, yellow);
		draw_segment(80, oy + 35, 4, 4, yellow);

		// Vẽ dấu chấm mili giây
		draw_segment(163, oy + 44, 4, 4, yellow);

		HAL_TIM_Base_Start_IT(&htim2);

  /* USER CODE END 2 */

  /* Infinite loop */
  /* USER CODE BEGIN WHILE */
	  while (1)
	  {
    /* USER CODE END WHILE */

    /* USER CODE BEGIN 3 */
      SystemEvent_t event = EVENT_NONE;

      // 1. Kiểm tra xem ngắt có gửi sự kiện nào tới không

      __disable_irq();
      event = pending_event;
      pending_event = EVENT_NONE;
      __enable_irq();

      if (event != EVENT_NONE)
      {
          FSM_Update(event);
      }

		  if (update_display == 1) {
			  update_display = 0;

        uint8_t disp_m, disp_s;
        uint16_t disp_ms;
        uint16_t color;

        if (current_state == STATE_LAP_VIEW)
        {
            disp_m = lap_array[current_lap_view].min;
            disp_s = lap_array[current_lap_view].sec;
            disp_ms = lap_array[current_lap_view].ms;
            color = GC9A01A_CYAN;
        }
        else
        {
            disp_m = minutes;
            disp_s = seconds;
            disp_ms = milliseconds;
            color = GC9A01A_YELLOW;
        }

			  uint16_t oy = 95;
			  uint16_t oy_small = oy + 18;
			  uint16_t black = GC9A01A_BLACK;

			  // Vẽ Phút
			  Display_Digit(disp_m / 10, 9,   oy, color, black);
			  Display_Digit(disp_m % 10, 42,  oy, color, black);

			  // Vẽ Giây
			  Display_Digit(disp_s / 10, 94,  oy, color, black);
			  Display_Digit(disp_s % 10, 127, oy, color, black);

			  // Dấu 2 chấm
			  uint16_t dot_color = (show_dots) ? color : black;
			  draw_segment(80, oy + 15, 4, 4, dot_color);
			  draw_segment(80, oy + 35, 4, 4, dot_color);

			  // Vẽ phần trăm giây
			  uint8_t hundredths = disp_ms / 10;
			  Display_Small_Digit(hundredths / 10, 180, oy_small, color, black);
			  Display_Small_Digit(hundredths % 10, 205, oy_small, color, black);

			  // Dấu 1 chấm tĩnh
			  draw_segment(163, oy + 44, 4, 4, color);
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
  RCC_PeriphCLKInitTypeDef PeriphClkInit = {0};

  /** Initializes the RCC Oscillators according to the specified parameters
  * in the RCC_OscInitTypeDef structure.
  */
  RCC_OscInitStruct.OscillatorType = RCC_OSCILLATORTYPE_LSI|RCC_OSCILLATORTYPE_HSE;
  RCC_OscInitStruct.HSEState = RCC_HSE_ON;
  RCC_OscInitStruct.HSEPredivValue = RCC_HSE_PREDIV_DIV1;
  RCC_OscInitStruct.HSIState = RCC_HSI_ON;
  RCC_OscInitStruct.LSIState = RCC_LSI_ON;
  RCC_OscInitStruct.PLL.PLLState = RCC_PLL_ON;
  RCC_OscInitStruct.PLL.PLLSource = RCC_PLLSOURCE_HSE;
  RCC_OscInitStruct.PLL.PLLMUL = RCC_PLL_MUL9;
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
  PeriphClkInit.PeriphClockSelection = RCC_PERIPHCLK_RTC;
  PeriphClkInit.RTCClockSelection = RCC_RTCCLKSOURCE_LSI;
  if (HAL_RCCEx_PeriphCLKConfig(&PeriphClkInit) != HAL_OK)
  {
    Error_Handler();
  }
}

/**
  * @brief RTC Initialization Function
  * @param None
  * @retval None
  */
static void MX_RTC_Init(void)
{

  /* USER CODE BEGIN RTC_Init 0 */

  /* USER CODE END RTC_Init 0 */

  RTC_TimeTypeDef sTime = {0};
  RTC_DateTypeDef DateToUpdate = {0};

  /* USER CODE BEGIN RTC_Init 1 */

  /* USER CODE END RTC_Init 1 */
  /** Initialize RTC Only
  */
  hrtc.Instance = RTC;
  hrtc.Init.AsynchPrediv = RTC_AUTO_1_SECOND;
  hrtc.Init.OutPut = RTC_OUTPUTSOURCE_ALARM;
  if (HAL_RTC_Init(&hrtc) != HAL_OK)
  {
    Error_Handler();
  }

  /* USER CODE BEGIN Check_RTC_BKUP */

  /* USER CODE END Check_RTC_BKUP */

  /** Initialize RTC and set the Time and Date
  */
  sTime.Hours = 0x12;
  sTime.Minutes = 0x15;
  sTime.Seconds = 0x0;

  if (HAL_RTC_SetTime(&hrtc, &sTime, RTC_FORMAT_BCD) != HAL_OK)
  {
    Error_Handler();
  }
  DateToUpdate.WeekDay = RTC_WEEKDAY_MONDAY;
  DateToUpdate.Month = RTC_MONTH_JANUARY;
  DateToUpdate.Date = 0x1;
  DateToUpdate.Year = 0x0;

  if (HAL_RTC_SetDate(&hrtc, &DateToUpdate, RTC_FORMAT_BCD) != HAL_OK)
  {
    Error_Handler();
  }
  /* USER CODE BEGIN RTC_Init 2 */

  /* USER CODE END RTC_Init 2 */

}

/**
  * @brief SPI1 Initialization Function
  * @param None
  * @retval None
  */
static void MX_SPI1_Init(void)
{

  /* USER CODE BEGIN SPI1_Init 0 */

  /* USER CODE END SPI1_Init 0 */

  /* USER CODE BEGIN SPI1_Init 1 */

  /* USER CODE END SPI1_Init 1 */
  /* SPI1 parameter configuration*/
  hspi1.Instance = SPI1;
  hspi1.Init.Mode = SPI_MODE_MASTER;
  hspi1.Init.Direction = SPI_DIRECTION_2LINES;
  hspi1.Init.DataSize = SPI_DATASIZE_8BIT;
  hspi1.Init.CLKPolarity = SPI_POLARITY_LOW;
  hspi1.Init.CLKPhase = SPI_PHASE_1EDGE;
  hspi1.Init.NSS = SPI_NSS_SOFT;
  hspi1.Init.BaudRatePrescaler = SPI_BAUDRATEPRESCALER_16;
  hspi1.Init.FirstBit = SPI_FIRSTBIT_MSB;
  hspi1.Init.TIMode = SPI_TIMODE_DISABLE;
  hspi1.Init.CRCCalculation = SPI_CRCCALCULATION_DISABLE;
  hspi1.Init.CRCPolynomial = 10;
  if (HAL_SPI_Init(&hspi1) != HAL_OK)
  {
    Error_Handler();
  }
  /* USER CODE BEGIN SPI1_Init 2 */

  /* USER CODE END SPI1_Init 2 */

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
  htim2.Init.Prescaler = 7199;
  htim2.Init.CounterMode = TIM_COUNTERMODE_UP;
  htim2.Init.Period = 99;
  htim2.Init.ClockDivision = TIM_CLOCKDIVISION_DIV1;
  htim2.Init.AutoReloadPreload = TIM_AUTORELOAD_PRELOAD_ENABLE;
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
  * @brief GPIO Initialization Function
  * @param None
  * @retval None
  */
static void MX_GPIO_Init(void)
{
  GPIO_InitTypeDef GPIO_InitStruct = {0};

  /* GPIO Ports Clock Enable */
  __HAL_RCC_GPIOD_CLK_ENABLE();
  __HAL_RCC_GPIOA_CLK_ENABLE();
  __HAL_RCC_GPIOB_CLK_ENABLE();

  /*Configure GPIO pin Output Level */
  HAL_GPIO_WritePin(RST_GPIO_Port, RST_Pin, GPIO_PIN_SET);

  /*Configure GPIO pin Output Level */
  HAL_GPIO_WritePin(GPIOB, CS_Pin|DC_Pin|Buzzer_Pin, GPIO_PIN_RESET);

  /*Configure GPIO pins : RST_Pin CS_Pin DC_Pin Buzzer_Pin */
  GPIO_InitStruct.Pin = RST_Pin|CS_Pin|DC_Pin|Buzzer_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
  HAL_GPIO_Init(GPIOB, &GPIO_InitStruct);

  /*Configure GPIO pins : BTN_BLUE_Pin BTN_YELLOW_Pin BTN_RED_Pin */
  GPIO_InitStruct.Pin = BTN_BLUE_Pin|BTN_YELLOW_Pin|BTN_RED_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_IT_FALLING;
  GPIO_InitStruct.Pull = GPIO_PULLUP;
  HAL_GPIO_Init(GPIOA, &GPIO_InitStruct);

  /* EXTI interrupt init*/
  HAL_NVIC_SetPriority(EXTI9_5_IRQn, 0, 0);
  HAL_NVIC_EnableIRQ(EXTI9_5_IRQn);

  HAL_NVIC_SetPriority(EXTI15_10_IRQn, 0, 0);
  HAL_NVIC_EnableIRQ(EXTI15_10_IRQn);
}

/* USER CODE BEGIN 4 */

void HAL_GPIO_EXTI_Callback(uint16_t GPIO_Pin)
{
    uint32_t current_time = HAL_GetTick();

    static uint32_t last_blue_time = 0;
    static uint32_t last_yellow_time = 0;
    static uint32_t last_red_time = 0;

    if (GPIO_Pin == BTN_BLUE_Pin) {
        if (current_time - last_blue_time > 200) {
            pending_event = EVENT_BTN_BLUE;
            last_blue_time = current_time;
        }
    }
    else if (GPIO_Pin == BTN_YELLOW_Pin) {
        if (current_time - last_yellow_time > 200) {
            pending_event = EVENT_BTN_YELLOW;
            last_yellow_time = current_time;
        }
    }
    else if (GPIO_Pin == BTN_RED_Pin) {
        if (current_time - last_red_time > 200) {
            pending_event = EVENT_BTN_RED;
            last_red_time = current_time;
        }
    }
}

void HAL_TIM_PeriodElapsedCallback(TIM_HandleTypeDef* htim) {
      if (htim->Instance == TIM2)
      {
    	  if (buzzer_timeout > 0)
    	  {
    		  buzzer_timeout--;
    		  if (buzzer_timeout == 0)
    		  {
    			  HAL_GPIO_WritePin(BUZZER_PORT, BUZZER_PIN, GPIO_PIN_RESET);
    		  }
    	  }
          if (is_running == 1) {
              milliseconds += 10;

              if (milliseconds >= 1000) {
                  milliseconds = 0;
                  seconds++;

                  if (seconds >= 60) {
                      seconds = 0;
                      minutes++;
                      if (minutes >= 60) {
                          minutes = 0;
                      }
                  }
              }
              if (current_state == STATE_RUN) {
                  update_display = 1;
              }
          }
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

/************************ (C) COPYRIGHT STMicroelectronics *****END OF FILE****/
