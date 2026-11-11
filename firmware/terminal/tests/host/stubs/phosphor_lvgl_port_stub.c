/*
 * Host-test stub for the PHOSPHOR canvas runtime. Theme tests only need the
 * attach/detach lifecycle; no PSRAM or render task is created.
 */
#include "phosphor_lvgl_port.h"

static int s_attach_calls;
static int s_detach_calls;
static bool s_active;

esp_err_t phosphor_lvgl_port_init(const espui_display_hal_t *display)
{
    (void)display;
    return ESP_OK;
}

esp_err_t phosphor_lvgl_port_attach(espui_ui_widget_t *canvas,
                                    const espui_renderer_ops_t *renderer)
{
    static uint16_t s_dummy_buffer[1];
    if (canvas == NULL || renderer == NULL ||
        renderer->widget_canvas_set_buffer == NULL) {
        return ESP_ERR_INVALID_ARG;
    }
    esp_err_t err = renderer->widget_canvas_set_buffer(
        canvas, s_dummy_buffer, 1, 1);
    if (err != ESP_OK) {
        return err;
    }
    ++s_attach_calls;
    return ESP_OK;
}

void phosphor_lvgl_port_detach(void)
{
    ++s_detach_calls;
    s_active = false;
}

void phosphor_lvgl_port_set_active(bool active)
{
    s_active = active;
}

void host_phosphor_stub_reset(void)
{
    s_attach_calls = 0;
    s_detach_calls = 0;
    s_active = false;
}

int host_phosphor_stub_attach_calls(void)
{
    return s_attach_calls;
}

int host_phosphor_stub_detach_calls(void)
{
    return s_detach_calls;
}

bool host_phosphor_stub_active(void)
{
    return s_active;
}
