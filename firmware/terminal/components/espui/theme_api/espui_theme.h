/*
 * Theme API.
 *
 * A theme implements visuals and abstract intents only:
 *   - it never talks to Node/network,
 *   - it never sends raw commands (use espui_dispatch_action()),
 *   - it never calls LVGL directly (use the renderer table via context),
 *   - it must free its own private resources in destroy().
 *
 * The lifecycle matches TerminalUi.md. The context pointer is stored by the
 * theme during init() because there is exactly one active theme.
 */
#pragma once

#include <stdbool.h>
#include <stdint.h>

#include "esp_err.h"
#include "espui_manifest.h"
#include "espui_renderer.h"
#include "espui_scene.h"
#include "espui_tokens.h"
#include "espui_types.h"
#include "espui_view_model.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct espui_context espui_context_t;

typedef struct {
    esp_err_t (*init)(const espui_context_t *ctx);
    esp_err_t (*enter_scene)(espui_scene_id_t scene,
                             const espui_view_model_t *model);
    void (*update)(const espui_view_model_t *model);
    void (*on_tick)(uint32_t delta_ms);
    void (*on_input)(const espui_input_event_t *event);
    void (*on_action_result)(const espui_action_result_t *result);
    void (*destroy)(void);
} espui_theme_ops_t;

typedef struct {
    const espui_manifest_t *manifest;
    const espui_theme_ops_t *ops;
    const espui_tokens_t *tokens; /* NULL => espui_tokens_safe_default() */
} espui_theme_descriptor_t;

/* Compile-time theme registry (bounded, no dynamic allocation). */
void espui_theme_registry_reset(void);
esp_err_t espui_theme_registry_add(const espui_theme_descriptor_t *theme);
const espui_theme_descriptor_t *espui_theme_registry_find(const char *theme_id);
size_t espui_theme_registry_count(void);
const espui_theme_descriptor_t *espui_theme_registry_get(size_t index);

/* Built-in themes: safe fallback + Terminal Aesthetic (E2 mock). */
void espui_register_builtin_themes(void);
const espui_theme_descriptor_t *espui_theme_safe(void);
const espui_theme_descriptor_t *espui_theme_terminal_aesthetic(void);

#ifdef __cplusplus
}
#endif
