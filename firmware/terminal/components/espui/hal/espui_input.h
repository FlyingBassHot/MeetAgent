/*
 * Raw input HAL contract: encoder, buttons and optional touch-to-button
 * mapping. The input task converts raw GPIO/PCNT events into espui_input_event_t
 * and posts them to the UI queue; it never touches LVGL.
 */
#pragma once

#include <stdint.h>

#include "esp_err.h"
#include "espui_types.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    esp_err_t (*init)(void);
    esp_err_t (*deinit)(void);

    /*
     * Blocks up to timeout_ms for one abstract event.
     * Returns ESP_ERR_TIMEOUT when no event arrived.
     */
    esp_err_t (*read_event)(espui_input_event_t *event, uint32_t timeout_ms);
} espui_input_hal_t;

#ifdef __cplusplus
}
#endif
