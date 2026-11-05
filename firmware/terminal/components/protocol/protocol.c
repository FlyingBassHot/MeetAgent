#include "meet_protocol.h"

#include <ctype.h>
#include <string.h>

#include "esp_log.h"

static const char *TAG = "meet_protocol";

static bool ieq(const char *a, const char *b)
{
    if (a == NULL || b == NULL) {
        return false;
    }
    while (*a != '\0' && *b != '\0') {
        if (tolower((unsigned char)*a) != tolower((unsigned char)*b)) {
            return false;
        }
        ++a;
        ++b;
    }
    return *a == '\0' && *b == '\0';
}

static bool empty(const char *text)
{
    return text == NULL || text[0] == '\0';
}

const char *meet_protocol_node_status_name(meet_node_status_t status)
{
    switch (status) {
    case MEET_NODE_OFFLINE:
        return "offline";
    case MEET_NODE_CONNECTING:
        return "connecting";
    case MEET_NODE_ONLINE:
        return "online";
    case MEET_NODE_SYNCING:
        return "syncing";
    default:
        return "offline";
    }
}

const char *meet_protocol_agent_status_name(meet_agent_status_t status)
{
    switch (status) {
    case MEET_AGENT_IDLE:
        return "idle";
    case MEET_AGENT_BUSY:
        return "busy";
    case MEET_AGENT_ERROR:
        return "error";
    case MEET_AGENT_OFFLINE:
        return "offline";
    case MEET_AGENT_UNKNOWN:
    default:
        return "unknown";
    }
}

const char *meet_protocol_session_status_name(meet_session_status_t status)
{
    switch (status) {
    case MEET_SESSION_RUNNING:
        return "running";
    case MEET_SESSION_WAITING_APPROVAL:
        return "waiting_approval";
    case MEET_SESSION_COMPLETED:
        return "completed";
    case MEET_SESSION_FAILED:
        return "failed";
    case MEET_SESSION_INTERRUPTED:
        return "interrupted";
    case MEET_SESSION_UNKNOWN:
    default:
        return "unknown";
    }
}

const char *meet_protocol_permission_status_name(meet_permission_status_t status)
{
    switch (status) {
    case MEET_PERMISSION_PENDING:
        return "pending";
    case MEET_PERMISSION_APPROVED:
        return "approved";
    case MEET_PERMISSION_REJECTED:
        return "rejected";
    case MEET_PERMISSION_EXPIRED:
        return "expired";
    case MEET_PERMISSION_UNKNOWN:
    default:
        return "unknown";
    }
}

meet_node_status_t meet_protocol_node_status_from_string(const char *text)
{
    if (ieq(text, "online") || ieq(text, "connected")) {
        return MEET_NODE_ONLINE;
    }
    if (ieq(text, "connecting")) {
        return MEET_NODE_CONNECTING;
    }
    if (ieq(text, "syncing")) {
        return MEET_NODE_SYNCING;
    }
    return MEET_NODE_OFFLINE;
}

meet_agent_status_t meet_protocol_agent_status_from_string(const char *text)
{
    if (ieq(text, "busy") || ieq(text, "running") || ieq(text, "working")) {
        return MEET_AGENT_BUSY;
    }
    if (ieq(text, "idle") || ieq(text, "ready") || ieq(text, "online")) {
        return MEET_AGENT_IDLE;
    }
    if (ieq(text, "error") || ieq(text, "failed")) {
        return MEET_AGENT_ERROR;
    }
    if (ieq(text, "offline") || ieq(text, "disconnected")) {
        return MEET_AGENT_OFFLINE;
    }
    return MEET_AGENT_UNKNOWN;
}

meet_session_status_t meet_protocol_session_status_from_string(const char *text)
{
    if (ieq(text, "running") || ieq(text, "active") ||
        ieq(text, "in_progress") || ieq(text, "started")) {
        return MEET_SESSION_RUNNING;
    }
    if (ieq(text, "waiting_approval") || ieq(text, "waiting") ||
        ieq(text, "pending_approval")) {
        return MEET_SESSION_WAITING_APPROVAL;
    }
    if (ieq(text, "completed") || ieq(text, "done") ||
        ieq(text, "finished")) {
        return MEET_SESSION_COMPLETED;
    }
    if (ieq(text, "failed") || ieq(text, "error")) {
        return MEET_SESSION_FAILED;
    }
    if (ieq(text, "interrupted") || ieq(text, "cancelled") ||
        ieq(text, "canceled")) {
        return MEET_SESSION_INTERRUPTED;
    }
    return MEET_SESSION_UNKNOWN;
}

meet_permission_status_t meet_protocol_permission_status_from_string(
    const char *text)
{
    if (ieq(text, "pending")) {
        return MEET_PERMISSION_PENDING;
    }
    if (ieq(text, "approved") || ieq(text, "once") ||
        ieq(text, "always")) {
        return MEET_PERMISSION_APPROVED;
    }
    if (ieq(text, "rejected") || ieq(text, "reject")) {
        return MEET_PERMISSION_REJECTED;
    }
    if (ieq(text, "expired")) {
        return MEET_PERMISSION_EXPIRED;
    }
    return MEET_PERMISSION_UNKNOWN;
}

esp_err_t meet_protocol_validate_snapshot(
    const meet_protocol_snapshot_t *snapshot)
{
    if (snapshot == NULL ||
        snapshot->schema_version != MEET_PROTOCOL_SCHEMA_VERSION) {
        return ESP_ERR_INVALID_ARG;
    }
    if (empty(snapshot->node.node_id)) {
        return ESP_ERR_INVALID_ARG;
    }
    if (snapshot->agent_count > MEET_PROTOCOL_MAX_AGENTS ||
        snapshot->permission_count > MEET_PROTOCOL_MAX_PERMISSIONS) {
        return ESP_ERR_INVALID_ARG;
    }

    for (uint8_t i = 0; i < snapshot->agent_count; ++i) {
        const meet_protocol_agent_t *agent = &snapshot->agents[i];
        if (empty(agent->agent_id) ||
            agent->session_count > MEET_PROTOCOL_MAX_SESSIONS) {
            return ESP_ERR_INVALID_ARG;
        }
        for (uint8_t j = 0; j < agent->session_count; ++j) {
            if (empty(agent->sessions[j].session_id)) {
                return ESP_ERR_INVALID_ARG;
            }
        }
    }

    for (uint8_t i = 0; i < snapshot->permission_count; ++i) {
        if (empty(snapshot->permissions[i].permission_id)) {
            return ESP_ERR_INVALID_ARG;
        }
    }

    return ESP_OK;
}

esp_err_t meet_protocol_parse_envelope(const char *json, size_t length,
                                       void *out_envelope)
{
    (void)json;
    (void)length;
    (void)out_envelope;
    ESP_LOGW(TAG, "JSON envelope parsing is an E4 task");
    return ESP_ERR_NOT_SUPPORTED;
}

esp_err_t meet_protocol_build_hello(const meet_protocol_hello_t *hello,
                                    char *out_json, size_t capacity,
                                    size_t *out_length)
{
    (void)hello;
    if (out_length != NULL) {
        *out_length = 0;
    }
    if (out_json != NULL && capacity > 0) {
        out_json[0] = '\0';
    }
    ESP_LOGW(TAG, "hello JSON serialization is an E4 task");
    return ESP_ERR_NOT_SUPPORTED;
}
