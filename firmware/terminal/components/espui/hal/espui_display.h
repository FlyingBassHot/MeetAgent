/*
 * Display HAL contract.
 *
 * The board port is responsible for the actual LCD controller, backlight,
 * touch registration and LVGL display driver. ESPUI only consumes this
 * board-agnostic view. No GPIO numbers belong here.
 */
#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "esp_err.h"
#include "espui_types.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    const char *panel_name;   /* Board/BSP identifier, not a driver name. */
    uint16_t width;           /* Logical width after rotation. */
    uint16_t height;          /* Logical height after rotation. */
    espui_orientation_t orientation;
    uint8_t color_depth;      /* 16 for RGB565. */
    bool supports_touch;
} espui_display_info_t;

typedef struct {
    esp_err_t (*init)(void);
    esp_err_t (*deinit)(void);
    const espui_display_info_t *(*get_info)(void);

    /*
     * Optional LVGL lock. When NULL the runtime assumes single-threaded access
     * (host tests / headless bring-up). Board ports using esp_lvgl_port should
     * map these to lvgl_port_lock()/lvgl_port_unlock().
     */
    esp_err_t (*lock)(uint32_t timeout_ms);
    void (*unlock)(void);

    /* Optional backlight control, 0..100. NULL means "not controllable". */
    esp_err_t (*set_backlight)(uint8_t percent);
} espui_display_hal_t;

#ifdef __cplusplus
}
#endif
