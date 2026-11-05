/*
 * Navigation mapping and application.
 *
 * Themes express focus/selection visually. The UI core owns scene changes so
 * safety rules cannot be bypassed by a theme.
 */
#pragma once

#include "esp_err.h"
#include "espui_scene.h"
#include "espui_types.h"

#ifdef __cplusplus
extern "C" {
#endif

espui_nav_intent_t espui_navigation_from_input(const espui_input_event_t *event,
                                               espui_scene_id_t scene);

/*
 * Applies scene-level intents. NEXT/PREV/ACTIVATE are left to the theme/core
 * focus handler; ACTIVATE from Ambient opens Focus.
 */
esp_err_t espui_navigation_apply(espui_nav_intent_t intent,
                                 espui_scene_manager_t *scenes);

#ifdef __cplusplus
}
#endif
