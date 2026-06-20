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
TIM_HandleTypeDef htim3;

/* USER CODE BEGIN PV */
#define FLOOR_1 1
#define FLOOR_2 2
#define FLOOR_3 3

// Motor Cruise Speed Boundaries (Out of 1000 ARR)
#define MOTOR_MIN_SPEED 295
#define MOTOR_MAX_SPEED 320

// --- MODULAR ALIGNMENT TIMINGS (Tweak these to align the lift perfectly!) ---
uint16_t f1_align_delay = 500; // Milliseconds motor keeps running after hitting IR1
uint16_t f2_align_delay = 500; // Milliseconds motor keeps running after hitting IR2
uint16_t f3_align_delay = 500; // Milliseconds motor keeps running after hitting IR3

// Elevator State Control Variables
uint8_t current_floor = 0;
uint8_t target_floor = 1;
uint8_t is_moving = 0;
uint8_t emergency_active = 0;
uint8_t display_initialized = 0;

// High-Reliability IR Sampling Filter Variables
uint16_t f1_debounce = 0;
uint16_t f2_debounce = 0;
uint16_t f3_debounce = 0;
#define IR_THRESHOLD 25

// 7-Segment Array Matrix (Common Anode Profile)
const uint8_t seg_matrix[4][7] = {
    {0, 0, 0, 0, 0, 0, 0}, // Index 0: Completely OFF
    {0, 1, 1, 0, 0, 0, 0}, // Index 1: Floor 1 (b, c)
    {1, 1, 0, 1, 1, 0, 1}, // Index 2: Floor 2 (a, b, d, e, g)
    {1, 1, 1, 1, 0, 0, 1}  // Index 3: Floor 3 (a, b, c, d, g)
};
/* USER CODE END PV */

/* Private function prototypes -----------------------------------------------*/
void SystemClock_Config(void);
static void MX_GPIO_Init(void);
static void MX_TIM3_Init(void);
/* USER CODE BEGIN PFP */
extern TIM_HandleTypeDef htim3; // Link to CubeMX generated Timer 3

void Update_7Segment(uint8_t floor_num) {
    HAL_GPIO_WritePin(GPIOD, GPIO_PIN_5,  seg_matrix[floor_num][0] ? GPIO_PIN_RESET : GPIO_PIN_SET); // a
    HAL_GPIO_WritePin(GPIOE, GPIO_PIN_2,  seg_matrix[floor_num][1] ? GPIO_PIN_RESET : GPIO_PIN_SET); // b
    HAL_GPIO_WritePin(GPIOA, GPIO_PIN_10, seg_matrix[floor_num][2] ? GPIO_PIN_RESET : GPIO_PIN_SET); // c
    HAL_GPIO_WritePin(GPIOB, GPIO_PIN_3,  seg_matrix[floor_num][3] ? GPIO_PIN_RESET : GPIO_PIN_SET); // d
    HAL_GPIO_WritePin(GPIOB, GPIO_PIN_4,  seg_matrix[floor_num][4] ? GPIO_PIN_RESET : GPIO_PIN_SET); // e
    HAL_GPIO_WritePin(GPIOD, GPIO_PIN_1,  seg_matrix[floor_num][5] ? GPIO_PIN_RESET : GPIO_PIN_SET); // f
    HAL_GPIO_WritePin(GPIOB, GPIO_PIN_8,  seg_matrix[floor_num][6] ? GPIO_PIN_RESET : GPIO_PIN_SET); // g
}

void Set_Motor_Direction(uint8_t direction) {
    if (direction == 1) { // UP
        HAL_GPIO_WritePin(GPIOC, GPIO_PIN_5, GPIO_PIN_SET);   // IN3 = HIGH
        HAL_GPIO_WritePin(GPIOC, GPIO_PIN_6, GPIO_PIN_RESET); // IN4 = LOW
    } else if (direction == 2) { // DOWN
        HAL_GPIO_WritePin(GPIOC, GPIO_PIN_5, GPIO_PIN_RESET); // IN3 = LOW
        HAL_GPIO_WritePin(GPIOC, GPIO_PIN_6, GPIO_PIN_SET);   // IN4 = HIGH
    } else { // STOP
        HAL_GPIO_WritePin(GPIOC, GPIO_PIN_5, GPIO_PIN_RESET); // IN3 = LOW
        HAL_GPIO_WritePin(GPIOC, GPIO_PIN_6, GPIO_PIN_RESET); // IN4 = LOW
        __HAL_TIM_SET_COMPARE(&htim3, TIM_CHANNEL_2, 0);      // ENB PWM = 0
    }
}

void Start_Motor_With_Ramp(uint8_t direction) {
    Set_Motor_Direction(direction);
    for (uint16_t speed = MOTOR_MIN_SPEED; speed <= MOTOR_MAX_SPEED; speed += 5) {
        __HAL_TIM_SET_COMPARE(&htim3, TIM_CHANNEL_2, speed);
        HAL_Delay(20);
    }
}

void Clear_All_Floor_LEDs(void) {
    HAL_GPIO_WritePin(GPIOC, GPIO_PIN_4,  GPIO_PIN_RESET); // Floor 1 LED
    HAL_GPIO_WritePin(GPIOE, GPIO_PIN_12, GPIO_PIN_RESET); // Floor 2 LED
    HAL_GPIO_WritePin(GPIOA, GPIO_PIN_2,  GPIO_PIN_RESET); // Floor 3 LED
}
/* USER CODE END PFP */

int main(void)
{
  HAL_Init();
  SystemClock_Config();
  MX_GPIO_Init();
  MX_TIM3_Init();

  /* USER CODE BEGIN 2 */
    Clear_All_Floor_LEDs();
    HAL_GPIO_WritePin(GPIOE, GPIO_PIN_8,  GPIO_PIN_RESET); // Up LED
    HAL_GPIO_WritePin(GPIOB, GPIO_PIN_10, GPIO_PIN_RESET); // Down LED
    HAL_GPIO_WritePin(GPIOC, GPIO_PIN_2,  GPIO_PIN_RESET); // Emergency LED
    Update_7Segment(0);                                    // Display Blank
    Set_Motor_Direction(0);                                // Motor halted

    HAL_TIM_PWM_Start(&htim3, TIM_CHANNEL_2);
    HAL_Delay(1500); // Power-on safety window

    // SMART HOMING CONFIGURATION
    if (HAL_GPIO_ReadPin(GPIOA, GPIO_PIN_0) == GPIO_PIN_RESET) current_floor = FLOOR_1;
    else if (HAL_GPIO_ReadPin(GPIOA, GPIO_PIN_4) == GPIO_PIN_RESET) current_floor = FLOOR_2;
    else if (HAL_GPIO_ReadPin(GPIOA, GPIO_PIN_6) == GPIO_PIN_RESET) current_floor = FLOOR_3;

    if (current_floor != 0) {
        target_floor = FLOOR_1;
        display_initialized = 1;
    }
    else {
        Set_Motor_Direction(2); // Drive DOWN to hunt for home
        __HAL_TIM_SET_COMPARE(&htim3, TIM_CHANNEL_2, MOTOR_MIN_SPEED);

        uint32_t hunting_timeout = HAL_GetTick();
        uint8_t floor_found = 0;

        while (!floor_found) {
            if (HAL_GPIO_ReadPin(GPIOA, GPIO_PIN_0) == GPIO_PIN_RESET) { current_floor = FLOOR_1; floor_found = 1; }
            if (HAL_GPIO_ReadPin(GPIOA, GPIO_PIN_4) == GPIO_PIN_RESET) { current_floor = FLOOR_2; floor_found = 1; }
            if (HAL_GPIO_ReadPin(GPIOA, GPIO_PIN_6) == GPIO_PIN_RESET) { current_floor = FLOOR_3; floor_found = 1; }

            if ((HAL_GetTick() - hunting_timeout) > 4000) {
                Set_Motor_Direction(1); // Reverse UP if stuck below Floor 1
                __HAL_TIM_SET_COMPARE(&htim3, TIM_CHANNEL_2, MOTOR_MIN_SPEED);

                while (!floor_found) {
                    if (HAL_GPIO_ReadPin(GPIOA, GPIO_PIN_0) == GPIO_PIN_RESET) { current_floor = FLOOR_1; floor_found = 1; }
                    if (HAL_GPIO_ReadPin(GPIOA, GPIO_PIN_4) == GPIO_PIN_RESET) { current_floor = FLOOR_2; floor_found = 1; }
                    if (HAL_GPIO_ReadPin(GPIOA, GPIO_PIN_6) == GPIO_PIN_RESET) { current_floor = FLOOR_3; floor_found = 1; }
                }
            }
        }
        Set_Motor_Direction(0); // Brake immediately
        target_floor = FLOOR_1;
        display_initialized = 1;
    }
  /* USER CODE END 2 */

  while (1)
  {
    /* ---------------- 1. PRIORITY EMERGENCY MONITORING (PD9) ---------------- */
    if (HAL_GPIO_ReadPin(GPIOD, GPIO_PIN_9) == GPIO_PIN_RESET)
    {
        HAL_Delay(40);
        if (HAL_GPIO_ReadPin(GPIOD, GPIO_PIN_9) == GPIO_PIN_RESET)
        {
            emergency_active = 1;
            target_floor = FLOOR_1;
            if (current_floor > FLOOR_1) {
                HAL_GPIO_WritePin(GPIOC, GPIO_PIN_2, GPIO_PIN_SET);
            }
        }
    }

    /* ---------------- 2. PASSENGER INPUT STORAGE MATRIX ---------------- */
    if (!emergency_active && !is_moving)
    {
        if (HAL_GPIO_ReadPin(GPIOB, GPIO_PIN_1) == GPIO_PIN_RESET)   target_floor = FLOOR_1;
        else if (HAL_GPIO_ReadPin(GPIOE, GPIO_PIN_15) == GPIO_PIN_RESET) target_floor = FLOOR_2;
        else if (HAL_GPIO_ReadPin(GPIOD, GPIO_PIN_0) == GPIO_PIN_RESET)  target_floor = FLOOR_3;

        else if (HAL_GPIO_ReadPin(GPIOE, GPIO_PIN_5) == GPIO_PIN_RESET)  target_floor = FLOOR_1;
        else if (HAL_GPIO_ReadPin(GPIOA, GPIO_PIN_7) == GPIO_PIN_RESET)  target_floor = FLOOR_2;
        else if (HAL_GPIO_ReadPin(GPIOE, GPIO_PIN_6) == GPIO_PIN_RESET)  target_floor = FLOOR_2;
        else if (HAL_GPIO_ReadPin(GPIOA, GPIO_PIN_1) == GPIO_PIN_RESET)  target_floor = FLOOR_3;
    }

    /* ---------------- 3. CORE MOTION & REALTIME SENSOR SAMPLING ENGINE ---------------- */
    if (display_initialized)
    {
        Update_7Segment(current_floor);

        // CASE A: ELEVATOR RUNNING UPWARDS
        if (current_floor < target_floor)
        {
            if (!is_moving) {
                Clear_All_Floor_LEDs();
                HAL_GPIO_WritePin(GPIOE, GPIO_PIN_8, GPIO_PIN_SET);
                HAL_GPIO_WritePin(GPIOB, GPIO_PIN_10, GPIO_PIN_RESET);
                Start_Motor_With_Ramp(1);
                is_moving = 1;
            }

            uint8_t target_pin_state = 1;
            if (current_floor + 1 == FLOOR_2)      target_pin_state = HAL_GPIO_ReadPin(GPIOA, GPIO_PIN_4);
            else if (current_floor + 1 == FLOOR_3) target_pin_state = HAL_GPIO_ReadPin(GPIOA, GPIO_PIN_6);

            if (target_pin_state == GPIO_PIN_RESET) {
                f2_debounce++;
                if (f2_debounce >= IR_THRESHOLD) {
                    f2_debounce = 0;

                    // FIX: Mechanical Alignment occurs instantly for ANY floor hit
                    if (current_floor + 1 == FLOOR_2) HAL_Delay(f2_align_delay);
                    if (current_floor + 1 == FLOOR_3) HAL_Delay(f3_align_delay);

                    current_floor++; // Update floor position tracker

                    if (current_floor == target_floor) {
                        Set_Motor_Direction(0); // Brake at destination
                        is_moving = 0;
                    }
                }
            } else {
                f2_debounce = 0;
            }
        }

        // CASE B: ELEVATOR RUNNING DOWNWARDS
        else if (current_floor > target_floor)
        {
            if (!is_moving) {
                Clear_All_Floor_LEDs();
                HAL_GPIO_WritePin(GPIOE, GPIO_PIN_8, GPIO_PIN_RESET);
                HAL_GPIO_WritePin(GPIOB, GPIO_PIN_10, GPIO_PIN_SET);
                Start_Motor_With_Ramp(2);
                is_moving = 1;
            }

            uint8_t target_pin_state = 1;
            if (current_floor - 1 == FLOOR_2)      target_pin_state = HAL_GPIO_ReadPin(GPIOA, GPIO_PIN_4);
            else if (current_floor - 1 == FLOOR_1) target_pin_state = HAL_GPIO_ReadPin(GPIOA, GPIO_PIN_0);

            if (target_pin_state == GPIO_PIN_RESET) {
                f1_debounce++;
                if (f1_debounce >= IR_THRESHOLD) {
                    f1_debounce = 0;

                    if (emergency_active) {
                        // FIX: Run alignment before braking for intermediate steps during emergency
                        if (current_floor - 1 == FLOOR_2) HAL_Delay(f2_align_delay);
                        if (current_floor - 1 == FLOOR_1) HAL_Delay(f1_align_delay);

                        current_floor--;
                        Set_Motor_Direction(0);
                        Update_7Segment(current_floor);
                        HAL_Delay(500); // Hold pause window
                        if (current_floor != target_floor) {
                            Set_Motor_Direction(2);
                            __HAL_TIM_SET_COMPARE(&htim3, TIM_CHANNEL_2, MOTOR_MAX_SPEED);
                        }
                    }
                    else {
                        // FIX: Run alignment for standard non-emergency downward steps
                        if (current_floor - 1 == FLOOR_2) HAL_Delay(f2_align_delay);
                        if (current_floor - 1 == FLOOR_1) HAL_Delay(f1_align_delay);

                        current_floor--;
                    }

                    if (current_floor == target_floor) {
                        Set_Motor_Direction(0);
                        is_moving = 0;
                    }
                }
            } else {
                f1_debounce = 0;
            }
        }

        // CASE C: CABIN IDLE AT TARGET POSITION
        else
        {
            is_moving = 0;
            Set_Motor_Direction(0);
            HAL_GPIO_WritePin(GPIOE, GPIO_PIN_8,  GPIO_PIN_RESET);
            HAL_GPIO_WritePin(GPIOB, GPIO_PIN_10, GPIO_PIN_RESET);
            Clear_All_Floor_LEDs();

            if (emergency_active && current_floor == FLOOR_1)
            {
                HAL_GPIO_WritePin(GPIOC, GPIO_PIN_2, GPIO_PIN_RESET);
                emergency_active = 0;
            }
        }

        if (is_moving || current_floor != target_floor) {
            if (target_floor == FLOOR_1) HAL_GPIO_WritePin(GPIOC, GPIO_PIN_4,  GPIO_PIN_SET);
            if (target_floor == FLOOR_2) HAL_GPIO_WritePin(GPIOE, GPIO_PIN_12, GPIO_PIN_SET);
            if (target_floor == FLOOR_3) HAL_GPIO_WritePin(GPIOA, GPIO_PIN_2,  GPIO_PIN_SET);
        }
    }
  }
}

void SystemClock_Config(void) {
  RCC_OscInitTypeDef RCC_OscInitStruct = {0};
  RCC_ClkInitTypeDef RCC_ClkInitStruct = {0};
  __HAL_RCC_PWR_CLK_ENABLE();
  __HAL_PWR_VOLTAGESCALING_CONFIG(PWR_REGULATOR_VOLTAGE_SCALE1);
  RCC_OscInitStruct.OscillatorType = RCC_OSCILLATORTYPE_HSI;
  RCC_OscInitStruct.HSIState = RCC_HSI_ON;
  RCC_OscInitStruct.HSICalibrationValue = RCC_HSICALIBRATION_DEFAULT;
  RCC_OscInitStruct.PLL.PLLState = RCC_PLL_ON;
  RCC_OscInitStruct.PLL.PLLSource = RCC_PLLSOURCE_HSI;
  RCC_OscInitStruct.PLL.PLLM = 8;
  RCC_OscInitStruct.PLL.PLLN = 168;
  RCC_OscInitStruct.PLL.PLLP = RCC_PLLP_DIV2;
  RCC_OscInitStruct.PLL.PLLQ = 7;
  HAL_RCC_OscConfig(&RCC_OscInitStruct);
  RCC_ClkInitStruct.ClockType = RCC_CLOCKTYPE_HCLK|RCC_CLOCKTYPE_SYSCLK|RCC_CLOCKTYPE_PCLK1|RCC_CLOCKTYPE_PCLK2;
  RCC_ClkInitStruct.SYSCLKSource = RCC_SYSCLKSOURCE_PLLCLK;
  RCC_ClkInitStruct.AHBCLKDivider = RCC_SYSCLK_DIV1;
  RCC_ClkInitStruct.APB1CLKDivider = RCC_HCLK_DIV4;
  RCC_ClkInitStruct.APB2CLKDivider = RCC_HCLK_DIV2;
  HAL_RCC_ClockConfig(&RCC_ClkInitStruct, FLASH_LATENCY_5);
}

static void MX_TIM3_Init(void) {
  TIM_ClockConfigTypeDef sClockSourceConfig = {0};
  TIM_MasterConfigTypeDef sMasterConfig = {0};
  TIM_OC_InitTypeDef sConfigOC = {0};
  htim3.Instance = TIM3;
  htim3.Init.Prescaler = 167;
  htim3.Init.CounterMode = TIM_COUNTERMODE_UP;
  htim3.Init.Period = 999;
  htim3.Init.ClockDivision = TIM_CLOCKDIVISION_DIV1;
  htim3.Init.AutoReloadPreload = TIM_AUTORELOAD_PRELOAD_DISABLE;
  HAL_TIM_Base_Init(&htim3);
  sClockSourceConfig.ClockSource = TIM_CLOCKSOURCE_INTERNAL;
  HAL_TIM_ConfigClockSource(&htim3, &sClockSourceConfig);
  HAL_TIM_PWM_Init(&htim3);
  sMasterConfig.MasterOutputTrigger = TIM_TRGO_RESET;
  sMasterConfig.MasterSlaveMode = TIM_MASTERSLAVEMODE_DISABLE;
  HAL_TIMEx_MasterConfigSynchronization(&htim3, &sMasterConfig);
  sConfigOC.OCMode = TIM_OCMODE_PWM1;
  sConfigOC.Pulse = 0;
  sConfigOC.OCPolarity = TIM_OCPOLARITY_HIGH;
  sConfigOC.OCFastMode = TIM_OCFAST_DISABLE;
  HAL_TIM_PWM_ConfigChannel(&htim3, &sConfigOC, TIM_CHANNEL_2);
  HAL_TIM_MspPostInit(&htim3);
}

static void MX_GPIO_Init(void) {
  GPIO_InitTypeDef GPIO_InitStruct = {0};
  __HAL_RCC_GPIOE_CLK_ENABLE();
  __HAL_RCC_GPIOC_CLK_ENABLE();
  __HAL_RCC_GPIOH_CLK_ENABLE();
  __HAL_RCC_GPIOA_CLK_ENABLE();
  __HAL_RCC_GPIOB_CLK_ENABLE();
  __HAL_RCC_GPIOD_CLK_ENABLE();

  HAL_GPIO_WritePin(GPIOE, GPIO_PIN_2, GPIO_PIN_SET);
  HAL_GPIO_WritePin(GPIOE, GPIO_PIN_8|GPIO_PIN_12|GPIO_PIN_1, GPIO_PIN_RESET);
  HAL_GPIO_WritePin(GPIOC, GPIO_PIN_2|GPIO_PIN_4|GPIO_PIN_5|GPIO_PIN_6, GPIO_PIN_RESET);
  HAL_GPIO_WritePin(GPIOA, GPIO_PIN_2, GPIO_PIN_RESET);
  HAL_GPIO_WritePin(GPIOB, GPIO_PIN_10, GPIO_PIN_RESET);
  HAL_GPIO_WritePin(GPIOD, GPIO_PIN_1|GPIO_PIN_5, GPIO_PIN_SET);
  HAL_GPIO_WritePin(GPIOA, GPIO_PIN_10, GPIO_PIN_SET);
  HAL_GPIO_WritePin(GPIOB, GPIO_PIN_3|GPIO_PIN_4|GPIO_PIN_8, GPIO_PIN_SET);

  GPIO_InitStruct.Pin = GPIO_PIN_2|GPIO_PIN_8|GPIO_PIN_12|GPIO_PIN_1;
  GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
  HAL_GPIO_Init(GPIOE, &GPIO_InitStruct);

  GPIO_InitStruct.Pin = GPIO_PIN_5|GPIO_PIN_6|GPIO_PIN_15;
  GPIO_InitStruct.Mode = GPIO_MODE_INPUT;
  GPIO_InitStruct.Pull = GPIO_PULLUP;
  HAL_GPIO_Init(GPIOE, &GPIO_InitStruct);

  GPIO_InitStruct.Pin = GPIO_PIN_2|GPIO_PIN_4|GPIO_PIN_5|GPIO_PIN_6;
  GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
  HAL_GPIO_Init(GPIOC, &GPIO_InitStruct);

  GPIO_InitStruct.Pin = GPIO_PIN_0|GPIO_PIN_1|GPIO_PIN_4|GPIO_PIN_6|GPIO_PIN_7;
  GPIO_InitStruct.Mode = GPIO_MODE_INPUT;
  GPIO_InitStruct.Pull = GPIO_PULLUP;
  HAL_GPIO_Init(GPIOA, &GPIO_InitStruct);

  GPIO_InitStruct.Pin = GPIO_PIN_2|GPIO_PIN_10;
  GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
  HAL_GPIO_Init(GPIOA, &GPIO_InitStruct);

  GPIO_InitStruct.Pin = GPIO_PIN_1;
  GPIO_InitStruct.Mode = GPIO_MODE_INPUT;
  GPIO_InitStruct.Pull = GPIO_PULLUP;
  HAL_GPIO_Init(GPIOB, &GPIO_InitStruct);

  GPIO_InitStruct.Pin = GPIO_PIN_10|GPIO_PIN_3|GPIO_PIN_4|GPIO_PIN_8;
  GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
  HAL_GPIO_Init(GPIOB, &GPIO_InitStruct);

  GPIO_InitStruct.Pin = GPIO_PIN_9|GPIO_PIN_0;
  GPIO_InitStruct.Mode = GPIO_MODE_INPUT;
  GPIO_InitStruct.Pull = GPIO_PULLUP;
  HAL_GPIO_Init(GPIOD, &GPIO_InitStruct);

  GPIO_InitStruct.Pin = GPIO_PIN_1|GPIO_PIN_5;
  GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
  HAL_GPIO_Init(GPIOD, &GPIO_InitStruct);
}
