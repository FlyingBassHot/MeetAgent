/*
 * Host-test stub for the FreeRTOS task API used by ESPUI.
 */
#pragma once

#include "freertos/FreeRTOS.h"

typedef void *TaskHandle_t;
typedef void (*TaskFunction_t)(void *);

BaseType_t xTaskCreatePinnedToCore(TaskFunction_t task, const char *name,
                                   uint32_t stack_depth, void *arg,
                                   UBaseType_t priority, TaskHandle_t *handle,
                                   BaseType_t core_id);
void vTaskDelay(TickType_t ticks);
TickType_t xTaskGetTickCount(void);
TaskHandle_t xTaskGetCurrentTaskHandle(void);
void vTaskDelete(TaskHandle_t task);
