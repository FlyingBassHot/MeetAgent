#include "phosphor_view_model_adapter.h"

#include <string.h>

static void copy_id(char *dst, size_t size, const char *src)
{
    if (dst == NULL || size == 0) {
        return;
    }
    if (src == NULL) {
        dst[0] = '\0';
        return;
    }
    size_t length = strlen(src);
    if (length >= size) {
        length = size - 1;
    }
    memcpy(dst, src, length);
    dst[length] = '\0';
}

static phosphor_session_status_t map_session(espui_session_status_t status)
{
    switch (status) {
    case ESPUI_SESSION_RUNNING:
        return PHOSPHOR_SESSION_RUNNING;
    case ESPUI_SESSION_WAITING_APPROVAL:
        return PHOSPHOR_SESSION_WAITING;
    case ESPUI_SESSION_FAILED:
        return PHOSPHOR_SESSION_ERROR;
    case ESPUI_SESSION_COMPLETED:
        return PHOSPHOR_SESSION_COMPLETED;
    case ESPUI_SESSION_INTERRUPTED:
    case ESPUI_SESSION_UNKNOWN:
    default:
        return PHOSPHOR_SESSION_IDLE;
    }
}

static bool model_node_online(const espui_view_model_t *model)
{
    return model->node.status == ESPUI_NODE_ONLINE ||
           model->node.status == ESPUI_NODE_SYNCING;
}

static bool resolve_node_online(
    const espui_view_model_t *model,
    const phosphor_connection_input_t *connection)
{
    bool online = model_node_online(model);
    if (connection == NULL || !connection->valid) {
        return online;
    }
    switch (connection->state) {
    case PHOSPHOR_CONNECTION_ONLINE:
        return true;
    case PHOSPHOR_CONNECTION_CONNECTING:
        /* The Core has no Stale visual; keep Offline until the next welcome
         * instead of replaying the previous snapshot's Working animation. */
        return false;
    case PHOSPHOR_CONNECTION_ERROR:
    case PHOSPHOR_CONNECTION_OFFLINE:
    default:
        return false;
    }
}

phosphor_input_state_t phosphor_view_model_to_input(
    const espui_view_model_t *model, uint64_t completion_event_hash,
    const phosphor_connection_input_t *connection)
{
    phosphor_input_state_t input;
    memset(&input, 0, sizeof(input));
    if (model == NULL) {
        return input;
    }

    input.node_online = resolve_node_online(model, connection);

    for (size_t i = 0; i < ESPUI_MAX_SESSIONS &&
                       input.session_count < PHOSPHOR_MAX_SESSIONS;
         ++i) {
        const espui_session_view_t *session = &model->sessions[i];
        if (!session->in_use || session->session_id[0] == '\0') {
            continue;
        }
        phosphor_session_input_t *target =
            &input.sessions[input.session_count++];
        copy_id(target->session_id, sizeof(target->session_id),
                session->session_id);
        target->status = map_session(session->status);
    }

    uint32_t approvals = 0;
    uint32_t errors = 0;
    for (size_t i = 0; i < ESPUI_MAX_ATTENTION; ++i) {
        const espui_attention_view_t *attention = &model->attention[i];
        if (!attention->in_use) {
            continue;
        }
        if (attention->kind == ESPUI_ATTENTION_APPROVAL) {
            ++approvals;
        } else if (attention->kind == ESPUI_ATTENTION_ERROR) {
            ++errors;
        }
    }
    /* Model counters include transient alerts and are capped at 99. */
    if (model->approval_count > approvals) {
        approvals = model->approval_count;
    }
    if (model->error_count > errors) {
        errors = model->error_count;
    }
    input.approval_count = (uint8_t)(approvals > UINT8_MAX ? UINT8_MAX
                                                           : approvals);
    input.error_count =
        (uint8_t)(errors > UINT8_MAX ? UINT8_MAX : errors);
    input.completed_event_id = completion_event_hash;
    input.error_event_id = 0;
    return input;
}

uint64_t phosphor_completion_hash(const char *event_id, uint64_t sequence)
{
    if ((event_id == NULL || event_id[0] == '\0') && sequence == 0) {
        return 0;
    }
    uint64_t hash = 1469598103934665603ULL;
    if (event_id != NULL) {
        for (const unsigned char *p = (const unsigned char *)event_id;
             *p != '\0'; ++p) {
            hash ^= (uint64_t)*p;
            hash *= 1099511628211ULL;
        }
    }
    hash ^= sequence + 0x9e3779b97f4a7c15ULL;
    hash *= 1099511628211ULL;
    hash ^= hash >> 32;
    return hash != 0 ? hash : 1;
}
