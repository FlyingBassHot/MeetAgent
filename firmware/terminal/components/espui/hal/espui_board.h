/*
 * Board HAL bundle. Board bring-up lives behind this single struct so themes
 * and UI core never depend on a specific PCB revision.
 */
#pragma once

#include "espui_display.h"
#include "espui_input.h"
#include "espui_power.h"
#include "espui_touch.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    const char *board_id;
    const espui_display_hal_t *display;
    const espui_touch_hal_t *touch;
    const espui_input_hal_t *input;
    const espui_power_hal_t *power;
} espui_board_hal_t;

/*
 * Headless bring-up board: 320x480 portrait, RGB565, no panel flush, no GPIO.
 * It exists so the UI/theme pipeline can boot before real board support lands.
 * Replace it with a board-specific bundle; never edit themes for a new board.
 */
const espui_board_hal_t *espui_board_stub(void);

#ifdef __cplusplus
}
#endif
