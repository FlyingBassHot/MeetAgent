#include "espui_board.h"

#include "hal_stub.h"

/*
 * The stub board deliberately contains no GPIO numbers, LCD controller or
 * touch controller assumptions. Replace it with a board-specific bundle after
 * the schematic and BSP examples are verified.
 */
const espui_board_hal_t *espui_board_stub(void)
{
    static espui_board_hal_t board;
    board.board_id = "stub-headless";
    board.display = espui_hal_display_stub();
    board.touch = espui_hal_touch_stub();
    board.input = espui_hal_input_stub();
    board.power = espui_hal_power_stub();
    return &board;
}
