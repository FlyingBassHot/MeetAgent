#pragma once

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    PHOSPHOR_MODE_IDLE = 0,
    PHOSPHOR_MODE_WORKING = 1,
} phosphor_motion_mode_t;

typedef struct {
    float speeds[3];       /* rad/s: inner, middle, outer */
    float breath_period_ms;
    float density;         /* 0..1 */
    float brightness;      /* 0..1 */
} phosphor_motion_profile_t;

const phosphor_motion_profile_t *phosphor_motion_idle_profile(void);
const phosphor_motion_profile_t *phosphor_motion_working_profile(void);

typedef struct {
    uint32_t time_ms;
    uint32_t last_time_ms;
    float orbit_phase[3];  /* radians */
    float breath_phase;    /* radians */
    float breath;          /* 0..1 */
    float mode_mix;        /* 0 idle, 1 working */
    float activity_energy; /* 0..1 low-pass */
    float density;         /* 0..1 */
    float brightness;      /* 0..1 */
    phosphor_motion_mode_t mode;
} phosphor_motion_state_t;

void phosphor_motion_init(phosphor_motion_state_t *state,
                          phosphor_motion_mode_t mode);
void phosphor_motion_update(phosphor_motion_state_t *state, uint32_t time_ms,
                            phosphor_motion_mode_t mode);

#ifdef __cplusplus
}
#endif
