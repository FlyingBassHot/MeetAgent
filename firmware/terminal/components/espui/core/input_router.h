/*
 * Input router.
 *
 * Converts raw HAL input events into abstract navigation intents. It is pure
 * logic: no LVGL, no FreeRTOS, no Node access. The UI task decides what each
 * intent means for the active scene.
 */
#pragma once

#include <stdint.h>

#include "espui_scene.h"
#include "espui_types.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    espui_scene_id_t scene;
    uint16_t focus_index;
    uint16_t attention_count;
} espui_input_router_t;

void espui_input_router_init(espui_input_router_t *router);
void espui_input_router_set_scene(espui_input_router_t *router,
                                  espui_scene_id_t scene);
void espui_input_router_set_attention_count(espui_input_router_t *router,
                                            uint16_t attention_count);

/* Returns ESPUI_NAV_NONE for unknown/disabled input. */
espui_nav_intent_t espui_input_router_feed(espui_input_router_t *router,
                                           const espui_input_event_t *event);

#ifdef __cplusplus
}
#endif
