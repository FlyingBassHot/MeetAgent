#include "phosphor_state.h"

#include <math.h>
#include <string.h>

#include "phosphor_lut.h"

static bool session_present(const phosphor_input_state_t *input,
                            const char *id)
{
    for (uint8_t i = 0; i < input->session_count &&
                    i < PHOSPHOR_MAX_SESSIONS;
         ++i) {
        if (strcmp(input->sessions[i].session_id, id) == 0) {
            return true;
        }
    }
    return false;
}

void phosphor_state_resolver_init(phosphor_state_resolver_t *resolver)
{
    if (resolver != NULL) {
        memset(resolver, 0, sizeof(*resolver));
    }
}

void phosphor_state_resolver_update(phosphor_state_resolver_t *resolver,
                                    const phosphor_input_state_t *input,
                                    uint32_t now_ms,
                                    phosphor_visual_state_t *visual)
{
    if (resolver == NULL || input == NULL || visual == NULL) {
        return;
    }
    memset(visual, 0, sizeof(*visual));

    visual->running_count = 0;
    visual->waiting_count = 0;
    visual->error_count = 0;
    visual->completed_count = 0;
    for (uint8_t i = 0; i < input->session_count &&
                    i < PHOSPHOR_MAX_SESSIONS;
         ++i) {
        switch (input->sessions[i].status) {
        case PHOSPHOR_SESSION_RUNNING:
            ++visual->running_count;
            break;
        case PHOSPHOR_SESSION_WAITING:
            ++visual->waiting_count;
            break;
        case PHOSPHOR_SESSION_ERROR:
            ++visual->error_count;
            break;
        case PHOSPHOR_SESSION_COMPLETED:
            ++visual->completed_count;
            break;
        default:
            break;
        }
    }

    /* Connectivity has absolute priority: no fake Working animation. */
    if (!input->node_online) {
        visual->activity = PHOSPHOR_ACTIVITY_OFFLINE;
        visual->attention = 0;
        visual->completion_active = false;
        visual->error_glitch_active = false;
        resolver->completion_active = false;
        resolver->error_glitch_active = false;
        visual->approval_breath = 0.5f;
        return;
    }

    visual->activity = visual->running_count > 0 ? PHOSPHOR_ACTIVITY_WORKING
                                                  : PHOSPHOR_ACTIVITY_IDLE;
    if (input->approval_count > 0 || visual->waiting_count > 0) {
        visual->attention |= PHOSPHOR_ATTENTION_APPROVAL;
    }
    if (input->error_count > 0 || visual->error_count > 0) {
        visual->attention |= PHOSPHOR_ATTENTION_ERROR;
    }

    if (input->completed_event_id != 0 &&
        input->completed_event_id != resolver->last_completed_event_id) {
        resolver->last_completed_event_id = input->completed_event_id;
        resolver->completion_start_ms = now_ms;
        resolver->completion_active = true;
    }
    if (resolver->completion_active &&
        now_ms - resolver->completion_start_ms >= PHOSPHOR_COMPLETION_MS) {
        resolver->completion_active = false;
    }
    visual->completion_active = resolver->completion_active;
    visual->completion_event_id = resolver->last_completed_event_id;
    visual->completion_elapsed_ms =
        resolver->completion_active ? now_ms - resolver->completion_start_ms
                                    : 0;

    if (input->error_event_id != 0 &&
        input->error_event_id != resolver->last_error_event_id) {
        resolver->last_error_event_id = input->error_event_id;
        resolver->error_start_ms = now_ms;
        resolver->error_glitch_active = true;
    }
    if (resolver->error_glitch_active &&
        now_ms - resolver->error_start_ms >= PHOSPHOR_ERROR_GLITCH_MS) {
        resolver->error_glitch_active = false;
    }
    visual->error_glitch_active = resolver->error_glitch_active;
    visual->error_glitch_elapsed_ms =
        resolver->error_glitch_active ? now_ms - resolver->error_start_ms : 0;

    float breath_phase = 6.28318530718f *
                         (float)(now_ms % PHOSPHOR_APPROVAL_PERIOD_MS) /
                         (float)PHOSPHOR_APPROVAL_PERIOD_MS;
    visual->approval_breath = 0.5f + 0.5f * phosphor_lut_sin(breath_phase);

    /* Stable satellite slots: existing session IDs retain their slot. */
    for (int i = 0; i < PHOSPHOR_MAX_SATELLITES; ++i) {
        if (resolver->slot_ids[i][0] != '\0' &&
            !session_present(input, resolver->slot_ids[i])) {
            resolver->slot_ids[i][0] = '\0';
        }
    }
    uint8_t overflow = 0;
    for (uint8_t i = 0; i < input->session_count &&
                    i < PHOSPHOR_MAX_SESSIONS;
         ++i) {
        const char *id = input->sessions[i].session_id;
        int slot = -1;
        for (int s = 0; s < PHOSPHOR_MAX_SATELLITES; ++s) {
            if (strcmp(resolver->slot_ids[s], id) == 0) {
                slot = s;
                break;
            }
        }
        if (slot < 0) {
            for (int s = 0; s < PHOSPHOR_MAX_SATELLITES; ++s) {
                if (resolver->slot_ids[s][0] == '\0') {
                    slot = s;
                    break;
                }
            }
        }
        if (slot < 0 || slot >= PHOSPHOR_MAX_SATELLITES ||
            visual->satellite_count >= PHOSPHOR_MAX_SATELLITES) {
            ++overflow;
            continue;
        }
        bool already_output = false;
        for (uint8_t k = 0; k < visual->satellite_count; ++k) {
            if (strcmp(visual->satellites[k].session_id, id) == 0) {
                already_output = true;
                break;
            }
        }
        if (already_output) {
            continue;
        }
        strncpy(resolver->slot_ids[slot], id,
                sizeof(resolver->slot_ids[slot]) - 1);
        resolver->slot_ids[slot][sizeof(resolver->slot_ids[slot]) - 1] = '\0';
        phosphor_satellite_state_t *sat = &visual->satellites[visual->satellite_count++];
        sat->slot = (uint8_t)slot;
        strncpy(sat->session_id, id, sizeof(sat->session_id) - 1);
        sat->session_id[sizeof(sat->session_id) - 1] = '\0';
        sat->status = input->sessions[i].status;
    }
    visual->overflow_count = overflow;
    if (visual->satellite_count == 0) {
        visual->activity = visual->activity == PHOSPHOR_ACTIVITY_WORKING
                               ? PHOSPHOR_ACTIVITY_WORKING
                               : PHOSPHOR_ACTIVITY_IDLE;
    }
}
