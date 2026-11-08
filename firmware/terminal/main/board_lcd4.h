#pragma once

#include "espui_board.h"

#ifdef __cplusplus
extern "C" {
#endif

/*
 * Waveshare ESP32-S3-Touch-LCD-4 V4.0 board adapter.
 * Uses the official waveshare/esp32_s3_touch_lcd_4 BSP; no BSP source is
 * copied into this repository.
 */
const espui_board_hal_t *espui_board_lcd4(void);

#ifdef __cplusplus
}
#endif
