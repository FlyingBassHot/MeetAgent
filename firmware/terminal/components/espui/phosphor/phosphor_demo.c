#include "phosphor_demo.h"

#include <string.h>

phosphor_demo_phase_t phosphor_demo_phase_at(uint32_t elapsed_ms)
{
    uint32_t t = elapsed_ms % PHOSPHOR_DEMO_CYCLE_MS;
    if (t < 4000u) {
        return PHOSPHOR_DEMO_IDLE;
    }
    if (t < 8000u) {
        return PHOSPHOR_DEMO_WORKING;
    }
    if (t < 12000u) {
        return PHOSPHOR_DEMO_APPROVAL;
    }
    if (t < 16000u) {
        return PHOSPHOR_DEMO_ERROR;
    }
    if (t < 18000u) {
        return PHOSPHOR_DEMO_COMPLETED;
    }
    return PHOSPHOR_DEMO_OFFLINE;
}

const char *phosphor_demo_phase_name(phosphor_demo_phase_t phase)
{
    switch (phase) {
    case PHOSPHOR_DEMO_WORKING:
        return "working";
    case PHOSPHOR_DEMO_APPROVAL:
        return "working+approval";
    case PHOSPHOR_DEMO_ERROR:
        return "working+error";
    case PHOSPHOR_DEMO_COMPLETED:
        return "completed";
    case PHOSPHOR_DEMO_OFFLINE:
        return "offline";
    case PHOSPHOR_DEMO_IDLE:
    default:
        return "idle";
    }
}

static void add_session(phosphor_input_state_t *input, const char *id,
                        phosphor_session_status_t status)
{
    if (input->session_count >= PHOSPHOR_MAX_SESSIONS) {
        return;
    }
    phosphor_session_input_t *s = &input->sessions[input->session_count++];
    memset(s, 0, sizeof(*s));
    strncpy(s->session_id, id, sizeof(s->session_id) - 1);
    s->status = status;
}

void phosphor_demo_build_input(phosphor_demo_phase_t phase, uint32_t cycle,
                               phosphor_input_state_t *input)
{
    if (input == NULL) {
        return;
    }
    memset(input, 0, sizeof(*input));
    input->node_online = true;
    switch (phase) {
    case PHOSPHOR_DEMO_IDLE:
        add_session(input, "demo-idle", PHOSPHOR_SESSION_IDLE);
        break;
    case PHOSPHOR_DEMO_WORKING:
        add_session(input, "demo-run", PHOSPHOR_SESSION_RUNNING);
        break;
    case PHOSPHOR_DEMO_APPROVAL:
        add_session(input, "demo-run", PHOSPHOR_SESSION_RUNNING);
        add_session(input, "demo-wait", PHOSPHOR_SESSION_WAITING);
        input->approval_count = 1;
        break;
    case PHOSPHOR_DEMO_ERROR:
        add_session(input, "demo-run", PHOSPHOR_SESSION_RUNNING);
        add_session(input, "demo-error", PHOSPHOR_SESSION_ERROR);
        input->error_count = 1;
        input->error_event_id = (uint64_t)cycle * 32u + 2u;
        break;
    case PHOSPHOR_DEMO_COMPLETED:
        add_session(input, "demo-run", PHOSPHOR_SESSION_RUNNING);
        input->completed_event_id = (uint64_t)cycle * 32u + 1u;
        break;
    case PHOSPHOR_DEMO_OFFLINE:
    default:
        input->node_online = false;
        break;
    }
    /* Keep all demo satellites stable even while their phase is inactive. */
    if (phase != PHOSPHOR_DEMO_APPROVAL &&
        phase != PHOSPHOR_DEMO_ERROR && phase != PHOSPHOR_DEMO_OFFLINE) {
        /* nothing extra */
    }
}
