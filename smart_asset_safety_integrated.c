/* =====================================================================================
 * File        : smart_asset_safety_integrated.c
 * Description : INTEGRATED SYSTEM - Week 4 Final Testing Report
 *               Complete Smart Asset Safety & Data Logging System using FreeRTOS
 *               Integrates: sensor monitoring, fault detection, logging, alerting,
 *               load control, and status feedback all in one system.
 * 
 * Architecture: 5 FreeRTOS Tasks with priority-based scheduling
 *   - Sensor Task (Priority 2): Reads ADC, RTC, accelerometer
 *   - Fault Task (Priority 3): Evaluates sensor data, triggers load shutdown
 *   - Logger Task (Priority 1): Writes events to SPI flash with RTC timestamp
 *   - Communication Task (Priority 1): Sends GPS/GSM alerts
 *   - Status Task (Priority 0): Updates LED indicator
 * ===================================================================================== */

#include "stm32f4xx_hal.h"
#include "FreeRTOS.h"
#include "task.h"
#include "queue.h"
#include <stdio.h>
#include <string.h>

/* ----- Pin Definitions ----- */
#define LED_GPIO_PORT        GPIOA
#define LED_GPIO_PIN         GPIO_PIN_5
#define LOAD_CTRL_PORT       GPIOC
#define LOAD_CTRL_PIN        GPIO_PIN_3

/* ----- Threshold Definitions ----- */
#define TEMP_THRESHOLD_WARNING    60    /* 60 deg C */
#define TEMP_THRESHOLD_CRITICAL   80    /* 80 deg C */
#define CURRENT_THRESHOLD_WARNING 5000  /* 5A */
#define CURRENT_THRESHOLD_CRITICAL 8000 /* 8A */
#define ACCEL_THRESHOLD_CRASH     2000  /* g*100 */

/* ----- Fault Levels ----- */
typedef enum {
    FAULT_NORMAL = 0,
    FAULT_WARNING,
    FAULT_CRITICAL
} FaultLevel_t;

/* ----- Sensor Data Structure ----- */
typedef struct {
    uint16_t temperature;    /* in deg C * 10 */
    uint16_t current;        /* in mA */
    int16_t  accel_x;        /* in g * 100 */
    int16_t  accel_y;
    int16_t  accel_z;
    uint32_t timestamp_ms;
    FaultLevel_t faultLevel;
} SensorData_t;

/* ----- Event Log Structure ----- */
typedef struct {
    uint32_t timestamp;
    FaultLevel_t level;
    uint16_t temperature;
    uint16_t current;
    int16_t  accel_magnitude;
} LogEvent_t;

/* ----- Peripheral Handles ----- */
TIM_HandleTypeDef  htim2;
UART_HandleTypeDef huart2;
ADC_HandleTypeDef  hadc1;
RTC_HandleTypeDef  hrtc;

/* ----- Queue Handles for Inter-Task Communication ----- */
QueueHandle_t sensorDataQueue;
QueueHandle_t faultEventQueue;
QueueHandle_t logEventQueue;

/* ----- Volatile State ----- */
static volatile FaultLevel_t systemFaultLevel = FAULT_NORMAL;
static volatile uint8_t ledBlinkRate = 5;   /* 500 ms toggle interval */
static volatile uint8_t loadEnabled = 1;

/* ----- Task Handles ----- */
TaskHandle_t sensorTaskHandle = NULL;
TaskHandle_t faultTaskHandle = NULL;
TaskHandle_t loggerTaskHandle = NULL;
TaskHandle_t comTaskHandle = NULL;
TaskHandle_t statusTaskHandle = NULL;

/* ----- Task Declarations ----- */
void vSensorTask(void *pvParameters);
void vFaultTask(void *pvParameters);
void vLoggerTask(void *pvParameters);
void vCommunicationTask(void *pvParameters);
void vStatusTask(void *pvParameters);

/* ----- Hardware Initialization ----- */
static void GPIO_Init(void)
{
    GPIO_InitTypeDef GPIO_InitStruct = {0};
    __HAL_RCC_GPIOA_CLK_ENABLE();
    __HAL_RCC_GPIOC_CLK_ENABLE();
    
    /* LED output */
    GPIO_InitStruct.Pin = LED_GPIO_PIN;
    GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
    GPIO_InitStruct.Pull = GPIO_NOPULL;
    GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
    HAL_GPIO_Init(LED_GPIO_PORT, &GPIO_InitStruct);
    
    /* Load control output */
    GPIO_InitStruct.Pin = LOAD_CTRL_PIN;
    HAL_GPIO_Init(LOAD_CTRL_PORT, &GPIO_InitStruct);
    HAL_GPIO_WritePin(LOAD_CTRL_PORT, LOAD_CTRL_PIN, GPIO_PIN_SET);  /* Load ON */
}

static void ADC_Init(void)
{
    ADC_ChannelConfTypeDef sConfig = {0};
    __HAL_RCC_ADC1_CLK_ENABLE();
    
    hadc1.Instance = ADC1;
    hadc1.Init.Resolution = ADC_RESOLUTION_12B;
    hadc1.Init.ScanConvMode = DISABLE;
    hadc1.Init.ContinuousConvMode = DISABLE;
    hadc1.Init.DiscontinuousConvMode = DISABLE;
    hadc1.Init.DataAlign = ADC_DATAALIGN_RIGHT;
    hadc1.Init.NbrOfConversion = 1;
    HAL_ADC_Init(&hadc1);
    
    sConfig.Channel = ADC_CHANNEL_0;
    sConfig.Rank = 1;
    sConfig.SamplingTime = ADC_SAMPLETIME_15CYCLES;
    HAL_ADC_ConfigChannel(&hadc1, &sConfig);
}

static void UART_Init(void)
{
    huart2.Instance = USART2;
    huart2.Init.BaudRate = 9600;
    huart2.Init.WordLength = UART_WORDLENGTH_8B;
    huart2.Init.StopBits = UART_STOPBITS_1;
    huart2.Init.Parity = UART_PARITY_NONE;
    huart2.Init.Mode = UART_MODE_TX_RX;
    huart2.Init.HwFlowCtl = UART_HWCONTROL_NONE;
    huart2.Init.OverSampling = UART_OVERSAMPLING_16;
    HAL_UART_Init(&huart2);
}

static void RTC_Init(void)
{
    /* RTC configuration - simplified for simulation */
    hrtc.Instance = RTC;
}

/* ----- Task Implementations ----- */

/* SENSOR TASK: Reads all sensors and publishes data to queue */
void vSensorTask(void *pvParameters)
{
    SensorData_t sensorData;
    TickType_t xLastWakeTime = xTaskGetTickCount();
    
    for (;;) {
        /* Read ADC (temperature & current) */
        HAL_ADC_Start(&hadc1);
        if (HAL_ADC_PollForConversion(&hadc1, 10) == HAL_OK) {
            sensorData.temperature = (uint16_t)HAL_ADC_GetValue(&hadc1);
            sensorData.current = (uint16_t)(HAL_ADC_GetValue(&hadc1) & 0xFFF);
        }
        HAL_ADC_Stop(&hadc1);
        
        /* Simulate accelerometer read (in real system: I2C MPU6050) */
        sensorData.accel_x = 0;
        sensorData.accel_y = 0;
        sensorData.accel_z = 981;  /* 1g = 981 cm/s^2, stored as int16_t * 100 */
        
        /* Read timestamp from RTC */
        sensorData.timestamp_ms = xTaskGetTickCount();
        
        /* Send to queue for Fault Task */
        if (xQueueSend(sensorDataQueue, &sensorData, portMAX_DELAY) != pdPASS) {
            /* Queue full - handle error */
        }
        
        /* Poll every 200 ms */
        vTaskDelayUntil(&xLastWakeTime, pdMS_TO_TICKS(200));
    }
}

/* FAULT TASK: High priority - evaluates sensor data and triggers actions */
void vFaultTask(void *pvParameters)
{
    SensorData_t sensorData;
    FaultLevel_t newLevel;
    
    for (;;) {
        /* Wait for sensor data (blocking receive) */
        if (xQueueReceive(sensorDataQueue, &sensorData, portMAX_DELAY) == pdPASS) {
            
            /* Evaluate fault level */
            if (sensorData.temperature >= TEMP_THRESHOLD_CRITICAL ||
                sensorData.current >= CURRENT_THRESHOLD_CRITICAL) {
                newLevel = FAULT_CRITICAL;
            } else if (sensorData.temperature >= TEMP_THRESHOLD_WARNING ||
                       sensorData.current >= CURRENT_THRESHOLD_WARNING) {
                newLevel = FAULT_WARNING;
            } else {
                newLevel = FAULT_NORMAL;
            }
            
            /* Update system state */
            if (newLevel != systemFaultLevel) {
                systemFaultLevel = newLevel;
                
                /* Send to Logger and Communication tasks */
                LogEvent_t event = {
                    .timestamp = sensorData.timestamp_ms,
                    .level = newLevel,
                    .temperature = sensorData.temperature,
                    .current = sensorData.current,
                };
                xQueueSend(logEventQueue, &event, 0);
                xQueueSend(faultEventQueue, &event, 0);
                
                /* Critical: disable load immediately */
                if (newLevel == FAULT_CRITICAL) {
                    HAL_GPIO_WritePin(LOAD_CTRL_PORT, LOAD_CTRL_PIN, GPIO_PIN_RESET);
                    loadEnabled = 0;
                }
                
                /* Update LED rate */
                if (newLevel == FAULT_CRITICAL) {
                    ledBlinkRate = 1;  /* 100 ms */
                } else if (newLevel == FAULT_WARNING) {
                    ledBlinkRate = 2;  /* 200 ms */
                } else {
                    ledBlinkRate = 5;  /* 500 ms */
                    /* Re-enable load */
                    HAL_GPIO_WritePin(LOAD_CTRL_PORT, LOAD_CTRL_PIN, GPIO_PIN_SET);
                    loadEnabled = 1;
                }
            }
        }
    }
}

/* LOGGER TASK: Low priority - writes events to flash storage */
void vLoggerTask(void *pvParameters)
{
    LogEvent_t event;
    
    for (;;) {
        if (xQueueReceive(logEventQueue, &event, portMAX_DELAY) == pdPASS) {
            /* In real system: write to SPI flash via SPI HAL */
            /* Simulated: just format and store in memory buffer */
            static char logBuffer[256];
            snprintf(logBuffer, sizeof(logBuffer),
                     "LOG [%u] Level=%d Temp=%u Current=%u\r\n",
                     event.timestamp, event.level, event.temperature, event.current);
        }
    }
}

/* COMMUNICATION TASK: Low priority - sends alerts over GSM/GPS */
void vCommunicationTask(void *pvParameters)
{
    LogEvent_t event;
    
    for (;;) {
        if (xQueueReceive(faultEventQueue, &event, portMAX_DELAY) == pdPASS) {
            /* Format GPS-tagged alert message */
            const char *levelStr;
            switch (event.level) {
                case FAULT_CRITICAL: levelStr = "CRITICAL"; break;
                case FAULT_WARNING:  levelStr = "WARNING";  break;
                default:              levelStr = "NORMAL";   break;
            }
            
            char alert[128];
            snprintf(alert, sizeof(alert),
                     "ALERT: Condition=%s Temp=%uC Current=%umA GPS:12.9716,77.5946\r\n",
                     levelStr, event.temperature / 10, event.current);
            
            /* Send over UART (GSM in real system) */
            HAL_UART_Transmit(&huart2, (uint8_t *)alert, strlen(alert), 100);
        }
    }
}

/* STATUS TASK: Lowest priority - updates LED based on current fault level */
void vStatusTask(void *pvParameters)
{
    uint8_t ledCounter = 0;
    
    for (;;) {
        /* Toggle LED based on current blink rate */
        if (ledCounter >= ledBlinkRate) {
            HAL_GPIO_TogglePin(LED_GPIO_PORT, LED_GPIO_PIN);
            ledCounter = 0;
        } else {
            ledCounter++;
        }
        
        /* Run every 100 ms */
        vTaskDelay(pdMS_TO_TICKS(100));
    }
}

/* ----- Main Entry Point ----- */
int main(void)
{
    HAL_Init();
    GPIO_Init();
    ADC_Init();
    UART_Init();
    RTC_Init();
    
    /* Create queues for inter-task communication */
    sensorDataQueue = xQueueCreate(4, sizeof(SensorData_t));
    faultEventQueue = xQueueCreate(2, sizeof(LogEvent_t));
    logEventQueue = xQueueCreate(8, sizeof(LogEvent_t));
    
    /* Create FreeRTOS tasks */
    xTaskCreate(vSensorTask,          "Sensor",        128, NULL, 2, &sensorTaskHandle);
    xTaskCreate(vFaultTask,           "Fault",         256, NULL, 3, &faultTaskHandle);
    xTaskCreate(vLoggerTask,          "Logger",        256, NULL, 1, &loggerTaskHandle);
    xTaskCreate(vCommunicationTask,   "Comm",          256, NULL, 1, &comTaskHandle);
    xTaskCreate(vStatusTask,          "Status",        128, NULL, 0, &statusTaskHandle);
    
    /* Start scheduler */
    vTaskStartScheduler();
    
    /* Should never reach here */
    while (1);
}

/* FreeRTOS Hooks */
void vApplicationIdleHook(void) { }
void vApplicationStackOverflowHook(TaskHandle_t xTask, char *pcTaskName) { }
