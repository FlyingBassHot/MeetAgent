/*
 * ESPUI runtime public API.
 *
 * Themes receive a const espui_context_t and may only use the accessors below.
 * Network/state tasks must post events; they must never call LVGL or theme ops
 * directly.
 */
#pragma once

#include <stdbool.h>
#include <stdint.h>

#include "action_dispatcher.h"
#include "esp_err.h"
#include "espui_board.h"
#include "espui_display.h"
#include "espui_motion.h"
#include "espui_renderer.h"
#include "espui_scene.h"
#include "espui_theme.h"
#include "espui_tokens.h"
#include "espui_types.h"
#include "espui_view_model.h"
#include "theme_manager.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct espui_context espui_context_t;

typedef struct {
    const espui_board_hal_t *board;
    const espui_view_model_t *initial_model; /* Optional; copied. */
    const char *initial_theme_id;            /* NULL => safe fallback. */

    espui_action_send_fn action_send; /* Optional until E4 networking. */
    void *action_send_ctx;

    uint32_t ui_task_stack;    /* 0 => 8192 */
    uint32_t ui_task_priority; /* 0 => 5 */
    int ui_task_core;          /* -1 => no affinity */
    uint32_t tick_ms;          /* 0 => 50 */
    uint32_t event_queue_length; /* 0 => 4 (ViewModel/action/theme) */
    uint32_t input_queue_length; /* 0 => 16 (raw/gesture input) */
    uint32_t input_poll_ms;      /* 0 => 20 */
    uint32_t input_task_stack;   /* 0 => 4096 */
    uint32_t input_task_priority; /* 0 => 4 */
    uint32_t memory_budget_bytes; /* 0 => 64 KiB */
} espui_config_t;

/* Initializes registries, ViewModel, renderer, theme manager and queues. */
esp_err_t espui_init(const espui_config_t *config);

/* Creates the UI task after espui_init(). */
esp_err_t espui_start(void);

/* Stops the UI task and releases the active theme. Best effort. */
esp_err_t espui_stop(void);

bool espui_initialized(void);
bool espui_started(void);
const espui_context_t *espui_context(void);

/* Thread-safe posts from network/state/input tasks. They copy into the queue. */
esp_err_t espui_post_view_model(const espui_view_model_t *model);
esp_err_t espui_post_input(const espui_input_event_t *event);
esp_err_t espui_post_action_result(const espui_action_result_t *result);
esp_err_t espui_post_scene(espui_scene_id_t scene);
esp_err_t espui_post_theme(const char *theme_id);

/* Theme-facing accessors. All are safe on the UI task. */
const espui_view_model_t *espui_view_model(const espui_context_t *ctx);
const espui_renderer_ops_t *espui_renderer(const espui_context_t *ctx);
const espui_tokens_t *espui_tokens(const espui_context_t *ctx);
const espui_motion_profile_t *espui_motion_profile(const espui_context_t *ctx);
const espui_display_info_t *espui_display(const espui_context_t *ctx);
espui_scene_id_t espui_current_scene(const espui_context_t *ctx);
esp_err_t espui_request_scene(const espui_context_t *ctx,
                              espui_scene_id_t scene);
esp_err_t espui_dispatch_action(const espui_context_t *ctx,
                                const espui_action_t *action);
bool espui_motion_enabled(const espui_context_t *ctx);
uint32_t espui_uptime_ms(const espui_context_t *ctx);

#ifdef __cplusplus
}
#endif
