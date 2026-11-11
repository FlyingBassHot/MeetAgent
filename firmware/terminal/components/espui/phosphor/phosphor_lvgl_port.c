/*
 * PHOSPHOR CORE P1 LVGL platform adapter.
 *
 * This is the only PHOSPHOR file that includes LVGL. It owns one PSRAM
 * canvas buffer and one top-layer overlay; it does not create labels or
 * per-frame objects. P1 is deliberately static (no timer/animation).
 */
#include "phosphor_lvgl_port.h"

#include <stddef.h>

#include "esp_heap_caps.h"
#include "esp_log.h"
#include "lvgl.h"
#include "phosphor_scene.h"
#include "phosphor_types.h"

#define TAG "phosphor_lvgl"

static uint16_t *s_canvas_buf;
static lv_obj_t *s_overlay;
static lv_obj_t *s_canvas;

esp_err_t phosphor_lvgl_port_init(const espui_display_hal_t *display)
{
    if (display == NULL || display->lock == NULL || display->unlock == NULL) {
        return ESP_ERR_INVALID_ARG;
    }
    if (s_canvas != NULL) {
        return ESP_OK;
    }
    if (lv_disp_get_default() == NULL) {
        ESP_LOGE(TAG, "no LVGL display registered");
        return ESP_ERR_INVALID_STATE;
    }

    const size_t bytes =
        (size_t)PHOSPHOR_CANVAS_W * PHOSPHOR_CANVAS_H * sizeof(uint16_t);
    s_canvas_buf = heap_caps_malloc(bytes, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
    if (s_canvas_buf == NULL) {
        ESP_LOGE(TAG, "PSRAM canvas allocation failed (%u bytes)",
                 (unsigned)bytes);
        return ESP_ERR_NO_MEM;
    }
    phosphor_scene_render_rgb565(s_canvas_buf, PHOSPHOR_CANVAS_W,
                                 PHOSPHOR_CANVAS_H);

    esp_err_t lock_err = display->lock(1000);
    if (lock_err != ESP_OK) {
        ESP_LOGE(TAG, "display lock failed: %s", esp_err_to_name(lock_err));
        heap_caps_free(s_canvas_buf);
        s_canvas_buf = NULL;
        return lock_err;
    }

    s_overlay = lv_obj_create(lv_layer_top());
    if (s_overlay == NULL) {
        display->unlock();
        heap_caps_free(s_canvas_buf);
        s_canvas_buf = NULL;
        return ESP_ERR_NO_MEM;
    }
    lv_obj_set_pos(s_overlay, 0, 0);
    lv_obj_set_size(s_overlay, 480, 480);
    lv_obj_set_style_bg_color(s_overlay, lv_color_hex(0x080C0A), 0);
    lv_obj_set_style_bg_opa(s_overlay, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(s_overlay, 0, 0);
    lv_obj_set_style_radius(s_overlay, 0, 0);
    lv_obj_set_style_pad_all(s_overlay, 0, 0);
    lv_obj_clear_flag(s_overlay, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_clear_flag(s_overlay, LV_OBJ_FLAG_CLICKABLE);

    s_canvas = lv_canvas_create(s_overlay);
    if (s_canvas == NULL) {
        lv_obj_del(s_overlay);
        s_overlay = NULL;
        display->unlock();
        heap_caps_free(s_canvas_buf);
        s_canvas_buf = NULL;
        return ESP_ERR_NO_MEM;
    }
    lv_canvas_set_buffer(s_canvas, s_canvas_buf, PHOSPHOR_CANVAS_W,
                         PHOSPHOR_CANVAS_H, LV_IMG_CF_TRUE_COLOR);
    lv_obj_set_pos(s_canvas, PHOSPHOR_CANVAS_X, PHOSPHOR_CANVAS_Y);
    lv_obj_clear_flag(s_canvas, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_invalidate(s_overlay);
    display->unlock();

    ESP_LOGI(TAG,
             "P1 static canvas ready: %ux%u at (%d,%d), PSRAM=%u bytes",
             PHOSPHOR_CANVAS_W, PHOSPHOR_CANVAS_H, PHOSPHOR_CANVAS_X,
             PHOSPHOR_CANVAS_Y, (unsigned)bytes);
    return ESP_OK;
}
