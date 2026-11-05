#include "espui_input.h"

#include "hal_stub.h"

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
    return ESP_ERR_TIMEOUT;
}

static const espui_input_hal_t s_input = {
    .init = input_init,
    .deinit = input_deinit,
    .read_event = input_read_event,
};

const espui_input_hal_t *espui_hal_input_stub(void)
{
    return &s_input;
}
