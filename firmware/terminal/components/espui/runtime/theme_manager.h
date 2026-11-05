/*
 * Theme lifecycle manager.
 *
 * Owns exactly one active theme. Switching never touches the ViewModel; it
 * only releases the current renderer objects, initializes the next theme and
 * re-applies the current scene + ViewModel. Any failure falls back to the
 * built-in safe theme.
 */
#pragma once

#include <stdbool.h>
#include <stdint.h>

#include "esp_err.h"
#include "espui_display.h"
#include "espui_error.h"
#include "espui_manifest.h"
#include "espui_scene.h"
#include "espui_theme.h"
#include "espui_types.h"
#include "espui_view_model.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    ESPUI_THEME_SWITCH_OK = 0,
    ESPUI_THEME_SWITCH_SAME_THEME,
    ESPUI_THEME_SWITCH_FALLBACK_USED,
    ESPUI_THEME_SWITCH_NO_FALLBACK,
    ESPUI_THEME_SWITCH_NOT_READY,
} espui_theme_switch_status_t;

typedef enum {
    ESPUI_THEME_REASON_NONE = 0,
    ESPUI_THEME_REASON_UNKNOWN_THEME,
    ESPUI_THEME_REASON_INVALID_DESCRIPTOR,
    ESPUI_THEME_REASON_MANIFEST,
    ESPUI_THEME_REASON_RENDERER_MISSING,
    ESPUI_THEME_REASON_DISPLAY_UNSUPPORTED,
    ESPUI_THEME_REASON_INIT_FAILED,
    ESPUI_THEME_REASON_FALLBACK_FAILED,
} espui_theme_switch_reason_t;

typedef struct {
    espui_theme_switch_status_t status;
    espui_theme_switch_reason_t reason;
    espui_manifest_status_t manifest_status;
    esp_err_t error;
    char requested_id[ESPUI_ID_MAX];
    char active_id[ESPUI_ID_MAX];
    const char *detail; /* Static string; never freed. */
} espui_theme_switch_report_t;

typedef struct {
    const espui_context_t *ctx;
    const espui_theme_descriptor_t *fallback;
    const espui_theme_descriptor_t *active;
    const espui_renderer_t *renderer;
    const espui_display_info_t *display;
    espui_theme_switch_report_t last_report;
    bool initialized;
    uint32_t switches;
} espui_theme_manager_t;

void espui_theme_manager_init(espui_theme_manager_t *manager,
                              const espui_context_t *ctx,
                              const espui_theme_descriptor_t *fallback,
                              const espui_display_info_t *display);

/*
 * Applies a registered theme by id. On failure the safe theme is activated
 * and the return code still describes the failure (never ESP_OK).
 */
esp_err_t espui_theme_manager_apply(espui_theme_manager_t *manager,
                                    const char *theme_id,
                                    espui_scene_id_t scene,
                                    const espui_view_model_t *model);

const espui_theme_descriptor_t *espui_theme_manager_active(
    const espui_theme_manager_t *manager);
const espui_renderer_t *espui_theme_manager_renderer(
    const espui_theme_manager_t *manager);
const espui_tokens_t *espui_theme_manager_tokens(
    const espui_theme_manager_t *manager);
const espui_theme_switch_report_t *espui_theme_manager_last_report(
    const espui_theme_manager_t *manager);
const char *espui_theme_switch_status_name(espui_theme_switch_status_t status);
const char *espui_theme_switch_reason_name(espui_theme_switch_reason_t reason);

#ifdef __cplusplus
}
#endif
