/*
 * Waveshare ESP32-S3-Touch-LCD-4 V4.0 board adapter.
 *
 * BSP responsibilities:
 *   - ST7701 480×480 RGB565 panel + RGB/SPI init
 *   - GT911 touch registration in esp_lvgl_port
 *   - CH32V003F4U6 reset/backlight/power helper over I2C
 *   - LVGL port task and mutex
 *
 * This adapter only exposes the ESPUI HAL. It does not copy BSP source and
 * does not use the BSP's LVGL demos or extra fonts.
 */
#include "board_lcd4.h"

#include <string.h>

#include "bsp/esp32_s3_touch_lcd_4.h"
#include "driver/gpio.h"
#include "esp_log.h"
#include "esp_rom_sys.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "lvgl.h"

static const char *TAG = "board_lcd4";

static lv_display_t *s_display;
static lv_indev_t *s_indev;

static const espui_display_info_t s_display_info = {
    .panel_name = "waveshare-esp32-s3-touch-lcd-4-v4",
    .width = 480,
    .height = 480,
    .orientation = ESPUI_ORIENTATION_ANY,
    .color_depth = 16,
    .supports_touch = true,
};

/*
 * Recover a stuck I2C bus before touching CH32/GT911. This follows the
 * official 06_lvgl_demo_v8 example (GPIOs come from the BSP header, not from
 * a local pin guess). It fixes startup after quick resets where CH32 did not
 * reset with the ESP32-S3.
 */
static void board_i2c_recover(void)
{
    gpio_config_t io_conf = {0};
    io_conf.pin_bit_mask = (1ULL << BSP_I2C_SDA) | (1ULL << BSP_I2C_SCL);
    io_conf.mode = GPIO_MODE_INPUT;
    io_conf.pull_up_en = GPIO_PULLUP_ENABLE;
    io_conf.pull_down_en = GPIO_PULLDOWN_DISABLE;
    io_conf.intr_type = GPIO_INTR_DISABLE;
    gpio_config(&io_conf);
    esp_rom_delay_us(20);

    gpio_set_direction(BSP_I2C_SCL, GPIO_MODE_OUTPUT_OD);
    gpio_set_pull_mode(BSP_I2C_SCL, GPIO_PULLUP_ONLY);
    gpio_set_level(BSP_I2C_SCL, 1);
    esp_rom_delay_us(10);

    for (int i = 0; i < 9 && gpio_get_level(BSP_I2C_SDA) == 0; ++i) {
        gpio_set_level(BSP_I2C_SCL, 0);
        esp_rom_delay_us(10);
        gpio_set_level(BSP_I2C_SCL, 1);
        esp_rom_delay_us(10);
    }

    gpio_set_direction(BSP_I2C_SDA, GPIO_MODE_OUTPUT_OD);
    gpio_set_pull_mode(BSP_I2C_SDA, GPIO_PULLUP_ONLY);
    gpio_set_level(BSP_I2C_SDA, 0);
    esp_rom_delay_us(10);
    gpio_set_level(BSP_I2C_SCL, 1);
    esp_rom_delay_us(10);
    gpio_set_level(BSP_I2C_SDA, 1);
    esp_rom_delay_us(10);

    gpio_set_direction(BSP_I2C_SDA, GPIO_MODE_INPUT);
    gpio_set_direction(BSP_I2C_SCL, GPIO_MODE_INPUT);
    gpio_set_pull_mode(BSP_I2C_SDA, GPIO_PULLUP_ONLY);
    gpio_set_pull_mode(BSP_I2C_SCL, GPIO_PULLUP_ONLY);
    vTaskDelay(pdMS_TO_TICKS(20));
}

static esp_err_t display_init(void)
{
    if (s_display != NULL) {
        return ESP_OK;
    }

    board_i2c_recover();

    s_display = bsp_display_start();
    if (s_display == NULL) {
        ESP_LOGE(TAG, "bsp_display_start failed");
        return ESP_FAIL;
    }

    s_indev = bsp_display_get_input_dev();
    if (s_indev == NULL) {
        ESP_LOGW(TAG, "BSP display started without a touch input device");
    }

    /* BSP leaves backlight control to the application after display start. */
    esp_err_t err = bsp_display_brightness_init();
    if (err != ESP_OK) {
        ESP_LOGW(TAG, "brightness init failed: %s", esp_err_to_name(err));
    }
    err = bsp_display_brightness_set(80);
    if (err != ESP_OK) {
        ESP_LOGW(TAG, "brightness set failed: %s", esp_err_to_name(err));
    }

    ESP_LOGI(TAG, "LCD-4 BSP started: %ux%u RGB565", 480, 480);
    return ESP_OK;
}

static esp_err_t display_deinit(void)
{
    /* BSP v3.0.0 has no display stop API; turning the panel off is the
     * cleanest available teardown. Do not free LVGL port resources here. */
    (void)bsp_display_backlight_off();
    ESP_LOGW(TAG, "BSP has no full display teardown; backlight off only");
    return ESP_OK;
}

static const espui_display_info_t *display_get_info(void)
{
    return &s_display_info;
}

static esp_err_t display_lock(uint32_t timeout_ms)
{
    return bsp_display_lock(timeout_ms) ? ESP_OK : ESP_ERR_TIMEOUT;
}

static void display_unlock(void)
{
    bsp_display_unlock();
}

static esp_err_t display_set_backlight(uint8_t percent)
{
    if (percent > 100) {
        percent = 100;
    }
    return bsp_display_brightness_set((int)percent);
}

static const espui_display_hal_t s_display_hal = {
    .init = display_init,
    .deinit = display_deinit,
    .get_info = display_get_info,
    .lock = display_lock,
    .unlock = display_unlock,
    .set_backlight = display_set_backlight,
};

static esp_err_t touch_init(void)
{
    if (s_display == NULL) {
        return ESP_ERR_INVALID_STATE;
    }
    if (s_indev == NULL) {
        s_indev = bsp_display_get_input_dev();
    }
    return s_indev != NULL ? ESP_OK : ESP_ERR_NOT_FOUND;
}

static esp_err_t touch_deinit(void)
{
    /* Touch is owned by the LVGL port; no separate teardown. */
    return ESP_OK;
}

static esp_err_t touch_read(espui_touch_point_t *points, size_t max_points,
                            size_t *count)
{
    if (count == NULL || (points == NULL && max_points > 0)) {
        return ESP_ERR_INVALID_ARG;
    }
    *count = 0;
    if (s_indev == NULL) {
        return ESP_ERR_INVALID_STATE;
    }

    /*
     * LVGL 8.4 public API exposes the last point but not the pressed state.
     * The indev struct is public in this LVGL version; use a single primary
     * point. GT911 supports five points, but esp_lvgl_port 2.9 registers one
     * point for the BSP. Multi-touch is a future BSP/port extension.
     */
    if (s_indev->proc.state != LV_INDEV_STATE_PRESSED || max_points == 0) {
        return ESP_OK;
    }

    lv_point_t point;
    lv_indev_get_point(s_indev, &point);
    int x = point.x;
    int y = point.y;
    if (x < 0) {
        x = 0;
    } else if (x >= (int)s_display_info.width) {
        x = (int)s_display_info.width - 1;
    }
    if (y < 0) {
        y = 0;
    } else if (y >= (int)s_display_info.height) {
        y = (int)s_display_info.height - 1;
    }

    points[0].x = (uint16_t)x;
    points[0].y = (uint16_t)y;
    points[0].pressure = 100;
    points[0].pressed = true;
    *count = 1;
    return ESP_OK;
}

static const espui_touch_hal_t s_touch_hal = {
    .init = touch_init,
    .deinit = touch_deinit,
    .read = touch_read,
};

static esp_err_t input_init(void)
{
    return ESP_OK;
}

static esp_err_t input_deinit(void)
{
    return ESP_OK;
}

static esp_err_t input_read_event(espui_input_event_t *event, uint32_t timeout_ms)
{
    (void)event;
    (void)timeout_ms;
    /* LCD-4 V4 has no confirmed encoder/button GPIO mapping in this project. */
    return ESP_ERR_TIMEOUT;
}

static const espui_input_hal_t s_input_hal = {
    .init = input_init,
    .deinit = input_deinit,
    .read_event = input_read_event,
};

static esp_err_t power_init(void)
{
    /* Display start already initialized CH32V003. Keep this as a readiness
     * check without assuming a battery ADC conversion curve. */
    return bsp_io_expander_init() != NULL ? ESP_OK : ESP_ERR_NOT_FOUND;
}

static esp_err_t power_deinit(void)
{
    return ESP_OK;
}

static esp_err_t power_get_status(espui_power_status_t *status)
{
    if (status == NULL) {
        return ESP_ERR_INVALID_ARG;
    }
    /* CH32 ADC/battery mapping is not documented in BSP v3.0.0. */
    status->battery_percent = 255;
    status->charging = false;
    status->usb_present = false;
    return ESP_OK;
}

static esp_err_t power_set_screen_on(bool on)
{
    return on ? bsp_display_backlight_on() : bsp_display_backlight_off();
}

static const espui_power_hal_t s_power_hal = {
    .init = power_init,
    .deinit = power_deinit,
    .get_status = power_get_status,
    .set_screen_on = power_set_screen_on,
};

const espui_board_hal_t *espui_board_lcd4(void)
{
    static espui_board_hal_t board;
    board.board_id = "waveshare-esp32-s3-touch-lcd-4-v4";
    board.display = &s_display_hal;
    board.touch = &s_touch_hal;
    board.input = &s_input_hal;
    board.power = &s_power_hal;
    return &board;
}
