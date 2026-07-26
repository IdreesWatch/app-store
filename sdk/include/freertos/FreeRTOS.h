/* FreeRTOS Stub Headers */
#ifndef FREERTOS_FREERTOS_H
#define FREERTOS_FREERTOS_H

#include <stdint.h>
#include <stddef.h>

typedef void* TaskHandle_t;
typedef void* QueueHandle_t;
typedef void* SemaphoreHandle_t;
typedef void* TimerHandle_t;
typedef uint32_t TickType_t;

#define pdMS_TO_TICKS(ms) (ms)
#define pdTRUE 1
#define pdFALSE 0
#define portMAX_DELAY 0xFFFFFFFF

#define xTaskCreatePinnedToCore(pvTaskCode, pcName, usStackDepth, pvParameters, uxPriority, pvCreatedTask, xCoreID) 1
#define vTaskDelay(pd) 
#define xTaskGetTickCount() 0
#define xQueueCreate(a, b) ((void*)1)
#define xQueueSend(a, b, c) 1
#define xQueueReceive(a, b, c) 1
#define xSemaphoreCreateMutex() ((void*)1)
#define xSemaphoreTake(a, b) 1
#define xSemaphoreGive(a) 1
#define vSemaphoreDelete(a)
#define xTimerCreate(a, b, c, d, e) ((void*)1)
#define xTimerStart(a, b) 1

#endif
