#include "espui_motion.h"

#include <string.h>

const espui_motion_profile_t espui_motion_profile_none = {
    .id = "motion_none",
    .speed_percent = 0,
    .idle_amplitude_percent = 0,
    .work_speed_percent = 0,
    .transition_ms = 0,
    .target_fps = 0,
    .reduced_motion = true,
};

const espui_motion_profile_t espui_motion_profile_phosphor_default = {
    .id = "phosphor_default",
    .speed_percent = 100,
    .idle_amplitude_percent = 30,
    .work_speed_percent = 100,
    .transition_ms = 220,
    .target_fps = 15,
    .reduced_motion = false,
};

const espui_motion_profile_t *espui_motion_profile_find(const char *id)
{
    if (id == NULL) {
        return NULL;
    }
    if (strcmp(id, espui_motion_profile_none.id) == 0) {
        return &espui_motion_profile_none;
    }
    if (strcmp(id, espui_motion_profile_phosphor_default.id) == 0) {
        return &espui_motion_profile_phosphor_default;
    }
    return NULL;
}

const espui_motion_profile_t *espui_motion_profile_default(void)
{
    return &espui_motion_profile_phosphor_default;
}
