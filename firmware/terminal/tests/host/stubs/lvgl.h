/*
 * Host-test stub for the subset of LVGL v8 used by the runtime renderer.
 * Declarations only: the syntax-check target compiles without linking.
 */
#pragma once

#include <stdbool.h>
#include <stdint.h>

typedef struct _lv_obj_t lv_obj_t;
typedef struct _lv_disp_t lv_disp_t;

typedef struct {
    uint8_t blue;
    uint8_t green;
    uint8_t red;
} lv_color_t;

typedef enum {
    LV_LABEL_LONG_WRAP = 0,
    LV_LABEL_LONG_DOT,
    LV_LABEL_LONG_SCROLL,
    LV_LABEL_LONG_SCROLL_CIRCULAR,
    LV_LABEL_LONG_CLIP,
} lv_label_long_mode_t;

typedef enum {
    LV_TEXT_ALIGN_LEFT = 0,
    LV_TEXT_ALIGN_CENTER,
    LV_TEXT_ALIGN_RIGHT,
} lv_text_align_t;

#define LV_OBJ_FLAG_HIDDEN (1u << 0)
#define LV_OBJ_FLAG_SCROLLABLE (1u << 1)

lv_disp_t *lv_disp_get_default(void);

lv_obj_t *lv_obj_create(lv_obj_t *parent);
void lv_obj_del(lv_obj_t *obj);
void lv_obj_set_pos(lv_obj_t *obj, int32_t x, int32_t y);
void lv_obj_set_size(lv_obj_t *obj, int32_t w, int32_t h);
void lv_obj_add_flag(lv_obj_t *obj, uint32_t flag);
void lv_obj_clear_flag(lv_obj_t *obj, uint32_t flag);
void lv_obj_add_event_cb(lv_obj_t *obj, void (*cb)(void *), uint32_t filter,
                         void *user_data);
void lv_obj_set_style_text_color(lv_obj_t *obj, lv_color_t color, uint32_t part);
void lv_obj_set_style_text_align(lv_obj_t *obj, lv_text_align_t align,
                                 uint32_t part);
void lv_obj_set_style_bg_color(lv_obj_t *obj, lv_color_t color, uint32_t part);
void lv_obj_set_style_bg_opa(lv_obj_t *obj, uint8_t opa, uint32_t part);
void lv_obj_set_style_border_width(lv_obj_t *obj, int32_t width, uint32_t part);
void lv_obj_set_style_radius(lv_obj_t *obj, int32_t radius, uint32_t part);
void lv_obj_set_style_pad_all(lv_obj_t *obj, int32_t value, uint32_t part);

lv_obj_t *lv_label_create(lv_obj_t *parent);
void lv_label_set_text(lv_obj_t *label, const char *text);
void lv_label_set_long_mode(lv_obj_t *label, lv_label_long_mode_t mode);

lv_color_t lv_color_hex(uint32_t value);

void lv_scr_load(lv_obj_t *screen);
lv_obj_t *lv_scr_act(void);
void lv_obj_invalidate(lv_obj_t *obj);
