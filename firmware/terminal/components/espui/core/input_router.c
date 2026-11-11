#include "input_router.h"

#include <string.h>

#include "navigation.h"

void espui_input_router_init(espui_input_router_t *router)
{
    if (router == NULL) {
        return;
    }
    memset(router, 0, sizeof(*router));
    router->scene = ESPUI_SCENE_AMBIENT;
}

void espui_input_router_set_scene(espui_input_router_t *router,
                                  espui_scene_id_t scene)
{
    if (router != NULL && espui_scene_is_valid(scene)) {
        router->scene = scene;
    }
}

void espui_input_router_set_attention_count(espui_input_router_t *router,
                                            uint16_t attention_count)
{
    if (router != NULL) {
        router->attention_count = attention_count;
    }
}

uint16_t espui_input_router_focus_index(
    const espui_input_router_t *router)
{
    return router != NULL ? router->focus_index : 0;
}

espui_nav_intent_t espui_input_router_feed(espui_input_router_t *router,
                                           const espui_input_event_t *event)
{
    if (router == NULL || event == NULL) {
        return ESPUI_NAV_NONE;
    }

    espui_nav_intent_t intent =
        espui_navigation_from_input(event, router->scene);

    switch (intent) {
    case ESPUI_NAV_NEXT:
        if (router->focus_index < UINT16_MAX) {
            ++router->focus_index;
        }
        break;
    case ESPUI_NAV_PREV:
        if (router->focus_index > 0) {
            --router->focus_index;
        }
        break;
    case ESPUI_NAV_TOGGLE_ATTENTION:
        if (router->attention_count > 0) {
            router->scene = ESPUI_SCENE_ATTENTION;
            return ESPUI_NAV_SCENE_ATTENTION;
        }
        /* Nothing is waiting for the user; long press is a no-op. */
        return ESPUI_NAV_NONE;
    case ESPUI_NAV_SCENE_AMBIENT:
        router->scene = ESPUI_SCENE_AMBIENT;
        break;
    case ESPUI_NAV_SCENE_FOCUS:
        router->scene = ESPUI_SCENE_FOCUS;
        break;
    case ESPUI_NAV_SCENE_ACTIVITY:
        router->scene = ESPUI_SCENE_ACTIVITY;
        break;
    case ESPUI_NAV_SCENE_ATTENTION:
        router->scene = ESPUI_SCENE_ATTENTION;
        break;
    case ESPUI_NAV_SCENE_SETTINGS:
        router->scene = ESPUI_SCENE_SETTINGS;
        break;
    default:
        break;
    }

    return intent;
}
