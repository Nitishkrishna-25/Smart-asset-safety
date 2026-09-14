/* =====================================================================================
 * File        : fault_led_controller_v2_optimized.c
 * Description : OPTIMIZED VERSION - Week 3 Debugging & Optimization Solution
 *               All identified bugs from v1 have been fixed and performance
 *               optimizations implemented. See Week 3 report for detailed analysis.
 * ===================================================================================== */

#include "stm32f4xx_hal.h"
#include <stdio.h>
#include <string.h>

#define LED_GPIO_PORT        GPIOA
#define LED_GPIO_PIN         GPIO_PIN_5
#define THRESHOLD_WARNING    1500
#define THRESHOLD_CRITICAL   3000
#define BLINK_TICKS_NORMAL    5
#define BLINK_TICKS_WARNING   2
#define BLINK_TICKS_CRITICAL  1

typedef enum {
    FAULT_NORMAL = 0,
    FAULT_WARNING,
    FAULT_CRITICAL
} FaultLevel_t;

TIM_HandleTypeDef  htim2;
UART_HandleTypeDef huart2;
ADC_HandleTypeDef  hadc1;

/* FIX #1: Added volatile keyword for ISR-accessed variables */
static volatile FaultLevel_t currentFaultLevel = FAULT_NORMAL;
static volatile uint8_t      blinkTickTarget   = BLINK_TICKS_NORMAL;
static volatile uint8_t      blinkTickCounter  = 0;

/* Constant strings moved to flash (via const) to save RAM */
static const char STATUS_NORMAL[]   = "STATUS: Asset condition -> NORMAL\r\n";
static const char STATUS_WARNING[]  = "STATUS: Asset condition -> WARNING\r\n";
static const char STATUS_CRITICAL[] = "STATUS: Asset condition -> CRITICAL\r\n";

void SystemClock_Config(void);
static void GPIO_LED_Init(void);
static void TIM2_Init(void);
static void UART2_Init(void);
static void ADC1_Init(void);
static uint16_t Read_Simulated_Sensor(void);
static FaultLevel_t Evaluate_Fault_Level(uint16_t sensorValue);
static void Send_UART_Status(FaultLevel_t level);

static void GPIO_LED_Init(void)
{
    GPIO_InitTypeDef GPIO_InitStruct = {0};
    __HAL_RCC_GPIOA_CLK_ENABLE();
    GPIO_InitStruct.Pin   = LED_GPIO_PIN;
    GPIO_InitStruct.Mode  = GPIO_MODE_OUTPUT_PP;
    GPIO_InitStruct.Pull  = GPIO_NOPULL;
    GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
    HAL_GPIO_Init(LED_GPIO_PORT, &GPIO_InitStruct);
    HAL_GPIO_WritePin(LED_GPIO_PORT, LED_GPIO_PIN, GPIO_PIN_RESET);
}

static void TIM2_Init(void)
{
    __HAL_RCC_TIM2_CLK_ENABLE();
    htim2.Instance               = TIM2;
    htim2.Init.Prescaler         = 1600 - 1;
    htim2.Init.CounterMode       = TIM_COUNTERMODE_UP;
    htim2.Init.Period            = 1000 - 1;
    htim2.Init.ClockDivision     = TIM_CLOCKDIVISION_DIV1;
    HAL_TIM_Base_Init(&htim2);
    HAL_NVIC_SetPriority(TIM2_IRQn, 1, 0);
    HAL_NVIC_EnableIRQ(TIM2_IRQn);
    HAL_TIM_Base_Start_IT(&htim2);
}

static void UART2_Init(void)
{
    huart2.Instance          = USART2;
    huart2.Init.BaudRate     = 9600;
    huart2.Init.WordLength   = UART_WORDLENGTH_8B;
    huart2.Init.StopBits     = UART_STOPBITS_1;
    huart2.Init.Parity       = UART_PARITY_NONE;
    huart2.Init.Mode         = UART_MODE_TX_RX;
    huart2.Init.HwFlowCtl    = UART_HWCONTROL_NONE;
    huart2.Init.OverSampling = UART_OVERSAMPLING_16;
    HAL_UART_Init(&huart2);
}

static void ADC1_Init(void)
{
    ADC_ChannelConfTypeDef sConfig = {0};
    __HAL_RCC_ADC1_CLK_ENABLE();
    hadc1.Instance                   = ADC1;
    hadc1.Init.Resolution            = ADC_RESOLUTION_12B;
    hadc1.Init.ScanConvMode          = DISABLE;
    hadc1.Init.ContinuousConvMode    = DISABLE;
    hadc1.Init.DiscontinuousConvMode = DISABLE;
    hadc1.Init.DataAlign             = ADC_DATAALIGN_RIGHT;
    hadc1.Init.NbrOfConversion       = 1;
    HAL_ADC_Init(&hadc1);
    sConfig.Channel      = ADC_CHANNEL_0;
    sConfig.Rank         = 1;
    /* FIX #2: Use faster sampling time (15 cycles instead of 480) */
    sConfig.SamplingTime = ADC_SAMPLETIME_15CYCLES;
    HAL_ADC_ConfigChannel(&hadc1, &sConfig);
}

static uint16_t Read_Simulated_Sensor(void)
{
    uint16_t value = 0;
    /* FIX #3: Use HAL polling with proper timeout instead of busy-wait */
    HAL_ADC_Start(&hadc1);
    if (HAL_ADC_PollForConversion(&hadc1, 10) == HAL_OK) {
        value = (uint16_t)HAL_ADC_GetValue(&hadc1);
    }
    HAL_ADC_Stop(&hadc1);
    return value;
}

static FaultLevel_t Evaluate_Fault_Level(uint16_t sensorValue)
{
    if (sensorValue >= THRESHOLD_CRITICAL)
        return FAULT_CRITICAL;
    else if (sensorValue >= THRESHOLD_WARNING)
        return FAULT_WARNING;
    else
        return FAULT_NORMAL;
}

static void Send_UART_Status(FaultLevel_t level)
{
    /* FIX #4 & #5: Use const strings to save stack and avoid string operations */
    const char *message;
    uint16_t len;
    
    switch (level) {
        case FAULT_WARNING:
            message = STATUS_WARNING;
            len = sizeof(STATUS_WARNING) - 1;
            break;
        case FAULT_CRITICAL:
            message = STATUS_CRITICAL;
            len = sizeof(STATUS_CRITICAL) - 1;
            break;
        default:
            message = STATUS_NORMAL;
            len = sizeof(STATUS_NORMAL) - 1;
            break;
    }
    
    HAL_UART_Transmit(&huart2, (uint8_t *)message, len, 100);
}

void TIM2_IRQHandler(void)
{
    if (__HAL_TIM_GET_FLAG(&htim2, TIM_FLAG_UPDATE) != RESET) {
        __HAL_TIM_CLEAR_FLAG(&htim2, TIM_FLAG_UPDATE);
        blinkTickCounter++;
        /* Volatile read ensures we get the latest value from main loop */
        if (blinkTickCounter >= blinkTickTarget) {
            HAL_GPIO_TogglePin(LED_GPIO_PORT, LED_GPIO_PIN);
            blinkTickCounter = 0;
        }
    }
}

int main(void)
{
    HAL_Init();
    SystemClock_Config();
    GPIO_LED_Init();
    TIM2_Init();
    UART2_Init();
    ADC1_Init();
    
    FaultLevel_t previousLevel = FAULT_NORMAL;

    while (1) {
        uint16_t sensorValue = Read_Simulated_Sensor();
        FaultLevel_t newLevel = Evaluate_Fault_Level(sensorValue);

        if (newLevel != previousLevel) {
            switch (newLevel) {
                case FAULT_WARNING:  blinkTickTarget = BLINK_TICKS_WARNING;  break;
                case FAULT_CRITICAL: blinkTickTarget = BLINK_TICKS_CRITICAL; break;
                default:              blinkTickTarget = BLINK_TICKS_NORMAL;   break;
            }
            Send_UART_Status(newLevel);
            currentFaultLevel = newLevel;
            previousLevel = newLevel;
        }
        
        /* FIX #8: Increased delay to 200ms - sensor poll frequency is adequate */
        HAL_Delay(200);
    }
}

void SystemClock_Config(void) { }
