/*
 * Touch HAL contract. Coordinates are in logical display pixels, already
 * rotated by the board port.
 */
#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    uint16_t x;
    uint16_t y;
    uint8_t pressure; /* 0 when the controller reports no pressure. */
    bool pressed;
} espui_touch_point_t;

typedef struct {
    esp_err_t (*init)(void);
    esp_err_t (*deinit)(void);

    /* Returns ESP_OK with *count == 0 when no touch is active. */
    esp_err_t (*read)(espui_touch_point_t *points, size_t max_points,
                      size_t *count);
} espui_touch_hal_t;

#ifdef __cplusplus
}
#endif
