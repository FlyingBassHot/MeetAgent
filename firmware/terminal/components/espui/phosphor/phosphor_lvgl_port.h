#pragma once

#include <stdbool.h>

#include "esp_err.h"
#include "espui_display.h"
#include "espui_renderer.h"

#ifdef __cplusplus
extern "C" {
#endif

/*
 * PHOSPHOR canvas runtime.
 *
 * init() allocates one PSRAM frame buffer and starts the render task. The
 * terminal theme creates an ESPUI_WIDGET_CANVAS and attaches it; there is no
 * top-layer overlay. attach/detach are called from the UI task under the
 * display lock. set_active(false) pauses rendering for non-Ambient scenes.
 */
esp_err_t phosphor_lvgl_port_init(const espui_display_hal_t *display);
esp_err_t phosphor_lvgl_port_attach(espui_ui_widget_t *canvas,
                                    const espui_renderer_ops_t *renderer);
void phosphor_lvgl_port_detach(void);
void phosphor_lvgl_port_set_active(bool active);

#ifdef __cplusplus
}
#endif
