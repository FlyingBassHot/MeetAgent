/*
 * Scene identifiers and scene manager contract.
 *
 * A scene is logical. A theme decides how each scene looks; the UI core owns
 * which scene is active and safety-related override rules.
 */
#pragma once

#include <stdbool.h>
#include <stdint.h>

#include "esp_err.h"
#include "espui_types.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    ESPUI_SCENE_AMBIENT = 0,
    ESPUI_SCENE_FOCUS,
    ESPUI_SCENE_ACTIVITY,
    ESPUI_SCENE_ATTENTION,
    ESPUI_SCENE_SETTINGS,
    ESPUI_SCENE_COUNT,
    ESPUI_SCENE_INVALID = 0xFF,
} espui_scene_id_t;

#define ESPUI_SCENE_MASK(scene) (1u << (unsigned)(scene))
#define ESPUI_SCENE_MASK_ALL                                                    \
    ((1u << ESPUI_SCENE_COUNT) - 1u)

typedef enum {
    ESPUI_SCENE_SOURCE_USER = 0,
    ESPUI_SCENE_SOURCE_ATTENTION,
    ESPUI_SCENE_SOURCE_SYSTEM,
} espui_scene_source_t;

typedef void (*espui_scene_change_cb_t)(espui_scene_id_t scene, void *user_ctx);

typedef struct {
    espui_scene_id_t current;
    espui_scene_id_t previous;
    espui_scene_id_t resume_scene;
    espui_scene_id_t stack[ESPUI_MAX_SCENE_STACK];
    uint8_t depth;
    uint16_t attention_count;
    espui_scene_change_cb_t listener;
    void *listener_ctx;
} espui_scene_manager_t;

const char *espui_scene_name(espui_scene_id_t scene);
bool espui_scene_is_valid(espui_scene_id_t scene);

void espui_scene_manager_init(espui_scene_manager_t *manager,
                              espui_scene_change_cb_t listener,
                              void *listener_ctx);

/* User/system navigation. Safety attention is sticky until its count is 0. */
esp_err_t espui_scene_manager_request(espui_scene_manager_t *manager,
                                      espui_scene_id_t scene,
                                      espui_scene_source_t source);

/* Used by the input router (Back). */
esp_err_t espui_scene_manager_back(espui_scene_manager_t *manager);

/* Updates the pending-attention count and enters/leaves Attention safely. */
esp_err_t espui_scene_manager_set_attention_count(
    espui_scene_manager_t *manager, uint16_t attention_count);

/* Bypasses sticky attention, for system-owned scenes such as Settings. */
esp_err_t espui_scene_manager_force(espui_scene_manager_t *manager,
                                    espui_scene_id_t scene,
                                    espui_scene_source_t source);

espui_scene_id_t espui_scene_manager_current(
    const espui_scene_manager_t *manager);
bool espui_scene_manager_in_attention(const espui_scene_manager_t *manager);

#ifdef __cplusplus
}
#endif
