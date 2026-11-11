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
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "lvgl.h"
#include "phosphor_demo.h"
#include "phosphor_motion.h"
#include "phosphor_state.h"
#include "phosphor_scene.h"
#include "phosphor_types.h"

#if LV_COLOR_16_SWAP
#error "PHOSPHOR P2 expects LV_COLOR_16_SWAP=0"
#endif

#define TAG "phosphor_lvgl"

static uint16_t *s_canvas_buf;
static lv_obj_t *s_overlay;
static lv_obj_t *s_canvas;
static phosphor_motion_state_t s_motion;
static phosphor_state_resolver_t s_resolver;

#define P3_DEMO_SWITCH_MS 6000u
#define P3_IDLE_FPS 7u
#define P3_WORKING_FPS 12u

static void phosphor_anim_task(void *arg)
{
    const espui_display_hal_t *display = (const espui_display_hal_t *)arg;
    phosphor_motion_init(&s_motion, PHOSPHOR_MODE_IDLE);
    phosphor_state_resolver_init(&s_resolver);
    const int64_t start_us = esp_timer_get_time();
    uint32_t last_stats_ms = 0;
    uint32_t frames = 0;
    uint64_t render_us_total = 0;
    uint64_t loop_us_total = 0;
    uint64_t period_us_total = 0;
    uint64_t last_frame_start_us = 0;
    uint32_t last_delay_ms = 0;
    phosphor_demo_phase_t last_phase = (phosphor_demo_phase_t)-1;
    bool was_offline = false;
    uint32_t offline_start_ms = 0;

    while (true) {
        uint64_t loop_start_us = esp_timer_get_time();
        if (last_frame_start_us != 0) {
            period_us_total += loop_start_us - last_frame_start_us;
        }
        last_frame_start_us = loop_start_us;
        uint32_t now_ms = (uint32_t)((esp_timer_get_time() - start_us) / 1000);
        phosphor_demo_phase_t phase = phosphor_demo_phase_at(now_ms);
        if (phase != last_phase) {
            ESP_LOGI(TAG, "P4 demo phase=%s",
                     phosphor_demo_phase_name(phase));
            last_phase = phase;
        }
        uint32_t cycle = now_ms / PHOSPHOR_DEMO_CYCLE_MS + 1u;
        phosphor_input_state_t input;
        phosphor_visual_state_t visual;
        phosphor_demo_build_input(phase, cycle, &input);
        phosphor_state_resolver_update(&s_resolver, &input, now_ms, &visual);
        if (visual.activity == PHOSPHOR_ACTIVITY_OFFLINE) {
            if (!was_offline) {
                was_offline = true;
                offline_start_ms = now_ms;
            }
            /* 550 ms ramp, then freeze; recovery resumes from this phase. */
            if (now_ms - offline_start_ms < 550u) {
                phosphor_motion_update(&s_motion, now_ms, PHOSPHOR_MODE_IDLE);
            }
        } else {
            was_offline = false;
            phosphor_motion_mode_t motion_mode =
                visual.activity == PHOSPHOR_ACTIVITY_WORKING
                    ? PHOSPHOR_MODE_WORKING
                    : PHOSPHOR_MODE_IDLE;
            phosphor_motion_update(&s_motion, now_ms, motion_mode);
        }

        uint64_t render_start = esp_timer_get_time();
        if (display->lock(1000) == ESP_OK) {
            phosphor_scene_render_visual_rgb565(
                s_canvas_buf, PHOSPHOR_CANVAS_W, PHOSPHOR_CANVAS_H,
                &s_motion, &visual);
            lv_obj_invalidate(s_canvas);
            display->unlock();
        }
        uint64_t render_elapsed_us =
            (uint64_t)(esp_timer_get_time() - render_start);
        render_us_total += render_elapsed_us;
        ++frames;

        if (now_ms - last_stats_ms >= 5000u) {
            uint32_t window_ms = now_ms - last_stats_ms;
            uint32_t fps = window_ms > 0 ? (frames * 1000u) / window_ms : 0;
            unsigned avg_us = frames > 0
                                  ? (unsigned)(render_us_total / frames)
                                  : 0;
            unsigned loop_avg_us = frames > 0
                                       ? (unsigned)(loop_us_total / frames)
                                       : 0;
            unsigned period_avg_us = frames > 1
                                         ? (unsigned)(period_us_total /
                                                      (frames - 1))
                                         : 0;
            ESP_LOGI(TAG,
                     "P4 anim stats state=%s activity=%d attention=%u "
                     "frames=%u fps=%u render_avg_us=%u loop_avg_us=%u "
                     "period_avg_us=%u delay=%u tick=%u internal_free=%u "
                     "psram_free=%u",
                     phosphor_demo_phase_name(phase), (int)visual.activity,
                     (unsigned)visual.attention, (unsigned)frames,
                     (unsigned)fps, avg_us, loop_avg_us, period_avg_us,
                     (unsigned)last_delay_ms, (unsigned)configTICK_RATE_HZ,
                     (unsigned)heap_caps_get_free_size(MALLOC_CAP_INTERNAL),
                     (unsigned)heap_caps_get_free_size(MALLOC_CAP_SPIRAM));
            last_stats_ms = now_ms;
            frames = 0;
            render_us_total = 0;
            loop_us_total = 0;
            period_us_total = 0;
            last_frame_start_us = 0;
        }

        loop_us_total += (uint64_t)(esp_timer_get_time() - loop_start_us);
        uint32_t fps_target = visual.activity == PHOSPHOR_ACTIVITY_WORKING
                                  ? P3_WORKING_FPS
                                  : P3_IDLE_FPS;
        uint32_t frame_ms = 1000u / fps_target;
        uint32_t render_ms = (uint32_t)(render_elapsed_us / 1000u);
        uint32_t delay_ms = frame_ms > render_ms
                                ? frame_ms - render_ms
                                : 1u;
        last_delay_ms = delay_ms;
        vTaskDelay(pdMS_TO_TICKS(delay_ms));
    }
}

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
             "P4 canvas ready: %ux%u at (%d,%d), PSRAM=%u bytes",
             PHOSPHOR_CANVAS_W, PHOSPHOR_CANVAS_H, PHOSPHOR_CANVAS_X,
             PHOSPHOR_CANVAS_Y, (unsigned)bytes);

    if (xTaskCreatePinnedToCore(phosphor_anim_task, "phosphor_p3", 6144,
                                (void *)display, 4, NULL,
                                tskNO_AFFINITY) != pdPASS) {
        ESP_LOGW(TAG, "P3 animation task not started");
    } else {
        ESP_LOGI(TAG, "P3 animation started: demo 6s Idle / 6s Working");
    }
    return ESP_OK;
}
