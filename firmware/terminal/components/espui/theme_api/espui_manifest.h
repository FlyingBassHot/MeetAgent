/*
 * Theme manifest.
 *
 * E2 uses compile-time manifests (C constants plus mirrored JSON assets for
 * documentation). Data-driven manifest loading is an E3+ extension.
 */
#pragma once

#include <stdbool.h>
#include <stdint.h>

#include "esp_err.h"
#include "espui_display.h"
#include "espui_scene.h"
#include "espui_types.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    ESPUI_MANIFEST_OK = 0,
    ESPUI_MANIFEST_NULL_ARG,
    ESPUI_MANIFEST_BAD_ID,
    ESPUI_MANIFEST_BAD_API,
    ESPUI_MANIFEST_BAD_RENDERER,
    ESPUI_MANIFEST_BAD_DISPLAY,
    ESPUI_MANIFEST_NO_SCENES,
} espui_manifest_status_t;

/* Manifest "configurable" bits. */
typedef enum {
    ESPUI_CONFIG_ACCENT_COLOR = 1u << 0,
    ESPUI_CONFIG_BRIGHTNESS = 1u << 1,
    ESPUI_CONFIG_ANIMATION_SPEED = 1u << 2,
    ESPUI_CONFIG_MOTION_ENABLED = 1u << 3,
    ESPUI_CONFIG_VISUAL_DENSITY = 1u << 4,
    ESPUI_CONFIG_IDLE_TIMEOUT = 1u << 5,
    ESPUI_CONFIG_NIGHT_MODE = 1u << 6,
} espui_config_flag_t;

typedef struct {
    espui_orientation_t orientation;
    uint16_t min_width;
    uint16_t min_height;
} espui_display_requirement_t;

typedef struct {
    const char *theme_id;
    const char *name;
    const char *version;      /* "MAJOR.MINOR.PATCH" */
    uint16_t api_version;     /* ESPUI_THEME_API_VERSION */
    const char *renderer_id;  /* Must exist in the renderer registry. */
    espui_display_requirement_t display;
    uint32_t scene_mask;      /* ESPUI_SCENE_MASK(...) */
    uint32_t configurable_mask;
    const char *motion_profile_id;
} espui_manifest_t;

/* Shape checks that do not need a physical display. */
espui_manifest_status_t espui_manifest_validate_basic(
    const espui_manifest_t *manifest);

/* Shape + display compatibility checks. */
espui_manifest_status_t espui_manifest_validate(
    const espui_manifest_t *manifest, const espui_display_info_t *display);

const char *espui_manifest_status_name(espui_manifest_status_t status);
bool espui_manifest_supports_scene(const espui_manifest_t *manifest,
                                   espui_scene_id_t scene);

#ifdef __cplusplus
}
#endif
