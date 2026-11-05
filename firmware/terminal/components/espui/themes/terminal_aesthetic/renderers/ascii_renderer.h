/*
 * Terminal Aesthetic — pure-logic ASCII renderer.
 *
 * E3a scope:
 *   - fixed character grid + glyph ramp density field
 *   - ASCII geometry: core ring, orbit, wireframe, scanline, particles
 *   - six visual states: idle / working / completed / approval / error / offline
 *   - per-state motion (speed / density / amplitude), completed pulse
 *   - bindings for the existing Animation Scheduler and Scene Manager
 *
 * No LVGL, no ESP-IDF dependency, no per-frame allocation. The LVGL
 * character-grid widget is E3b; this module only produces frames.
 */
#pragma once

#include <stdbool.h>
#include <stdint.h>

#include "animation_scheduler.h"
#include "esp_err.h"
#include "espui_motion.h"
#include "espui_scene.h"
#include "espui_view_model.h"

#ifdef __cplusplus
extern "C" {
#endif

#define ESPUI_ASCII_MAX_COLS 80
#define ESPUI_ASCII_MAX_ROWS 40
#define ESPUI_ASCII_GLYPH_MAX 16
#define ESPUI_ASCII_DEFAULT_COLS 60
#define ESPUI_ASCII_DEFAULT_ROWS 22
#define ESPUI_ASCII_DEFAULT_RAMP " .:-=+*#%@"
#define ESPUI_ASCII_DEFAULT_FPS 15
#define ESPUI_ASCII_DEFAULT_PULSE_MS 1200

typedef enum {
    ESPUI_VISUAL_IDLE = 0,
    ESPUI_VISUAL_WORKING,
    ESPUI_VISUAL_COMPLETED,
    ESPUI_VISUAL_APPROVAL,
    ESPUI_VISUAL_ERROR,
    ESPUI_VISUAL_OFFLINE,
    ESPUI_VISUAL_STATE_COUNT,
} espui_visual_state_t;

typedef enum {
    ESPUI_ASCII_COLOR_NONE = 0,
    ESPUI_ASCII_COLOR_PRIMARY,
    ESPUI_ASCII_COLOR_SECONDARY,
    ESPUI_ASCII_COLOR_MUTED,
    ESPUI_ASCII_COLOR_WARNING,
    ESPUI_ASCII_COLOR_ERROR,
    ESPUI_ASCII_COLOR_HIGHLIGHT,
} espui_ascii_color_t;

typedef struct {
    uint16_t speed_percent;
    uint8_t density_percent;
    uint8_t amplitude_percent;
} espui_ascii_motion_t;

typedef struct {
    espui_visual_state_t state;
    espui_node_status_t node_status;
    bool connected;
    uint16_t approval_count;
    uint16_t error_count;
    uint16_t completion_count;
    uint16_t active_sessions;
    uint16_t agent_count;
} espui_ascii_status_t;

typedef struct {
    uint8_t cols;
    uint8_t rows;
    char cells[ESPUI_ASCII_MAX_ROWS][ESPUI_ASCII_MAX_COLS + 1];
    uint8_t intensity[ESPUI_ASCII_MAX_ROWS][ESPUI_ASCII_MAX_COLS];
    uint8_t color[ESPUI_ASCII_MAX_ROWS][ESPUI_ASCII_MAX_COLS];
} espui_ascii_frame_t;

typedef struct {
    uint8_t cols;              /* 0 => 60 */
    uint8_t rows;              /* 0 => 22 */
    const char *glyph_ramp;    /* NULL => " .:-=+*#%@" */
    uint8_t target_fps;        /* 0 => profile fps, then 15 */
    uint16_t completed_pulse_ms; /* 0 => transition+800, then 1200 */
    const espui_motion_profile_t *motion_profile; /* NULL => default */
    uint8_t density_percent;      /* 0 => 100 */
    uint16_t animation_speed_percent; /* 0 => 100 */
} espui_ascii_config_t;

typedef struct {
    bool initialized;
    uint8_t cols;
    uint8_t rows;
    uint8_t glyph_count;
    char glyphs[ESPUI_ASCII_GLYPH_MAX];

    uint8_t target_fps;
    uint16_t completed_pulse_ms;
    uint8_t density_percent;
    uint16_t user_speed_percent;
    const espui_motion_profile_t *profile;
    espui_ascii_motion_t motion[ESPUI_VISUAL_STATE_COUNT];

    espui_ascii_status_t status;
    espui_ascii_frame_t frame;

    uint32_t clock_ms;
    uint32_t state_ms;
    uint32_t frame_count;
    uint16_t phase;

    espui_animation_scheduler_t *scheduler;
    int32_t scheduler_slot;
} espui_ascii_renderer_t;

/* NULL config => defaults. Invalid dimensions/ramp => ESP_ERR_INVALID_ARG. */
esp_err_t espui_ascii_renderer_init(espui_ascii_renderer_t *renderer,
                                    const espui_ascii_config_t *config);
void espui_ascii_renderer_deinit(espui_ascii_renderer_t *renderer);

/* Recomputes per-state motion from the profile + density/user speed. */
esp_err_t espui_ascii_renderer_set_motion_profile(
    espui_ascii_renderer_t *renderer, const espui_motion_profile_t *profile);
esp_err_t espui_ascii_renderer_set_density(espui_ascii_renderer_t *renderer,
                                           uint8_t density_percent);
esp_err_t espui_ascii_renderer_set_user_speed(
    espui_ascii_renderer_t *renderer, uint16_t speed_percent);

/* Resolves business state -> six visual states. scene is reserved for layout. */
espui_visual_state_t espui_ascii_resolve_visual_state(
    const espui_view_model_t *model, espui_scene_id_t scene);
espui_ascii_status_t espui_ascii_status_from_view_model(
    const espui_view_model_t *model, espui_scene_id_t scene);

esp_err_t espui_ascii_renderer_set_status(espui_ascii_renderer_t *renderer,
                                          const espui_ascii_status_t *status);
esp_err_t espui_ascii_renderer_apply_view_model(
    espui_ascii_renderer_t *renderer, const espui_view_model_t *model,
    espui_scene_id_t scene);

/* Advances animation time without rendering. */
void espui_ascii_renderer_advance(espui_ascii_renderer_t *renderer,
                                  uint32_t delta_ms);
/* Renders the current frame. */
esp_err_t espui_ascii_renderer_render(espui_ascii_renderer_t *renderer);
/* advance + render; matches espui_motion_tick_fn. */
void espui_ascii_renderer_tick(uint32_t delta_ms, void *user_ctx);

/* Adds a periodic scheduler slot (1000/fps). Returns slot or -1. */
int32_t espui_ascii_renderer_bind_scheduler(
    espui_ascii_renderer_t *renderer, espui_animation_scheduler_t *scheduler);
bool espui_ascii_renderer_unbind_scheduler(
    espui_ascii_renderer_t *renderer, espui_animation_scheduler_t *scheduler);

const espui_ascii_frame_t *espui_ascii_renderer_frame(
    const espui_ascii_renderer_t *renderer);
const espui_ascii_status_t *espui_ascii_renderer_status(
    const espui_ascii_renderer_t *renderer);
const espui_ascii_motion_t *espui_ascii_renderer_motion(
    const espui_ascii_renderer_t *renderer, espui_visual_state_t state);
char espui_ascii_glyph_for_intensity(const espui_ascii_renderer_t *renderer,
                                     uint8_t percent);
espui_ascii_color_t espui_ascii_state_color(espui_visual_state_t state);
const char *espui_visual_state_name(espui_visual_state_t state);
const char *espui_visual_state_label(espui_visual_state_t state);

#ifdef __cplusplus
}
#endif
