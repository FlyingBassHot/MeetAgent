#pragma once

#include "esp_err.h"
#include "espui_display.h"

#ifdef __cplusplus
extern "C" {
#endif

/*
 * P1 static Core overlay. Creates one top-layer black overlay and one P1
 * 288x288 RGB565 LVGL canvas in PSRAM. Call once after espui_start().
 */
esp_err_t phosphor_lvgl_port_init(const espui_display_hal_t *display);

#ifdef __cplusplus
}
#endif
