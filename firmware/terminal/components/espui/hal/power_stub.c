#include "espui_power.h"

#include "hal_stub.h"

static esp_err_t power_init(void)
{
    return ESP_OK;
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
    status->battery_percent = 100;
    status->charging = false;
    status->usb_present = true;
    return ESP_OK;
}

static esp_err_t power_set_screen_on(bool on)
{
    (void)on;
    return ESP_OK;
}

static const espui_power_hal_t s_power = {
    .init = power_init,
    .deinit = power_deinit,
    .get_status = power_get_status,
    .set_screen_on = power_set_screen_on,
};

const espui_power_hal_t *espui_hal_power_stub(void)
{
    return &s_power;
}
