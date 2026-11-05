#include "state_adapter.h"

#include <string.h>

#include "esp_log.h"
#include "espui_util.h"

static const char *TAG = "meet_state";

_Static_assert(MEET_PROTOCOL_MAX_AGENTS <= ESPUI_MAX_AGENTS,
               "protocol agent capacity must fit the ViewModel");
_Static_assert(MEET_PROTOCOL_MAX_SESSIONS <= ESPUI_MAX_SESSIONS,
               "protocol session capacity must fit the ViewModel");
_Static_assert(MEET_PROTOCOL_MAX_PERMISSIONS <= ESPUI_MAX_ATTENTION,
               "protocol permission capacity must fit the ViewModel");

static espui_node_status_t map_node(meet_node_status_t status)
{
    switch (status) {
    case MEET_NODE_ONLINE:
        return ESPUI_NODE_ONLINE;
    case MEET_NODE_CONNECTING:
        return ESPUI_NODE_CONNECTING;
    case MEET_NODE_SYNCING:
        return ESPUI_NODE_SYNCING;
    case MEET_NODE_OFFLINE:
    default:
        return ESPUI_NODE_OFFLINE;
    }
}

static espui_agent_status_t map_agent(meet_agent_status_t status)
{
    switch (status) {
    case MEET_AGENT_IDLE:
        return ESPUI_AGENT_IDLE;
    case MEET_AGENT_BUSY:
        return ESPUI_AGENT_BUSY;
    case MEET_AGENT_ERROR:
        return ESPUI_AGENT_ERROR;
    case MEET_AGENT_OFFLINE:
        return ESPUI_AGENT_OFFLINE;
    case MEET_AGENT_UNKNOWN:
    default:
        return ESPUI_AGENT_UNKNOWN;
    }
}

static espui_session_status_t map_session(meet_session_status_t status)
{
    switch (status) {
    case MEET_SESSION_RUNNING:
        return ESPUI_SESSION_RUNNING;
    case MEET_SESSION_WAITING_APPROVAL:
        return ESPUI_SESSION_WAITING_APPROVAL;
    case MEET_SESSION_COMPLETED:
        return ESPUI_SESSION_COMPLETED;
    case MEET_SESSION_FAILED:
        return ESPUI_SESSION_FAILED;
    case MEET_SESSION_INTERRUPTED:
        return ESPUI_SESSION_INTERRUPTED;
    case MEET_SESSION_UNKNOWN:
    default:
        return ESPUI_SESSION_UNKNOWN;
    }
}

static uint16_t count_active_sessions(const espui_view_model_t *model,
                                      const char *agent_id)
{
    uint16_t count = 0;
    for (size_t i = 0; i < ESPUI_MAX_SESSIONS; ++i) {
        const espui_session_view_t *session = &model->sessions[i];
        if (!session->in_use || strcmp(session->agent_id, agent_id) != 0) {
            continue;
        }
        if (session->status == ESPUI_SESSION_RUNNING ||
            session->status == ESPUI_SESSION_WAITING_APPROVAL) {
            ++count;
        }
    }
    return count;
}

void meet_state_adapter_init(meet_state_adapter_t *adapter)
{
    if (adapter == NULL) {
        return;
    }
    memset(adapter, 0, sizeof(*adapter));
}

esp_err_t meet_state_adapter_apply_snapshot(
    meet_state_adapter_t *adapter, const meet_protocol_snapshot_t *snapshot,
    uint32_t now_ms, espui_view_model_t *model)
{
    if (adapter == NULL || snapshot == NULL || model == NULL) {
        return ESP_ERR_INVALID_ARG;
    }
    if (meet_protocol_validate_snapshot(snapshot) != ESP_OK) {
        return ESP_ERR_INVALID_ARG;
    }

    espui_view_model_reset(model, true);

    model->node.status = map_node(snapshot->node.status);
    model->node.connected = snapshot->node.status == MEET_NODE_ONLINE ||
                            snapshot->node.status == MEET_NODE_SYNCING;
    model->node.syncing = snapshot->node.status == MEET_NODE_SYNCING;
    espui_strlcpy(model->node.node_id, snapshot->node.node_id,
                  sizeof(model->node.node_id));
    espui_strlcpy(model->node.display_name, snapshot->node.name,
                  sizeof(model->node.display_name));
    model->node.last_sync_ms = now_ms;

    for (uint8_t i = 0; i < snapshot->agent_count; ++i) {
        const meet_protocol_agent_t *source = &snapshot->agents[i];
        espui_agent_view_t *agent =
            espui_view_model_upsert_agent(model, source->agent_id);
        if (agent == NULL) {
            continue;
        }
        agent->status = map_agent(source->status);
        espui_strlcpy(agent->display_name, source->name,
                      sizeof(agent->display_name));

        for (uint8_t j = 0; j < source->session_count; ++j) {
            const meet_protocol_session_t *session = &source->sessions[j];
            espui_session_view_t *target =
                espui_view_model_upsert_session(model, session->session_id);
            if (target == NULL) {
                continue;
            }
            target->status = map_session(session->status);
            target->duration_seconds = session->duration_seconds;
            target->updated_at_ms = now_ms;
            espui_strlcpy(target->title, session->title, sizeof(target->title));
            if (target->agent_id[0] == '\0') {
                espui_strlcpy(target->agent_id, source->agent_id,
                              sizeof(target->agent_id));
            }
        }

        agent->active_session_count =
            count_active_sessions(model, source->agent_id);
    }

    for (uint8_t i = 0; i < snapshot->permission_count; ++i) {
        const meet_protocol_permission_t *permission = &snapshot->permissions[i];
        if (permission->status != MEET_PERMISSION_PENDING) {
            continue;
        }
        espui_attention_view_t *attention = espui_view_model_push_attention(
            model, permission->permission_id, ESPUI_ATTENTION_APPROVAL);
        if (attention == NULL) {
            continue;
        }
        espui_strlcpy(attention->session_id, permission->session_id,
                      sizeof(attention->session_id));
        espui_strlcpy(attention->summary, permission->summary,
                      sizeof(attention->summary));
        attention->created_at_ms = now_ms;
    }

    adapter->last_sequence = snapshot->last_sequence;
    ++adapter->applied_snapshots;

    ESP_LOGI(TAG, "snapshot applied: agents=%u sessions=%u attention=%u seq=%llu",
             (unsigned)espui_view_model_agent_count(model),
             (unsigned)espui_view_model_session_count(model),
             (unsigned)espui_view_model_attention_count(model),
             (unsigned long long)adapter->last_sequence);
    return ESP_OK;
}

esp_err_t meet_state_adapter_apply_event(meet_state_adapter_t *adapter,
                                         const meet_protocol_event_t *event,
                                         uint32_t now_ms,
                                         espui_view_model_t *model)
{
    (void)adapter;
    (void)event;
    (void)now_ms;
    (void)model;
    ESP_LOGW(TAG, "event application is an E4 task");
    return ESP_ERR_NOT_SUPPORTED;
}
