/*
 * Motion Profile types.
 *
 * A Manifest references a profile by id. E2 provides two built-in profiles and
 * the Animation Scheduler already supports speed/pause; the actual six-state
 * motion recipes arrive with the E3 ASCII renderer.
 */
#pragma once

#include <stdbool.h>
#include <stdint.h>

#include "espui_types.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    const char *id;
    uint16_t speed_percent;         /* 100 = 1.0 */
    uint8_t idle_amplitude_percent; /* 0..100 */
    uint16_t work_speed_percent;    /* 100 = 1.0 */
    uint16_t transition_ms;
    uint8_t target_fps;
    bool reduced_motion; /* OS/user reduce-motion preference simulation */
} espui_motion_profile_t;

extern const espui_motion_profile_t espui_motion_profile_none;
extern const espui_motion_profile_t espui_motion_profile_phosphor_default;

const espui_motion_profile_t *espui_motion_profile_find(const char *id);
const espui_motion_profile_t *espui_motion_profile_default(void);

#ifdef __cplusplus
}
#endif
