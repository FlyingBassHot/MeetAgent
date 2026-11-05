/*
 * Power/battery HAL contract.
 */
#pragma once

#include <stdbool.h>
#include <stdint.h>

#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    uint8_t battery_percent; /* 0..100, or 255 when unknown. */
    bool charging;
    bool usb_present;
} espui_power_status_t;

typedef struct {
    esp_err_t (*init)(void);
    esp_err_t (*deinit)(void);
    esp_err_t (*get_status)(espui_power_status_t *status);
    esp_err_t (*set_screen_on)(bool on);
} espui_power_hal_t;

#ifdef __cplusplus
}
#endif
