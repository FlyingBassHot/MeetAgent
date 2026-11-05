/*
 * Host-test stub for the FreeRTOS kernel types used by ESPUI.
 */
#pragma once

#include <stdint.h>

typedef uint32_t TickType_t;
typedef int BaseType_t;
typedef unsigned int UBaseType_t;

#define pdFALSE 0
#define pdTRUE 1
#define pdPASS 1
#define pdFAIL 0

#define portMAX_DELAY 0xFFFFFFFFu
#define pdMS_TO_TICKS(ms) ((TickType_t)(ms))
#define pdTICKS_TO_MS(ticks) ((uint32_t)(ticks))
#define tskNO_AFFINITY 0x7FFFFFFF
