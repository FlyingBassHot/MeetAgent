#include "state_adapter.h"

#include <ctype.h>
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


/* ------------------------------------------------------------------------- */
/* Minimal JSON string extraction from event->payload_json (no cJSON)        */

static const char *event_json_find_key(const char *json, const char *key)
{
    if (json == NULL || key == NULL) {
        return NULL;
    }
    size_t key_len = strlen(key);
    const char *p = json;
    while (*p != '\0') {
        if (*p != '"') {
            ++p;
            continue;
        }
        const char *start = ++p;
        while (*p != '\0' && *p != '"') {
            if (*p == '\\' && p[1] != '\0') {
                p += 2;
            } else {
                ++p;
            }
        }
        if (*p != '"') {
            break;
        }
        size_t length = (size_t)(p - start);
        const char *after = p + 1;
        if (length == key_len && memcmp(start, key, key_len) == 0) {
            const char *q = after;
            while (isspace((unsigned char)*q)) {
                ++q;
            }
            if (*q == ':') {
                ++q;
                while (isspace((unsigned char)*q)) {
                    ++q;
                }
                return q;
            }
        }
        ++p;
    }
    return NULL;
}

static bool event_json_get_string(const char *json, const char *key, char *out,
                                  size_t size)
{
    if (out == NULL || size == 0) {
        return false;
    }
    out[0] = '\0';
    const char *value = event_json_find_key(json, key);
    if (value == NULL || *value != '"') {
        return false;
    }
    ++value;
    size_t out_len = 0;
    while (*value != '\0' && *value != '"') {
        char c = *value++;
        if (c == '\\') {
            c = *value++;
            switch (c) {
            case '"':
            case '\\':
            case '/':
                break;
            case 'b':
                c = '\b';
                break;
            case 'f':
                c = '\f';
                break;
            case 'n':
                c = '\n';
                break;
            case 'r':
                c = '\r';
                break;
            case 't':
                c = '\t';
                break;
            case 'u':
                /* Keep the basic ASCII extractor allocation-free. */
                for (int i = 0; i < 4 && *value != '\0'; ++i) {
                    ++value;
                }
                c = '?';
                break;
            default:
                break;
            }
        }
        if (out_len + 1 >= size) {
            out[size - 1] = '\0';
            return false;
        }
        out[out_len++] = c;
    }
    if (*value != '"') {
        out[size - 1] = '\0';
        return false;
    }
    out[out_len] = '\0';
    return true;
}

static void update_agent_active_sessions(espui_view_model_t *model,
                                         const char *agent_id)
{
    if (model == NULL || espui_str_empty(agent_id)) {
        return;
    }
    espui_agent_view_t *agent = espui_view_model_find_agent(model, agent_id);
    if (agent != NULL) {
        agent->active_session_count = count_active_sessions(model, agent_id);
    }
}

static bool event_get_agent_id(const meet_protocol_event_t *event,
                               char *out, size_t size)
{
    if (event->agent_id[0] != '\0') {
        espui_strlcpy(out, event->agent_id, size);
        return true;
    }
    return event_json_get_string(event->payload_json, "agent_id", out, size);
}

static bool event_get_session_id(const meet_protocol_event_t *event,
                                 char *out, size_t size)
{
    if (event->session_id[0] != '\0') {
        espui_strlcpy(out, event->session_id, size);
        return true;
    }
    return event_json_get_string(event->payload_json, "session_id", out, size);
}

static espui_agent_status_t event_agent_status(const meet_protocol_event_t *event)
{
    if (strcmp(event->type, MEET_PROTOCOL_EVENT_AGENT_DISCONNECTED) == 0) {
        return ESPUI_AGENT_OFFLINE;
    }
    if (strcmp(event->type, MEET_PROTOCOL_EVENT_AGENT_ERROR) == 0) {
        return ESPUI_AGENT_ERROR;
    }

    char status[32];
    if (!event_json_get_string(event->payload_json, "status", status,
                               sizeof(status))) {
        return ESPUI_AGENT_IDLE;
    }
    return map_agent(meet_protocol_agent_status_from_string(status));
}

static void apply_agent_event(const meet_protocol_event_t *event,
                              espui_view_model_t *model)
{
    char agent_id[ESPUI_ID_MAX];
    bool id_ok = event_get_agent_id(event, agent_id, sizeof(agent_id));
    if (!id_ok && agent_id[0] != '\0') {
        ESP_LOGW(TAG, "agent_id too long; event dropped seq=%llu",
                 (unsigned long long)event->sequence);
        return;
    }
    if (espui_str_empty(agent_id)) {
        return;
    }
    espui_agent_view_t *agent =
        espui_view_model_upsert_agent(model, agent_id);
    if (agent == NULL) {
        ESP_LOGW(TAG, "agent capacity reached for %s", agent_id);
        return;
    }
    agent->status = event_agent_status(event);
    if (strcmp(event->type, MEET_PROTOCOL_EVENT_AGENT_CONNECTED) == 0) {
        char status[32];
        if (event_json_get_string(event->payload_json, "status", status,
                                  sizeof(status))) {
            agent->status = map_agent(
                meet_protocol_agent_status_from_string(status));
        } else {
            agent->status = ESPUI_AGENT_IDLE;
        }
    }
}

static void apply_session_event(const meet_protocol_event_t *event,
                                espui_view_model_t *model, uint32_t now_ms)
{
    char session_id[ESPUI_ID_MAX];
    bool session_ok = event_get_session_id(event, session_id,
                                           sizeof(session_id));
    if (!session_ok && session_id[0] != '\0') {
        ESP_LOGW(TAG, "session_id too long; event dropped seq=%llu",
                 (unsigned long long)event->sequence);
        return;
    }
    if (espui_str_empty(session_id)) {
        return;
    }

    if (strcmp(event->type, MEET_PROTOCOL_EVENT_SESSION_DELETED) == 0) {
        espui_session_view_t *existing =
            espui_view_model_find_session(model, session_id);
        char agent_id[ESPUI_ID_MAX] = {0};
        if (existing != NULL) {
            espui_strlcpy(agent_id, existing->agent_id, sizeof(agent_id));
        }
        if (espui_view_model_remove_session(model, session_id)) {
            update_agent_active_sessions(model, agent_id);
        }
        return;
    }

    espui_session_view_t *session =
        espui_view_model_upsert_session(model, session_id);
    if (session == NULL) {
        ESP_LOGW(TAG, "session capacity reached for %s", session_id);
        return;
    }

    char agent_id[ESPUI_ID_MAX] = {0};
    bool agent_ok = event_get_agent_id(event, agent_id, sizeof(agent_id));
    if (!agent_ok && agent_id[0] != '\0') {
        ESP_LOGW(TAG, "agent_id too long; session update dropped seq=%llu",
                 (unsigned long long)event->sequence);
        return;
    }
    if (agent_id[0] == '\0') {
        espui_strlcpy(agent_id, session->agent_id, sizeof(agent_id));
    }
    if (agent_id[0] != '\0') {
        espui_strlcpy(session->agent_id, agent_id, sizeof(session->agent_id));
    }

    char title[ESPUI_TITLE_MAX];
    if (event_json_get_string(event->payload_json, "title", title,
                              sizeof(title)) &&
        title[0] != '\0') {
        espui_strlcpy(session->title, title, sizeof(session->title));
    }
    char status[32];
    if (event_json_get_string(event->payload_json, "status", status,
                              sizeof(status)) &&
        status[0] != '\0') {
        session->status =
            map_session(meet_protocol_session_status_from_string(status));
    }
    session->updated_at_ms = now_ms;
    update_agent_active_sessions(model, session->agent_id);
}

static void apply_permission_event(const meet_protocol_event_t *event,
                                   espui_view_model_t *model, uint32_t now_ms)
{
    char permission_id[ESPUI_ID_MAX];
    if (!event_json_get_string(event->payload_json, "permission_id",
                               permission_id, sizeof(permission_id))) {
        return;
    }

    if (strcmp(event->type, MEET_PROTOCOL_EVENT_PERMISSION_RESOLVED) == 0) {
        (void)espui_view_model_resolve_attention(model, permission_id);
        return;
    }

    char description[ESPUI_TITLE_MAX];
    char action[ESPUI_TITLE_MAX];
    event_json_get_string(event->payload_json, "description", description,
                          sizeof(description));
    event_json_get_string(event->payload_json, "action", action,
                          sizeof(action));
    char session_id[ESPUI_ID_MAX] = {0};
    bool session_ok = event_get_session_id(event, session_id,
                                           sizeof(session_id));
    if (!session_ok && session_id[0] != '\0') {
        ESP_LOGW(TAG, "permission session_id too long; event dropped seq=%llu",
                 (unsigned long long)event->sequence);
        return;
    }

    espui_attention_view_t *attention = espui_view_model_push_attention(
        model, permission_id, ESPUI_ATTENTION_APPROVAL);
    if (attention == NULL) {
        ESP_LOGW(TAG, "attention capacity reached for %s", permission_id);
        return;
    }
    espui_strlcpy(attention->session_id, session_id,
                  sizeof(attention->session_id));
    espui_strlcpy(attention->summary,
                  description[0] != '\0' ? description : action,
                  sizeof(attention->summary));
    attention->created_at_ms = now_ms;
}

esp_err_t meet_state_adapter_apply_event(meet_state_adapter_t *adapter,
                                         const meet_protocol_event_t *event,
                                         uint32_t now_ms,
                                         espui_view_model_t *model)
{
    if (adapter == NULL || event == NULL || model == NULL) {
        return ESP_ERR_INVALID_ARG;
    }
    if (event->schema_version != MEET_PROTOCOL_SCHEMA_VERSION) {
        return ESP_ERR_INVALID_ARG;
    }
    if (event->sequence <= adapter->last_sequence) {
        ++adapter->ignored_events;
        return ESP_OK;
    }

    adapter->last_sequence = event->sequence;

    if (strcmp(event->type, MEET_PROTOCOL_EVENT_AGENT_CONNECTED) == 0 ||
        strcmp(event->type, MEET_PROTOCOL_EVENT_AGENT_DISCONNECTED) == 0 ||
        strcmp(event->type, MEET_PROTOCOL_EVENT_AGENT_STATUS_CHANGED) == 0 ||
        strcmp(event->type, MEET_PROTOCOL_EVENT_AGENT_ERROR) == 0) {
        apply_agent_event(event, model);
    } else if (strcmp(event->type, MEET_PROTOCOL_EVENT_SESSION_CREATED) == 0 ||
               strcmp(event->type, MEET_PROTOCOL_EVENT_SESSION_UPDATED) == 0 ||
               strcmp(event->type,
                      MEET_PROTOCOL_EVENT_SESSION_STATUS_CHANGED) == 0 ||
               strcmp(event->type, MEET_PROTOCOL_EVENT_SESSION_DELETED) == 0) {
        apply_session_event(event, model, now_ms);
    } else if (strcmp(event->type,
                      MEET_PROTOCOL_EVENT_PERMISSION_REQUESTED) == 0 ||
               strcmp(event->type,
                      MEET_PROTOCOL_EVENT_PERMISSION_RESOLVED) == 0) {
        apply_permission_event(event, model, now_ms);
    } else if (strcmp(event->type, MEET_PROTOCOL_EVENT_TOOL_FAILED) == 0) {
        espui_view_model_note_error(model);
    } else if (strcmp(event->type,
                      MEET_PROTOCOL_EVENT_MESSAGE_COMPLETED) == 0) {
        espui_view_model_note_completion(model);
    } else {
        ESP_LOGD(TAG, "ignored event type=%s seq=%llu", event->type,
                 (unsigned long long)event->sequence);
    }

    return ESP_OK;
}
