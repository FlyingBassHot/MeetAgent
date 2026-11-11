/*
 * Renderer API.
 *
 * Themes never call LVGL directly. They create a scene root and widgets
 * through this table. The runtime owns the LVGL lock and the concrete
 * implementation (LVGL today; a headless/mock renderer for tests later).
 */
#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "esp_err.h"
#include "espui_types.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct espui_ui_scene espui_ui_scene_t;
typedef struct espui_ui_widget espui_ui_widget_t;

typedef enum {
    ESPUI_WIDGET_LABEL = 0,
    ESPUI_WIDGET_RECT,
    ESPUI_WIDGET_CANVAS,
} espui_widget_kind_t;

typedef enum {
    ESPUI_TEXT_ALIGN_LEFT = 0,
    ESPUI_TEXT_ALIGN_CENTER,
    ESPUI_TEXT_ALIGN_RIGHT,
} espui_text_align_t;

typedef struct {
    int16_t x;
    int16_t y;
    int16_t width;  /* 0 = content-sized where the renderer supports it. */
    int16_t height;
    uint32_t fg_color; /* 0xRRGGBB */
    uint32_t bg_color;
    bool transparent_bg;
    uint8_t font_scale_percent; /* 100 = renderer default */
    espui_text_align_t align;
    bool visible;
} espui_widget_spec_t;

typedef struct {
    esp_err_t (*init)(void);
    void (*deinit)(void);

    esp_err_t (*scene_create)(espui_ui_scene_t **out_scene);
    void (*scene_destroy)(espui_ui_scene_t *scene);
    esp_err_t (*scene_commit)(espui_ui_scene_t *scene);

    espui_ui_widget_t *(*widget_create)(espui_ui_scene_t *scene,
                                        espui_widget_kind_t kind,
                                        const espui_widget_spec_t *spec);
    esp_err_t (*widget_set_text)(espui_ui_widget_t *widget, const char *text);
    esp_err_t (*widget_set_spec)(espui_ui_widget_t *widget,
                                 const espui_widget_spec_t *spec);
    esp_err_t (*widget_set_visible)(espui_ui_widget_t *widget, bool visible);

    /*
     * CANVAS widgets own an external RGB565 buffer. The buffer must stay valid
     * until the widget/scene is destroyed. These ops are called on the UI task
     * (or the PHOSPHOR render task under display lock).
     */
    esp_err_t (*widget_canvas_set_buffer)(espui_ui_widget_t *widget,
                                          void *buffer, uint16_t width,
                                          uint16_t height);
    esp_err_t (*widget_canvas_invalidate)(espui_ui_widget_t *widget);
    esp_err_t (*widget_canvas_invalidate_area)(espui_ui_widget_t *widget,
                                               int16_t x, int16_t y,
                                               uint16_t width,
                                               uint16_t height);
} espui_renderer_ops_t;

typedef struct {
    const char *id;
    const espui_renderer_ops_t *ops;
} espui_renderer_t;

/* Compile-time registry (bounded, no dynamic allocation). */
void espui_renderer_registry_reset(void);
esp_err_t espui_renderer_registry_add(const espui_renderer_t *renderer);
const espui_renderer_t *espui_renderer_registry_find(const char *id);
size_t espui_renderer_registry_count(void);
const espui_renderer_t *espui_renderer_registry_get(size_t index);

/* LVGL v8 implementation shipped by the espui component. */
const espui_renderer_t *espui_renderer_builtin_label(void);

#ifdef __cplusplus
}
#endif
