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
 * Headless board for host tests and board-independent bring-up: 480x480,
 * RGB565, no panel flush, no local GPIO. The real LCD-4 V4 board uses the
 * official Waveshare BSP through main/board_lcd4.c.
 */
const espui_board_hal_t *espui_board_stub(void);

#ifdef __cplusplus
}
#endif
