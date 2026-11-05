/*
 * MeetAgent V2.1 client protocol types for the terminal.
 *
 * Mirrors docs/v2.1/PROTOCOL.md and pkg/clientproto/protocol.go using fixed
 * capacity C structs. JSON parsing/serialization is intentionally stubbed for
 * E0/E1/E2; E4 will implement it with cJSON (bundled with ESP-IDF).
 *
 * This component must not depend on ESPUI or LVGL.
 */
#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

#define MEET_PROTOCOL_SCHEMA_VERSION 1
#define MEET_PROTOCOL_DEFAULT_PORT 8787
#define MEET_PROTOCOL_WS_PATH "/ws"

#define MEET_PROTOCOL_CLIENT_TYPE_TERMINAL "terminal"

#define MEET_PROTOCOL_CAP_UI_COMPACT "ui.compact"
#define MEET_PROTOCOL_CAP_DISPLAY_TOUCH "display.touch"
#define MEET_PROTOCOL_CAP_INPUT_BUTTON "input.button"
#define MEET_PROTOCOL_CAP_INPUT_ENCODER "input.encoder"
#define MEET_PROTOCOL_CAP_NOTIFICATION_LOCAL "notification.local"

#define MEET_PROTOCOL_MAX_AGENTS 8
#define MEET_PROTOCOL_MAX_SESSIONS 8
#define MEET_PROTOCOL_MAX_PERMISSIONS 6
#define MEET_PROTOCOL_ID_MAX 40
#define MEET_PROTOCOL_NAME_MAX 32
#define MEET_PROTOCOL_TITLE_MAX 48
#define MEET_PROTOCOL_NODE_ID_MAX 64

/* Envelope types. */
#define MEET_PROTOCOL_TYPE_HELLO "hello"
#define MEET_PROTOCOL_TYPE_WELCOME "welcome"
#define MEET_PROTOCOL_TYPE_SNAPSHOT "snapshot"
#define MEET_PROTOCOL_TYPE_EVENT "event"
#define MEET_PROTOCOL_TYPE_COMMAND "command"
#define MEET_PROTOCOL_TYPE_COMMAND_RESULT "command.result"
#define MEET_PROTOCOL_TYPE_HEARTBEAT "heartbeat"
#define MEET_PROTOCOL_TYPE_ERROR "error"

/* Command types. */
#define MEET_PROTOCOL_COMMAND_PING "ping"
#define MEET_PROTOCOL_COMMAND_NODE_STATUS "node.status"
#define MEET_PROTOCOL_COMMAND_SESSION_LIST "session.list"
#define MEET_PROTOCOL_COMMAND_SESSION_PROMPT "session.prompt"
#define MEET_PROTOCOL_COMMAND_SESSION_INTERRUPT "session.interrupt"
#define MEET_PROTOCOL_COMMAND_PERMISSION_RESPOND "permission.respond"

/* Normalized event types. */
#define MEET_PROTOCOL_EVENT_AGENT_CONNECTED "agent.connected"
#define MEET_PROTOCOL_EVENT_AGENT_DISCONNECTED "agent.disconnected"
#define MEET_PROTOCOL_EVENT_AGENT_STATUS_CHANGED "agent.status_changed"
#define MEET_PROTOCOL_EVENT_AGENT_ERROR "agent.error"
#define MEET_PROTOCOL_EVENT_SESSION_CREATED "session.created"
#define MEET_PROTOCOL_EVENT_SESSION_UPDATED "session.updated"
#define MEET_PROTOCOL_EVENT_SESSION_DELETED "session.deleted"
#define MEET_PROTOCOL_EVENT_SESSION_STATUS_CHANGED "session.status_changed"
#define MEET_PROTOCOL_EVENT_MESSAGE_STARTED "message.started"
#define MEET_PROTOCOL_EVENT_MESSAGE_DELTA "message.delta"
#define MEET_PROTOCOL_EVENT_MESSAGE_COMPLETED "message.completed"
#define MEET_PROTOCOL_EVENT_TOOL_STARTED "tool.started"
#define MEET_PROTOCOL_EVENT_TOOL_COMPLETED "tool.completed"
#define MEET_PROTOCOL_EVENT_TOOL_FAILED "tool.failed"
#define MEET_PROTOCOL_EVENT_PERMISSION_REQUESTED "permission.requested"
#define MEET_PROTOCOL_EVENT_PERMISSION_RESOLVED "permission.resolved"

typedef enum {
    MEET_NODE_OFFLINE = 0,
    MEET_NODE_CONNECTING,
    MEET_NODE_ONLINE,
    MEET_NODE_SYNCING,
} meet_node_status_t;

typedef enum {
    MEET_AGENT_UNKNOWN = 0,
    MEET_AGENT_IDLE,
    MEET_AGENT_BUSY,
    MEET_AGENT_ERROR,
    MEET_AGENT_OFFLINE,
} meet_agent_status_t;

typedef enum {
    MEET_SESSION_UNKNOWN = 0,
    MEET_SESSION_RUNNING,
    MEET_SESSION_WAITING_APPROVAL,
    MEET_SESSION_COMPLETED,
    MEET_SESSION_FAILED,
    MEET_SESSION_INTERRUPTED,
} meet_session_status_t;

typedef enum {
    MEET_PERMISSION_PENDING = 0,
    MEET_PERMISSION_APPROVED,
    MEET_PERMISSION_REJECTED,
    MEET_PERMISSION_EXPIRED,
    MEET_PERMISSION_UNKNOWN,
} meet_permission_status_t;

typedef struct {
    char session_id[MEET_PROTOCOL_ID_MAX];
    char agent_id[MEET_PROTOCOL_ID_MAX];
    char title[MEET_PROTOCOL_TITLE_MAX];
    meet_session_status_t status;
    uint32_t duration_seconds;
    uint32_t updated_at_unix_s;
} meet_protocol_session_t;

typedef struct {
    char agent_id[MEET_PROTOCOL_ID_MAX];
    char name[MEET_PROTOCOL_NAME_MAX];
    meet_agent_status_t status;
    meet_protocol_session_t sessions[MEET_PROTOCOL_MAX_SESSIONS];
    uint8_t session_count;
} meet_protocol_agent_t;

typedef struct {
    char permission_id[MEET_PROTOCOL_ID_MAX];
    char agent_id[MEET_PROTOCOL_ID_MAX];
    char session_id[MEET_PROTOCOL_ID_MAX];
    char summary[MEET_PROTOCOL_TITLE_MAX];
    meet_permission_status_t status;
    uint32_t created_at_unix_s;
} meet_protocol_permission_t;

typedef struct {
    char node_id[MEET_PROTOCOL_NODE_ID_MAX];
    char name[MEET_PROTOCOL_NAME_MAX];
    char hostname[MEET_PROTOCOL_NAME_MAX];
    char platform[MEET_PROTOCOL_NAME_MAX];
    char version[MEET_PROTOCOL_NAME_MAX];
    uint32_t protocol_version;
    meet_node_status_t status;
} meet_protocol_node_t;

typedef struct {
    uint32_t schema_version;
    meet_protocol_node_t node;
    meet_protocol_agent_t agents[MEET_PROTOCOL_MAX_AGENTS];
    uint8_t agent_count;
    meet_protocol_permission_t permissions[MEET_PROTOCOL_MAX_PERMISSIONS];
    uint8_t permission_count;
    uint64_t last_sequence;
    uint64_t server_time_unix_s;
} meet_protocol_snapshot_t;

typedef struct {
    uint32_t schema_version;
    char event_id[MEET_PROTOCOL_ID_MAX];
    char node_id[MEET_PROTOCOL_NODE_ID_MAX];
    uint64_t sequence;
    char type[MEET_PROTOCOL_TITLE_MAX];
    uint32_t occurred_at_unix_s;
    char agent_id[MEET_PROTOCOL_ID_MAX];
    char session_id[MEET_PROTOCOL_ID_MAX];
    const char *payload_json; /* Non-owning; valid for the callback only. */
    size_t payload_len;
} meet_protocol_event_t;

typedef struct {
    const char *client_id;
    const char *token;
    const char *pairing_code;
    const char *name;
    const char *client_version;
    uint64_t last_sequence;
} meet_protocol_hello_t;

const char *meet_protocol_node_status_name(meet_node_status_t status);
const char *meet_protocol_agent_status_name(meet_agent_status_t status);
const char *meet_protocol_session_status_name(meet_session_status_t status);
const char *meet_protocol_permission_status_name(meet_permission_status_t status);

meet_node_status_t meet_protocol_node_status_from_string(const char *text);
meet_agent_status_t meet_protocol_agent_status_from_string(const char *text);
meet_session_status_t meet_protocol_session_status_from_string(const char *text);
meet_permission_status_t meet_protocol_permission_status_from_string(
    const char *text);

esp_err_t meet_protocol_validate_snapshot(
    const meet_protocol_snapshot_t *snapshot);

/*
 * E4 entry points. E2 keeps the types and validation testable while JSON
 * parsing stays out of the firmware until the Node integration milestone.
 */
esp_err_t meet_protocol_parse_envelope(const char *json, size_t length,
                                       void *out_envelope);
esp_err_t meet_protocol_build_hello(const meet_protocol_hello_t *hello,
                                    char *out_json, size_t capacity,
                                    size_t *out_length);

#ifdef __cplusplus
}
#endif
