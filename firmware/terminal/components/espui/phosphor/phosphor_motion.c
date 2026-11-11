#include "phosphor_motion.h"

#include <math.h>
#include <stddef.h>

#include "phosphor_lut.h"

#define PHOSPHOR_TWO_PI 6.28318530718f
#define PHOSPHOR_TRANSITION_MS 1000.0f

static const phosphor_motion_profile_t s_idle = {
    .speeds = {0.08f, -0.045f, 0.025f},
    .breath_period_ms = 8000.0f,
    .density = 0.30f,
    .brightness = 0.82f,
};

static const phosphor_motion_profile_t s_working = {
    .speeds = {0.22f, -0.55f, 0.32f},
    .breath_period_ms = 5000.0f,
    .density = 0.75f,
    .brightness = 1.00f,
};

const phosphor_motion_profile_t *phosphor_motion_idle_profile(void)
{
    return &s_idle;
}

const phosphor_motion_profile_t *phosphor_motion_working_profile(void)
{
    return &s_working;
}

static void wrap_phase(float *phase)
{
    while (*phase >= PHOSPHOR_TWO_PI) {
        *phase -= PHOSPHOR_TWO_PI;
    }
    while (*phase < 0.0f) {
        *phase += PHOSPHOR_TWO_PI;
    }
}

void phosphor_motion_init(phosphor_motion_state_t *state,
                          phosphor_motion_mode_t mode)
{
    if (state == NULL) {
        return;
    }
    state->time_ms = 0;
    state->last_time_ms = 0;
    state->orbit_phase[0] = 0.0f;
    state->orbit_phase[1] = 0.0f;
    state->orbit_phase[2] = 0.0f;
    state->breath_phase = 0.0f;
    state->breath = 0.5f;
    state->mode = mode;
    state->mode_mix = mode == PHOSPHOR_MODE_WORKING ? 1.0f : 0.0f;
    state->activity_energy = state->mode_mix;
    state->density = mode == PHOSPHOR_MODE_WORKING ? s_working.density
                                                    : s_idle.density;
    state->brightness = mode == PHOSPHOR_MODE_WORKING ? s_working.brightness
                                                       : s_idle.brightness;
    phosphor_lut_init();
}

void phosphor_motion_update(phosphor_motion_state_t *state, uint32_t time_ms,
                            phosphor_motion_mode_t mode)
{
    if (state == NULL) {
        return;
    }
    if (time_ms < state->last_time_ms) {
        state->last_time_ms = time_ms;
        return;
    }
    uint32_t delta = time_ms - state->last_time_ms;
    if (delta > 200u) {
        delta = 200u;
    }
    if (delta == 0 && time_ms == state->last_time_ms) {
        state->mode = mode;
        return;
    }

    float target = mode == PHOSPHOR_MODE_WORKING ? 1.0f : 0.0f;
    float step = (float)delta / PHOSPHOR_TRANSITION_MS;
    if (state->mode_mix < target) {
        state->mode_mix += step;
        if (state->mode_mix > target) {
            state->mode_mix = target;
        }
    } else if (state->mode_mix > target) {
        state->mode_mix -= step;
        if (state->mode_mix < target) {
            state->mode_mix = target;
        }
    }

    float dt_s = (float)delta / 1000.0f;
    for (int i = 0; i < 3; ++i) {
        float speed = s_idle.speeds[i] +
                      (s_working.speeds[i] - s_idle.speeds[i]) *
                          state->mode_mix;
        state->orbit_phase[i] += speed * dt_s;
        wrap_phase(&state->orbit_phase[i]);
    }

    float breath_period = s_idle.breath_period_ms +
                          (s_working.breath_period_ms -
                           s_idle.breath_period_ms) *
                              state->mode_mix;
    state->breath_phase += PHOSPHOR_TWO_PI * (float)delta /
                           breath_period;
    wrap_phase(&state->breath_phase);
    state->breath = 0.5f + 0.5f * phosphor_lut_sin(state->breath_phase);

    float energy_rate = (float)delta / 450.0f;
    if (energy_rate > 1.0f) {
        energy_rate = 1.0f;
    }
    state->activity_energy +=
        (target - state->activity_energy) * energy_rate;
    if (state->activity_energy < 0.0f) {
        state->activity_energy = 0.0f;
    } else if (state->activity_energy > 1.0f) {
        state->activity_energy = 1.0f;
    }

    state->density = s_idle.density +
                     (s_working.density - s_idle.density) *
                         state->mode_mix;
    state->density += 0.025f * (state->breath - 0.5f) *
                      (1.0f - state->mode_mix);
    state->brightness = s_idle.brightness +
                        (s_working.brightness - s_idle.brightness) *
                            state->mode_mix;
    state->time_ms = time_ms;
    state->last_time_ms = time_ms;
    state->mode = mode;
}
