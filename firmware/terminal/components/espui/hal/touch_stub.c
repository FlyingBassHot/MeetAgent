#include "espui_touch.h"

#include "hal_stub.h"

static esp_err_t touch_init(void)
{
    return ESP_OK;
}

static esp_err_t touch_deinit(void)
{
    return ESP_OK;
}

static esp_err_t touch_read(espui_touch_point_t *points, size_t max_points,
                            size_t *count)
{
    (void)points;
    (void)max_points;
    if (count == NULL) {
        return ESP_ERR_INVALID_ARG;
    }
    *count = 0;
    return ESP_OK;
}

static const espui_touch_hal_t s_touch = {
    .init = touch_init,
    .deinit = touch_deinit,
    .read = touch_read,
};

const espui_touch_hal_t *espui_hal_touch_stub(void)
{
    return &s_touch;
}
