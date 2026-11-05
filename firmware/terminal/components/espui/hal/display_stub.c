/*
 * Headless display stub. It reports the target logical resolution but does not
 * drive a panel. Replaced by the board BSP once the LCD/touch controller are
 * confirmed from the Waveshare schematic.
 */
#include "espui_display.h"

#include "hal_stub.h"

static const espui_display_info_t s_info = {
    .panel_name = "headless-stub-320x480",
    .width = 320,
    .height = 480,
    .orientation = ESPUI_ORIENTATION_PORTRAIT,
    .color_depth = 16,
    .supports_touch = true,
};

static esp_err_t display_init(void)
{
    return ESP_OK;
}

static esp_err_t display_deinit(void)
{
    return ESP_OK;
}

static const espui_display_info_t *display_get_info(void)
{
    return &s_info;
}

static esp_err_t display_set_backlight(uint8_t percent)
{
    (void)percent;
    return ESP_OK;
}

static const espui_display_hal_t s_display = {
    .init = display_init,
    .deinit = display_deinit,
    .get_info = display_get_info,
    .lock = NULL, /* single-threaded headless bring-up */
    .unlock = NULL,
    .set_backlight = display_set_backlight,
};

const espui_display_hal_t *espui_hal_display_stub(void)
{
    return &s_display;
}
