#include "espui_renderer.h"

#include <stdlib.h>
#include <string.h>

#include "esp_log.h"
#include "lvgl.h"

#if LV_COLOR_16_SWAP
#error "ESPUI canvas expects LV_COLOR_16_SWAP=0"
#endif

#define TAG "espui_lvgl"
#define ESPUI_RENDERER_MAX_WIDGETS 32

struct espui_ui_widget {
    lv_obj_t *obj;
    espui_widget_kind_t kind;
    bool used;
};

struct espui_ui_scene {
    lv_obj_t *root;
    espui_ui_widget_t widgets[ESPUI_RENDERER_MAX_WIDGETS];
};

static void apply_common(lv_obj_t *obj, const espui_widget_spec_t *spec)
{
    if (spec->width > 0 && spec->height > 0) {
        lv_obj_set_size(obj, spec->width, spec->height);
    }
    lv_obj_set_pos(obj, spec->x, spec->y);
    if (spec->visible) {
        lv_obj_clear_flag(obj, LV_OBJ_FLAG_HIDDEN);
    } else {
        lv_obj_add_flag(obj, LV_OBJ_FLAG_HIDDEN);
    }
}

static void apply_spec(espui_ui_widget_t *widget,
                       const espui_widget_spec_t *spec)
{
    apply_common(widget->obj, spec);

    if (widget->kind == ESPUI_WIDGET_LABEL) {
        lv_obj_set_style_text_color(widget->obj,
                                    lv_color_hex(spec->fg_color), 0);
        lv_obj_set_style_text_align(widget->obj,
                                    (lv_text_align_t)spec->align, 0);
        /*
         * Font scaling is intentionally a no-op until E3 font assets exist.
         * Themes still declare scale so tokens stay forward compatible.
         */
        (void)spec->font_scale_percent;
    } else if (widget->kind == ESPUI_WIDGET_RECT) {
        lv_obj_set_style_bg_color(widget->obj, lv_color_hex(spec->bg_color), 0);
        lv_obj_set_style_bg_opa(widget->obj, spec->transparent_bg ? 0 : 255, 0);
        lv_obj_set_style_border_width(widget->obj, 0, 0);
        lv_obj_set_style_radius(widget->obj, 0, 0);
        lv_obj_set_style_pad_all(widget->obj, 0, 0);
    } else if (widget->kind == ESPUI_WIDGET_CANVAS) {
        lv_obj_set_style_border_width(widget->obj, 0, 0);
        lv_obj_set_style_pad_all(widget->obj, 0, 0);
    }
}

static esp_err_t renderer_init(void)
{
    return ESP_OK;
}

static void renderer_deinit(void)
{
}

static esp_err_t scene_create(espui_ui_scene_t **out_scene)
{
    if (out_scene == NULL) {
        return ESP_ERR_INVALID_ARG;
    }
    *out_scene = NULL;

    if (lv_disp_get_default() == NULL) {
        ESP_LOGE(TAG, "no LVGL display registered by the board port");
        return ESP_ERR_INVALID_STATE;
    }

    espui_ui_scene_t *scene = calloc(1, sizeof(*scene));
    if (scene == NULL) {
        return ESP_ERR_NO_MEM;
    }

    scene->root = lv_obj_create(NULL);
    if (scene->root == NULL) {
        free(scene);
        return ESP_ERR_NO_MEM;
    }

    /* Default before a theme paints its own background; avoids white flash. */
    lv_obj_set_style_bg_color(scene->root, lv_color_hex(0x080C0A), 0);
    lv_obj_set_style_bg_opa(scene->root, 255, 0);
    lv_obj_clear_flag(scene->root, LV_OBJ_FLAG_SCROLLABLE);

    *out_scene = scene;
    return ESP_OK;
}

static void scene_destroy(espui_ui_scene_t *scene)
{
    if (scene == NULL) {
        return;
    }
    if (scene->root != NULL) {
        lv_obj_del(scene->root);
    }
    free(scene);
}

static esp_err_t scene_commit(espui_ui_scene_t *scene)
{
    if (scene == NULL || scene->root == NULL) {
        return ESP_ERR_INVALID_ARG;
    }
    lv_scr_load(scene->root);
    lv_obj_invalidate(scene->root);
    return ESP_OK;
}

static espui_ui_widget_t *widget_create(espui_ui_scene_t *scene,
                                        espui_widget_kind_t kind,
                                        const espui_widget_spec_t *spec)
{
    if (scene == NULL || spec == NULL) {
        return NULL;
    }

    espui_ui_widget_t *widget = NULL;
    for (size_t i = 0; i < ESPUI_RENDERER_MAX_WIDGETS; ++i) {
        if (!scene->widgets[i].used) {
            widget = &scene->widgets[i];
            break;
        }
    }
    if (widget == NULL) {
        ESP_LOGE(TAG, "widget budget exhausted");
        return NULL;
    }

    lv_obj_t *obj = NULL;
    switch (kind) {
    case ESPUI_WIDGET_LABEL:
        obj = lv_label_create(scene->root);
        if (obj != NULL) {
            lv_label_set_text(obj, "");
            lv_label_set_long_mode(obj, LV_LABEL_LONG_WRAP);
        }
        break;
    case ESPUI_WIDGET_RECT:
        obj = lv_obj_create(scene->root);
        break;
    case ESPUI_WIDGET_CANVAS:
        obj = lv_canvas_create(scene->root);
        if (obj != NULL) {
            lv_obj_clear_flag(obj, LV_OBJ_FLAG_CLICKABLE);
        }
        break;
    default:
        return NULL;
    }

    if (obj == NULL) {
        return NULL;
    }

    lv_obj_clear_flag(obj, LV_OBJ_FLAG_SCROLLABLE);
    widget->obj = obj;
    widget->kind = kind;
    widget->used = true;
    apply_spec(widget, spec);
    return widget;
}

static esp_err_t widget_set_text(espui_ui_widget_t *widget, const char *text)
{
    if (widget == NULL || text == NULL || widget->kind != ESPUI_WIDGET_LABEL) {
        return ESP_ERR_INVALID_ARG;
    }
    lv_label_set_text(widget->obj, text);
    return ESP_OK;
}

static esp_err_t widget_set_spec(espui_ui_widget_t *widget,
                                 const espui_widget_spec_t *spec)
{
    if (widget == NULL || spec == NULL) {
        return ESP_ERR_INVALID_ARG;
    }
    apply_spec(widget, spec);
    return ESP_OK;
}

static esp_err_t widget_set_visible(espui_ui_widget_t *widget, bool visible)
{
    if (widget == NULL) {
        return ESP_ERR_INVALID_ARG;
    }
    if (visible) {
        lv_obj_clear_flag(widget->obj, LV_OBJ_FLAG_HIDDEN);
    } else {
        lv_obj_add_flag(widget->obj, LV_OBJ_FLAG_HIDDEN);
    }
    return ESP_OK;
}

static esp_err_t widget_canvas_set_buffer(espui_ui_widget_t *widget,
                                          void *buffer, uint16_t width,
                                          uint16_t height)
{
    if (widget == NULL || widget->kind != ESPUI_WIDGET_CANVAS ||
        buffer == NULL || width == 0 || height == 0) {
        return ESP_ERR_INVALID_ARG;
    }
    lv_canvas_set_buffer(widget->obj, buffer, width, height,
                         LV_IMG_CF_TRUE_COLOR);
    return ESP_OK;
}

static esp_err_t widget_canvas_invalidate(espui_ui_widget_t *widget)
{
    if (widget == NULL || widget->kind != ESPUI_WIDGET_CANVAS) {
        return ESP_ERR_INVALID_ARG;
    }
    lv_obj_invalidate(widget->obj);
    return ESP_OK;
}

static esp_err_t widget_canvas_invalidate_area(espui_ui_widget_t *widget,
                                               int16_t x, int16_t y,
                                               uint16_t width,
                                               uint16_t height)
{
    if (widget == NULL || widget->kind != ESPUI_WIDGET_CANVAS ||
        width == 0 || height == 0) {
        return ESP_ERR_INVALID_ARG;
    }
    lv_area_t area = {
        .x1 = x,
        .y1 = y,
        .x2 = (lv_coord_t)(x + (int16_t)width - 1),
        .y2 = (lv_coord_t)(y + (int16_t)height - 1),
    };
    lv_obj_invalidate_area(widget->obj, &area);
    return ESP_OK;
}

static const espui_renderer_ops_t s_ops = {
    .init = renderer_init,
    .deinit = renderer_deinit,
    .scene_create = scene_create,
    .scene_destroy = scene_destroy,
    .scene_commit = scene_commit,
    .widget_create = widget_create,
    .widget_set_text = widget_set_text,
    .widget_set_spec = widget_set_spec,
    .widget_set_visible = widget_set_visible,
    .widget_canvas_set_buffer = widget_canvas_set_buffer,
    .widget_canvas_invalidate = widget_canvas_invalidate,
    .widget_canvas_invalidate_area = widget_canvas_invalidate_area,
};

static const espui_renderer_t s_renderer = {
    .id = "builtin.label",
    .ops = &s_ops,
};

const espui_renderer_t *espui_renderer_builtin_label(void)
{
    return &s_renderer;
}
