/*
 * Internal UI event queue payloads. Only espui.c consumes these.
 */
#pragma once

#include "espui_scene.h"
#include "espui_types.h"
#include "espui_view_model.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    ESPUI_EVENT_NONE = 0,
    ESPUI_EVENT_VIEW_MODEL,
    ESPUI_EVENT_INPUT,
    ESPUI_EVENT_ACTION_RESULT,
    ESPUI_EVENT_SCENE,
    ESPUI_EVENT_THEME,
    ESPUI_EVENT_STOP,
} espui_event_type_t;

typedef struct {
    espui_event_type_t type;
    union {
        espui_view_model_t view_model;
        espui_input_event_t input;
        espui_action_result_t action_result;
        espui_scene_id_t scene;
        char theme_id[ESPUI_ID_MAX];
    } data;
} espui_event_t;

#ifdef __cplusplus
}
#endif
