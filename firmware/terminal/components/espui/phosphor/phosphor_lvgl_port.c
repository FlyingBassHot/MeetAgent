/*
 * PHOSPHOR CORE platform adapter.
 *
 * The renderer owns the LVGL canvas; this module owns one PSRAM RGB565 buffer
 * and the render task. There is no top-layer overlay: the terminal theme
 * creates a canvas widget in its Ambient scene and attaches it here. The P5
 * core/motion/dirty rasterizer is unchanged.
 */
#include "phosphor_lvgl_port.h"

#include <stddef.h>

#include "esp_heap_caps.h"
#include "esp_timer.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "freertos/task.h"

#include "glyph_dirty.h"
#include "phosphor_demo.h"
#include "phosphor_input_publisher.h"
#include "phosphor_motion.h"
#include "phosphor_scene.h"
#include "phosphor_state.h"
#include "phosphor_types.h"

#define TAG "phosphor_lvgl"
#define P3_IDLE_FPS 7u
#define P3_WORKING_FPS 12u
#define PHOSPHOR_IDLE_SLEEP_MS 100u

static const espui_display_hal_t *s_display;
static uint16_t *s_canvas_buf;
static espui_ui_widget_t *s_canvas_widget;
static const espui_renderer_ops_t *s_renderer;
static SemaphoreHandle_t s_state_lock;
static volatile bool s_active;

static phosphor_motion_state_t s_motion;
static phosphor_state_resolver_t s_resolver;
static glyph_dirty_state_t s_dirty;

static bool phosphor_canvas_ready(void)
{
    if (s_state_lock == NULL ||
        xSemaphoreTake(s_state_lock, portMAX_DELAY) != pdTRUE) {
        return false;
    }
    bool ready = s_canvas_widget != NULL && s_renderer != NULL;
    (void)xSemaphoreGive(s_state_lock);
    return ready;
}

static void phosphor_anim_task(void *arg)
{
    (void)arg;
    phosphor_motion_init(&s_motion, PHOSPHOR_MODE_IDLE);
    phosphor_state_resolver_init(&s_resolver);
    glyph_dirty_reset(&s_dirty);
    const int64_t start_us = esp_timer_get_time();
    uint32_t last_stats_ms = 0;
    uint32_t frames = 0;
    uint64_t render_us_total = 0;
    uint64_t build_us_total = 0;
    uint64_t raster_us_total = 0;
    uint64_t invalidate_us_total = 0;
    uint64_t loop_us_total = 0;
    uint64_t period_us_total = 0;
    uint64_t last_frame_start_us = 0;
    uint32_t last_delay_ms = 0;
    uint32_t last_bbox_w = 0;
    uint32_t last_bbox_h = 0;
    bool last_bbox_full = false;
    phosphor_demo_phase_t last_phase = (phosphor_demo_phase_t)-1;
    bool was_offline = false;
    uint32_t offline_start_ms = 0;
    bool last_active = true;

    while (true) {
        if (!s_active || !phosphor_canvas_ready()) {
            if (last_active) {
                ESP_LOGI(TAG, "canvas inactive; rendering paused");
                last_active = false;
            }
            vTaskDelay(pdMS_TO_TICKS(PHOSPHOR_IDLE_SLEEP_MS));
            last_frame_start_us = 0;
            continue;
        }
        if (!last_active) {
            ESP_LOGI(TAG, "canvas active; rendering resumed");
            last_active = true;
            last_phase = (phosphor_demo_phase_t)-1;
        }

        uint64_t loop_start_us = esp_timer_get_time();
        if (last_frame_start_us != 0) {
            period_us_total += loop_start_us - last_frame_start_us;
        }
        last_frame_start_us = loop_start_us;
        uint32_t now_ms = (uint32_t)((esp_timer_get_time() - start_us) / 1000);
        phosphor_demo_phase_t phase = phosphor_demo_phase_at(now_ms);
        uint32_t cycle = now_ms / PHOSPHOR_DEMO_CYCLE_MS + 1u;
        phosphor_input_state_t input;
        bool real_snapshot = false;
        phosphor_visual_state_t visual;
        bool using_demo = !phosphor_input_publisher_read(&input, &real_snapshot) ||
                          !real_snapshot;
        if (using_demo) {
            phosphor_demo_build_input(phase, cycle, &input);
            if (phase != last_phase) {
                ESP_LOGI(TAG, "P4 demo phase=%s",
                         phosphor_demo_phase_name(phase));
                last_phase = phase;
            }
        }
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
        glyph_frame_t frame;
        phosphor_scene_build_frame_visual(&frame, &s_motion, &visual);
        uint64_t build_done_us = esp_timer_get_time();
        glyph_dirty_bbox_t dirty;
        bool changed = false;
        if (s_display->lock(1000) == ESP_OK) {
            changed = glyph_dirty_render(
                &s_dirty, &frame, s_canvas_buf, PHOSPHOR_CANVAS_W,
                PHOSPHOR_CANVAS_H, phosphor_palette_default(), &dirty);
            uint64_t raster_done_us = esp_timer_get_time();
            last_bbox_w = (uint32_t)dirty.w;
            last_bbox_h = (uint32_t)dirty.h;
            last_bbox_full = dirty.full;
            if (changed && s_state_lock != NULL &&
                xSemaphoreTake(s_state_lock, portMAX_DELAY) == pdTRUE) {
                if (s_canvas_widget != NULL && s_renderer != NULL) {
                    if (dirty.full || s_renderer->widget_canvas_invalidate_area ==
                                          NULL) {
                        (void)s_renderer->widget_canvas_invalidate(
                            s_canvas_widget);
                    } else if (dirty.w > 0 && dirty.h > 0) {
                        (void)s_renderer->widget_canvas_invalidate_area(
                            s_canvas_widget, dirty.x, dirty.y,
                            (uint16_t)dirty.w, (uint16_t)dirty.h);
                    }
                }
                (void)xSemaphoreGive(s_state_lock);
            }
            invalidate_us_total +=
                (uint64_t)(esp_timer_get_time() - raster_done_us);
            s_display->unlock();
            raster_us_total += (uint64_t)(raster_done_us - build_done_us);
        } else {
            raster_us_total +=
                (uint64_t)(esp_timer_get_time() - build_done_us);
        }
        build_us_total += (uint64_t)(build_done_us - render_start);
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
                     "frames=%u fps=%u render_avg_us=%u build_avg_us=%u "
                     "raster_avg_us=%u invalidate_avg_us=%u dirty_changed=%d "
                     "dirty_cells=%d redraw_cells=%d bbox=%ux%u full=%d "
                     "stack_hwm=%u loop_avg_us=%u period_avg_us=%u delay=%u "
                     "tick=%u internal_free=%u psram_free=%u",
                     using_demo ? phosphor_demo_phase_name(phase) : "node",
                     (int)visual.activity, (unsigned)visual.attention,
                     (unsigned)frames, (unsigned)fps, avg_us,
                     frames > 0 ? (unsigned)(build_us_total / frames) : 0,
                     frames > 0 ? (unsigned)(raster_us_total / frames) : 0,
                     frames > 0 ? (unsigned)(invalidate_us_total / frames) : 0,
                     s_dirty.last_changed_cells, s_dirty.last_dirty_cells,
                     s_dirty.last_redraw_cells, (unsigned)last_bbox_w,
                     (unsigned)last_bbox_h, (int)last_bbox_full,
                     (unsigned)uxTaskGetStackHighWaterMark(NULL), loop_avg_us,
                     period_avg_us, (unsigned)last_delay_ms,
                     (unsigned)configTICK_RATE_HZ,
                     (unsigned)heap_caps_get_free_size(MALLOC_CAP_INTERNAL),
                     (unsigned)heap_caps_get_free_size(MALLOC_CAP_SPIRAM));
            last_stats_ms = now_ms;
            frames = 0;
            render_us_total = 0;
            build_us_total = 0;
            raster_us_total = 0;
            invalidate_us_total = 0;
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
        uint32_t delay_ms = frame_ms > render_ms ? frame_ms - render_ms : 1u;
        last_delay_ms = delay_ms;
        vTaskDelay(pdMS_TO_TICKS(delay_ms));
    }
}

esp_err_t phosphor_lvgl_port_init(const espui_display_hal_t *display)
{
    if (display == NULL || display->lock == NULL || display->unlock == NULL) {
        return ESP_ERR_INVALID_ARG;
    }
    if (s_canvas_buf != NULL) {
        return ESP_OK;
    }

    phosphor_input_publisher_init();
    s_display = display;
    s_state_lock = xSemaphoreCreateMutex();
    if (s_state_lock == NULL) {
        return ESP_ERR_NO_MEM;
    }

    const size_t bytes =
        (size_t)PHOSPHOR_CANVAS_W * PHOSPHOR_CANVAS_H * sizeof(uint16_t);
    s_canvas_buf = heap_caps_malloc(bytes, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
    if (s_canvas_buf == NULL) {
        vSemaphoreDelete(s_state_lock);
        s_state_lock = NULL;
        ESP_LOGE(TAG, "PSRAM canvas allocation failed (%u bytes)",
                 (unsigned)bytes);
        return ESP_ERR_NO_MEM;
    }
    phosphor_scene_render_rgb565(s_canvas_buf, PHOSPHOR_CANVAS_W,
                                 PHOSPHOR_CANVAS_H);

    if (xTaskCreatePinnedToCore(phosphor_anim_task, "phosphor_p3", 6144, NULL,
                                4, NULL, tskNO_AFFINITY) != pdPASS) {
        heap_caps_free(s_canvas_buf);
        s_canvas_buf = NULL;
        vSemaphoreDelete(s_state_lock);
        s_state_lock = NULL;
        ESP_LOGE(TAG, "PHOSPHOR render task creation failed");
        return ESP_ERR_NO_MEM;
    }
    ESP_LOGI(TAG, "canvas runtime ready: %ux%u, PSRAM=%u bytes (detached)",
             PHOSPHOR_CANVAS_W, PHOSPHOR_CANVAS_H, (unsigned)bytes);
    return ESP_OK;
}

esp_err_t phosphor_lvgl_port_attach(espui_ui_widget_t *canvas,
                                    const espui_renderer_ops_t *renderer)
{
    if (canvas == NULL || renderer == NULL ||
        renderer->widget_canvas_set_buffer == NULL ||
        renderer->widget_canvas_invalidate == NULL ||
        s_canvas_buf == NULL || s_state_lock == NULL) {
        return ESP_ERR_INVALID_ARG;
    }

    esp_err_t err = renderer->widget_canvas_set_buffer(
        canvas, s_canvas_buf, PHOSPHOR_CANVAS_W, PHOSPHOR_CANVAS_H);
    if (err != ESP_OK) {
        return err;
    }

    if (xSemaphoreTake(s_state_lock, portMAX_DELAY) != pdTRUE) {
        return ESP_ERR_INVALID_STATE;
    }
    s_canvas_widget = canvas;
    s_renderer = renderer;
    s_active = false;
    (void)xSemaphoreGive(s_state_lock);
    return ESP_OK;
}

void phosphor_lvgl_port_detach(void)
{
    s_active = false;
    if (s_state_lock == NULL ||
        xSemaphoreTake(s_state_lock, portMAX_DELAY) != pdTRUE) {
        return;
    }
    s_canvas_widget = NULL;
    s_renderer = NULL;
    (void)xSemaphoreGive(s_state_lock);
}

void phosphor_lvgl_port_set_active(bool active)
{
    s_active = active;
}
