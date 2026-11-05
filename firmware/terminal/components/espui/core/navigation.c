#include "navigation.h"

#include "input_router.h"

espui_nav_intent_t espui_navigation_from_input(const espui_input_event_t *event,
                                               espui_scene_id_t scene)
{
    (void)scene;
    if (event == NULL) {
        return ESPUI_NAV_NONE;
    }

    switch (event->type) {
    case ESPUI_INPUT_ENCODER_CW:
        return ESPUI_NAV_NEXT;
    case ESPUI_INPUT_ENCODER_CCW:
        return ESPUI_NAV_PREV;
    case ESPUI_INPUT_ENCODER_PRESS:
    case ESPUI_INPUT_SELECT:
        return ESPUI_NAV_ACTIVATE;
    case ESPUI_INPUT_BACK:
        return ESPUI_NAV_BACK;
    case ESPUI_INPUT_LONG_PRESS:
        return ESPUI_NAV_TOGGLE_ATTENTION;
    case ESPUI_INPUT_TOUCH:
        return event->pressed ? ESPUI_NAV_ACTIVATE : ESPUI_NAV_NONE;
    case ESPUI_INPUT_NONE:
    default:
        return ESPUI_NAV_NONE;
    }
}

esp_err_t espui_navigation_apply(espui_nav_intent_t intent,
                                 espui_scene_manager_t *scenes)
{
    if (scenes == NULL) {
        return ESP_ERR_INVALID_ARG;
    }

    switch (intent) {
    case ESPUI_NAV_NONE:
    case ESPUI_NAV_NEXT:
    case ESPUI_NAV_PREV:
        return ESP_OK;

    case ESPUI_NAV_ACTIVATE:
        if (espui_scene_manager_current(scenes) == ESPUI_SCENE_AMBIENT) {
            return espui_scene_manager_request(scenes, ESPUI_SCENE_FOCUS,
                                               ESPUI_SCENE_SOURCE_USER);
        }
        /* Focus/Attention actions are explicit and theme/core driven. */
        return ESP_OK;

    case ESPUI_NAV_BACK:
        return espui_scene_manager_back(scenes);

    case ESPUI_NAV_SCENE_AMBIENT:
        return espui_scene_manager_request(scenes, ESPUI_SCENE_AMBIENT,
                                           ESPUI_SCENE_SOURCE_USER);
    case ESPUI_NAV_SCENE_FOCUS:
        return espui_scene_manager_request(scenes, ESPUI_SCENE_FOCUS,
                                           ESPUI_SCENE_SOURCE_USER);
    case ESPUI_NAV_SCENE_ACTIVITY:
        return espui_scene_manager_request(scenes, ESPUI_SCENE_ACTIVITY,
                                           ESPUI_SCENE_SOURCE_USER);
    case ESPUI_NAV_SCENE_SETTINGS:
        return espui_scene_manager_request(scenes, ESPUI_SCENE_SETTINGS,
                                           ESPUI_SCENE_SOURCE_USER);
    case ESPUI_NAV_SCENE_ATTENTION:
        return espui_scene_manager_request(scenes, ESPUI_SCENE_ATTENTION,
                                           ESPUI_SCENE_SOURCE_USER);

    case ESPUI_NAV_TOGGLE_ATTENTION:
        if (scenes->attention_count == 0 ||
            espui_scene_manager_in_attention(scenes)) {
            return ESP_OK;
        }
        return espui_scene_manager_request(scenes, ESPUI_SCENE_ATTENTION,
                                           ESPUI_SCENE_SOURCE_ATTENTION);

    default:
        return ESP_ERR_INVALID_ARG;
    }
}
