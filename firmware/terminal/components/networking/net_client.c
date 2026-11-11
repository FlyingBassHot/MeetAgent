/*
 * MeetAgent V2.1 terminal WebSocket client (E4b-2).
 *
 * Large welcome frames are accumulated in a 128 KB PSRAM buffer. Before cJSON
 * touches a welcome, the snapshot.events array is replaced with [] in place
 * (string/escape/nesting aware), leaving only the small node/agents/session
 * summary for cJSON. protocol/state stay cJSON-free.
 */
#include "net_client.h"

#include <ctype.h>
#include <stdio.h>
#include <string.h>

#include "cJSON.h"
#include "esp_heap_caps.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "esp_websocket_client.h"
#include "espui.h"
#include "freertos/event_groups.h"
#include "freertos/semphr.h"
#include "freertos/task.h"
#include "mock_state.h"
#include "nvs.h"
#include "state_adapter.h"
#include "wifi_prov.h"

static const char *TAG = "meet_net";

#define NVS_NAMESPACE "meet_wifi"
#define NVS_KEY_TOKEN "client_token"
#define NVS_KEY_CLIENT_ID "client_id"
#define NVS_KEY_LAST_SEQ "last_sequence"
#define NVS_KEY_PAIRING "pairing_code"

#define WS_MAX_MESSAGE (128 * 1024)
#define WS_CONNECTED_BIT (1u << 0)
#define WS_CLOSED_BIT (1u << 1)
#define WS_WELCOME_BIT (1u << 2)
#define WS_AUTH_FAILED_BIT (1u << 3)

#define HEARTBEAT_INTERVAL_MS 15000
#define HEARTBEAT_MAX_MISSED 3
#define WELCOME_TIMEOUT_MS 20000
#define CONNECT_TIMEOUT_MS 10000

typedef struct {
    int bytes;
    uint32_t parse_ms;
    int events_skipped;
    int agents;
    int sessions_total;
    int sessions_stored;
    int permissions;
    int pending;
    uint64_t sequence;
    bool replay_truncated;
    bool token_set;
} welcome_summary_t;

static meet_net_client_t *s_client;
static EventGroupHandle_t s_events;
static esp_websocket_client_handle_t s_ws;
static TaskHandle_t s_task;
static SemaphoreHandle_t s_lifecycle_lock;
static SemaphoreHandle_t s_model_lock;
static volatile bool s_running;

static char s_token[256];
static char s_client_id[64];
static char s_pairing_code[33];
static uint64_t s_last_sequence;
static bool s_used_token;
static bool s_mock_stopped;
static uint32_t s_message_counter;

static char *s_rx_buf;
static int s_rx_len;
static int s_rx_expected;
static bool s_rx_active;

static meet_state_adapter_t s_adapter;
static espui_view_model_t s_model;
static meet_protocol_snapshot_t *s_snapshot;
static welcome_summary_t s_summary;

static char s_ws_uri[192];
static char s_hb_nonce[40];
static volatile bool s_hb_pending;
static uint32_t s_hb_sent_ms;
static volatile uint32_t s_hb_missed;
static bool s_model_dirty;
static uint32_t s_last_post_ms;

static void copy_str(char *dst, size_t size, const char *src)
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

static void set_state(meet_net_client_t *client, meet_net_state_t state)
{
    if (client == NULL) {
        return;
    }
    if (client->state == state) {
        return;
    }
    client->state = state;
    if (client->callbacks.on_state != NULL) {
        client->callbacks.on_state(state, client->user_ctx);
    }
}

/* ------------------------------------------------------------------------- */
/* NVS                                                                       */

static void nvs_load_string(const char *key, char *out, size_t size)
{
    if (size == 0) {
        return;
    }
    out[0] = '\0';
    nvs_handle_t handle;
    if (nvs_open(NVS_NAMESPACE, NVS_READONLY, &handle) != ESP_OK) {
        return;
    }
    size_t length = size;
    if (nvs_get_str(handle, key, out, &length) != ESP_OK) {
        out[0] = '\0';
    }
    nvs_close(handle);
    out[size - 1] = '\0';
}

static void load_auth(void)
{
    nvs_load_string(NVS_KEY_TOKEN, s_token, sizeof(s_token));
    nvs_load_string(NVS_KEY_CLIENT_ID, s_client_id, sizeof(s_client_id));
    nvs_load_string(NVS_KEY_PAIRING, s_pairing_code, sizeof(s_pairing_code));

    s_last_sequence = 0;
    nvs_handle_t handle;
    if (nvs_open(NVS_NAMESPACE, NVS_READONLY, &handle) == ESP_OK) {
        (void)nvs_get_u64(handle, NVS_KEY_LAST_SEQ, &s_last_sequence);
        nvs_close(handle);
    }

    ESP_LOGI(TAG, "auth: token=%s pairing_code=%s last_sequence=%llu",
             s_token[0] != '\0' ? "set" : "empty",
             s_pairing_code[0] != '\0' ? "set" : "empty",
             (unsigned long long)s_last_sequence);
}

static esp_err_t save_auth(const char *token, const char *client_id)
{
    nvs_handle_t handle;
    esp_err_t err = nvs_open(NVS_NAMESPACE, NVS_READWRITE, &handle);
    if (err != ESP_OK) {
        return err;
    }
    if (token != NULL && token[0] != '\0') {
        err = nvs_set_str(handle, NVS_KEY_TOKEN, token);
    }
    if (err == ESP_OK && client_id != NULL && client_id[0] != '\0') {
        err = nvs_set_str(handle, NVS_KEY_CLIENT_ID, client_id);
    }
    if (err == ESP_OK) {
        err = nvs_commit(handle);
    }
    nvs_close(handle);
    return err;
}

static void save_last_sequence(void)
{
    nvs_handle_t handle;
    if (nvs_open(NVS_NAMESPACE, NVS_READWRITE, &handle) != ESP_OK) {
        return;
    }
    (void)nvs_set_u64(handle, NVS_KEY_LAST_SEQ, s_last_sequence);
    (void)nvs_commit(handle);
    nvs_close(handle);
}

static void clear_token(void)
{
    nvs_handle_t handle;
    if (nvs_open(NVS_NAMESPACE, NVS_READWRITE, &handle) != ESP_OK) {
        s_token[0] = '\0';
        return;
    }
    (void)nvs_set_str(handle, NVS_KEY_TOKEN, "");
    (void)nvs_commit(handle);
    nvs_close(handle);
    s_token[0] = '\0';
}

/* ------------------------------------------------------------------------- */
/* JSON helpers (raw PSRAM buffer; cJSON only sees the pruned welcome)       */

static const char *json_find_key(const char *json, const char *key)
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

static char *json_matching_array(char *open)
{
    if (open == NULL || *open != '[') {
        return NULL;
    }
    int depth = 0;
    bool in_string = false;
    bool escaped = false;
    for (char *p = open; *p != '\0'; ++p) {
        char c = *p;
        if (in_string) {
            if (escaped) {
                escaped = false;
            } else if (c == '\\') {
                escaped = true;
            } else if (c == '"') {
                in_string = false;
            }
            continue;
        }
        if (c == '"') {
            in_string = true;
        } else if (c == '[') {
            ++depth;
        } else if (c == ']') {
            --depth;
            if (depth == 0) {
                return p;
            }
        }
    }
    return NULL;
}

static int json_count_array_items(const char *open, const char *close)
{
    int count = 0;
    int depth = 0;
    bool in_string = false;
    bool escaped = false;
    bool in_item = false;
    for (const char *p = open + 1; p < close; ++p) {
        char c = *p;
        if (in_string) {
            if (escaped) {
                escaped = false;
            } else if (c == '\\') {
                escaped = true;
            } else if (c == '"') {
                in_string = false;
            }
            continue;
        }
        if (c == '"') {
            in_string = true;
        } else if (c == '[' || c == '{') {
            if (depth == 0 && !in_item) {
                ++count;
                in_item = true;
            }
            ++depth;
        } else if (c == ']' || c == '}') {
            if (depth > 0) {
                --depth;
            }
        } else if (depth == 0) {
            if (c == ',') {
                in_item = false;
            } else if (!isspace((unsigned char)c) && !in_item) {
                ++count;
                in_item = true;
            }
        }
    }
    return count;
}

static int json_elide_events(char *json)
{
    char *value = (char *)json_find_key(json, "events");
    if (value == NULL || *value != '[') {
        return 0;
    }
    char *open = value;
    char *close = json_matching_array(open);
    if (close == NULL) {
        return -1;
    }
    int count = json_count_array_items(open, close);
    memmove(open + 1, close, strlen(close) + 1);
    return count;
}

static bool json_top_type(const char *json, char *out, size_t size)
{
    const char *value = json_find_key(json, "type");
    if (value == NULL || *value != '"' || size == 0) {
        return false;
    }
    ++value;
    size_t out_len = 0;
    while (*value != '\0' && *value != '"') {
        if (out_len + 1 < size) {
            out[out_len++] = *value;
        }
        ++value;
    }
    out[out_len] = '\0';
    return out_len > 0;
}

static cJSON *json_item(cJSON *object, const char *key)
{
    return cJSON_IsObject(object)
               ? cJSON_GetObjectItemCaseSensitive(object, key)
               : NULL;
}

static void json_get_string(cJSON *object, const char *key, char *out,
                            size_t size)
{
    if (out == NULL || size == 0) {
        return;
    }
    out[0] = '\0';
    cJSON *item = json_item(object, key);
    if (cJSON_IsString(item) && item->valuestring != NULL) {
        copy_str(out, size, item->valuestring);
    }
}

static uint64_t json_get_u64(cJSON *object, const char *key)
{
    cJSON *item = json_item(object, key);
    return cJSON_IsNumber(item) ? (uint64_t)item->valuedouble : 0;
}

static int json_get_int(cJSON *object, const char *key, int fallback)
{
    cJSON *item = json_item(object, key);
    return cJSON_IsNumber(item) ? item->valueint : fallback;
}

static bool json_get_bool(cJSON *object, const char *key)
{
    cJSON *item = json_item(object, key);
    return cJSON_IsTrue(item);
}

static void append_csv(char *dst, size_t size, const char *value)
{
    size_t length = strlen(dst);
    if (length >= size - 1) {
        return;
    }
    if (length > 0) {
        dst[length++] = ',';
        dst[length] = '\0';
    }
    copy_str(dst + length, size - length, value);
}

/* ------------------------------------------------------------------------- */
/* Hello / heartbeat                                                         */

static bool send_text(const char *text, size_t length, const char *what)
{
    if (s_ws == NULL || !esp_websocket_client_is_connected(s_ws)) {
        ESP_LOGW(TAG, "%s not sent: websocket disconnected", what);
        return false;
    }
    int sent = esp_websocket_client_send_text(s_ws, text, (int)length,
                                              pdMS_TO_TICKS(5000));
    if (sent < 0) {
        ESP_LOGW(TAG, "%s send failed", what);
        return false;
    }
    return true;
}

static bool send_hello(void)
{
    static const char *CAPABILITIES[] = {
        "ui.compact",
        "display.touch",
        "input.button",
        "input.encoder",
        "notification.local",
    };

    cJSON *root = cJSON_CreateObject();
    if (root == NULL) {
        return false;
    }
    cJSON_AddNumberToObject(root, "schema_version", MEET_PROTOCOL_SCHEMA_VERSION);
    cJSON_AddStringToObject(root, "type", MEET_PROTOCOL_TYPE_HELLO);
    char message_id[48];
    (void)snprintf(message_id, sizeof(message_id), "cmsg-terminal-%u",
                   (unsigned)++s_message_counter);
    cJSON_AddStringToObject(root, "message_id", message_id);
    cJSON *payload = cJSON_AddObjectToObject(root, "payload");
    if (payload == NULL) {
        cJSON_Delete(root);
        return false;
    }
    cJSON_AddStringToObject(payload, "client_type",
                            MEET_PROTOCOL_CLIENT_TYPE_TERMINAL);
    cJSON_AddStringToObject(payload, "client_version", "0.1.0");
    cJSON_AddStringToObject(payload, "name", "MeetAgent Terminal");
    cJSON *capabilities = cJSON_AddArrayToObject(payload, "capabilities");
    for (size_t i = 0; i < sizeof(CAPABILITIES) / sizeof(CAPABILITIES[0]); ++i) {
        cJSON_AddItemToArray(capabilities,
                             cJSON_CreateString(CAPABILITIES[i]));
    }
    cJSON_AddNumberToObject(payload, "last_sequence",
                            (double)s_last_sequence);
    if (s_client_id[0] != '\0') {
        cJSON_AddStringToObject(payload, "client_id", s_client_id);
    }

    s_used_token = s_token[0] != '\0';
    if (s_used_token) {
        cJSON_AddStringToObject(payload, "token", s_token);
    } else if (s_pairing_code[0] != '\0') {
        cJSON_AddStringToObject(payload, "pairing_code", s_pairing_code);
    } else {
        ESP_LOGW(TAG, "hello not sent: no token or pairing_code");
        cJSON_Delete(root);
        return false;
    }

    char *json = cJSON_PrintUnformatted(root);
    cJSON_Delete(root);
    if (json == NULL) {
        return false;
    }
    size_t length = strlen(json);
    ESP_LOGI(TAG, "hello: auth=%s last_sequence=%llu caps=5 bytes=%u",
             s_used_token ? "token" : "pairing_code",
             (unsigned long long)s_last_sequence, (unsigned)length);
    bool ok = send_text(json, length, "hello");
    cJSON_free(json);
    return ok;
}

static bool send_heartbeat(void)
{
    cJSON *root = cJSON_CreateObject();
    if (root == NULL) {
        return false;
    }
    cJSON_AddNumberToObject(root, "schema_version", MEET_PROTOCOL_SCHEMA_VERSION);
    cJSON_AddStringToObject(root, "type", MEET_PROTOCOL_TYPE_HEARTBEAT);
    cJSON *payload = cJSON_AddObjectToObject(root, "payload");
    if (payload == NULL) {
        cJSON_Delete(root);
        return false;
    }
    (void)snprintf(s_hb_nonce, sizeof(s_hb_nonce), "hb-%u",
                   (unsigned)++s_message_counter);
    cJSON_AddStringToObject(payload, "nonce", s_hb_nonce);
    char *json = cJSON_PrintUnformatted(root);
    cJSON_Delete(root);
    if (json == NULL) {
        return false;
    }
    bool ok = send_text(json, strlen(json), "heartbeat");
    cJSON_free(json);
    if (ok) {
        s_hb_pending = true;
        s_hb_sent_ms = (uint32_t)(esp_timer_get_time() / 1000);
    }
    return ok;
}

/* ------------------------------------------------------------------------- */
/* Incoming messages                                                         */

static void process_heartbeat(cJSON *root)
{
    cJSON *payload = json_item(root, "payload");
    char nonce[40];
    json_get_string(payload, "nonce", nonce, sizeof(nonce));
    if (!s_hb_pending || nonce[0] == '\0' || strcmp(nonce, s_hb_nonce) != 0) {
        return;
    }
    uint32_t now_ms = (uint32_t)(esp_timer_get_time() / 1000);
    uint32_t rtt_ms = now_ms - s_hb_sent_ms;
    s_hb_pending = false;
    s_hb_missed = 0;
    ESP_LOGI(TAG, "heartbeat RTT=%u ms", (unsigned)rtt_ms);
}

/* Caller must hold s_model_lock. */
static void post_model_locked(void)
{
    if (!s_model_dirty) {
        return;
    }
    uint32_t now_ms = (uint32_t)(esp_timer_get_time() / 1000);
    if (s_last_post_ms != 0 && now_ms - s_last_post_ms < 250) {
        return; /* at most 4 posts/second */
    }
    if (espui_post_view_model(&s_model) == ESP_OK) {
        s_model_dirty = false;
        s_last_post_ms = now_ms;
    }
}

static void post_model_if_due(void)
{
    if (s_model_lock == NULL) {
        return;
    }
    if (xSemaphoreTake(s_model_lock, portMAX_DELAY) != pdTRUE) {
        return;
    }
    post_model_locked();
    (void)xSemaphoreGive(s_model_lock);
}

static void process_event(cJSON *root)
{
    uint32_t started_us = (uint32_t)esp_timer_get_time();
    /* Event envelope: {type:"event", payload:{type, sequence, ...}}. */
    cJSON *payload = json_item(root, "payload");
    if (!cJSON_IsObject(payload)) {
        ESP_LOGW(TAG, "event envelope without payload");
        return;
    }
    char type[48];
    char agent_id[40];
    char session_id[40];
    char event_id[40];
    char node_id[64];
    json_get_string(payload, "type", type, sizeof(type));
    json_get_string(payload, "agent_id", agent_id, sizeof(agent_id));
    json_get_string(payload, "session_id", session_id, sizeof(session_id));
    json_get_string(payload, "event_id", event_id, sizeof(event_id));
    json_get_string(payload, "node_id", node_id, sizeof(node_id));
    uint64_t sequence = json_get_u64(payload, "sequence");

    /* Only state-bearing events need their business payload re-serialized.
     * message.delta, tool events and message.completed are counted/ignored
     * without copying potentially large text into internal RAM. */
    bool needs_payload =
        strncmp(type, "agent.", 6) == 0 ||
        strncmp(type, "session.", 8) == 0 ||
        strncmp(type, "permission.", 11) == 0;
    cJSON *business = needs_payload ? json_item(payload, "payload") : NULL;
    char *payload_json =
        business != NULL ? cJSON_PrintUnformatted(business) : NULL;
    if (payload_json == NULL && business != NULL) {
        ESP_LOGW(TAG, "event payload serialization failed type=%s seq=%llu",
                 type, (unsigned long long)sequence);
    }

    meet_protocol_event_t event;
    memset(&event, 0, sizeof(event));
    event.schema_version = MEET_PROTOCOL_SCHEMA_VERSION;
    copy_str(event.event_id, sizeof(event.event_id), event_id);
    copy_str(event.node_id, sizeof(event.node_id), node_id);
    copy_str(event.type, sizeof(event.type), type);
    copy_str(event.agent_id, sizeof(event.agent_id), agent_id);
    copy_str(event.session_id, sizeof(event.session_id), session_id);
    event.sequence = sequence;
    event.payload_json = payload_json;
    event.payload_len = payload_json != NULL ? strlen(payload_json) : 0;

    uint64_t previous = s_adapter.last_sequence;
    bool old = sequence <= previous;
    if (!old && previous > 0 && sequence > previous + 1) {
        ESP_LOGW(TAG, "sequence gap previous=%llu current=%llu",
                 (unsigned long long)previous,
                 (unsigned long long)sequence);
    }

    if (s_model_lock == NULL) {
        if (payload_json != NULL) {
            cJSON_free(payload_json);
        }
        return;
    }
    if (xSemaphoreTake(s_model_lock, portMAX_DELAY) != pdTRUE) {
        if (payload_json != NULL) {
            cJSON_free(payload_json);
        }
        return;
    }
    esp_err_t apply_err = meet_state_adapter_apply_event(
        &s_adapter, &event, (uint32_t)(esp_timer_get_time() / 1000), &s_model);
    if (payload_json != NULL) {
        cJSON_free(payload_json);
    }
    if (apply_err != ESP_OK) {
        (void)xSemaphoreGive(s_model_lock);
        ESP_LOGW(TAG, "event apply failed type=%s seq=%llu err=%s", type,
                 (unsigned long long)sequence, esp_err_to_name(apply_err));
        return;
    }
    s_last_sequence = s_adapter.last_sequence;

    if (old) {
        (void)xSemaphoreGive(s_model_lock);
        ESP_LOGD(TAG, "old event ignored type=%s seq=%llu adapter_seq=%llu",
                 type, (unsigned long long)sequence,
                 (unsigned long long)s_adapter.last_sequence);
        return;
    }

    bool significant =
        strncmp(type, "session.", 8) == 0 ||
        strncmp(type, "agent.", 6) == 0 ||
        strncmp(type, "permission.", 11) == 0 ||
        strcmp(type, MEET_PROTOCOL_EVENT_MESSAGE_COMPLETED) == 0 ||
        strcmp(type, MEET_PROTOCOL_EVENT_TOOL_FAILED) == 0;
    uint32_t agents = 0;
    uint32_t sessions = 0;
    uint32_t active = 0;
    uint32_t alerts = 0;
    if (significant) {
        s_model_dirty = true;
        post_model_locked();
        agents = espui_view_model_agent_count(&s_model);
        sessions = espui_view_model_session_count(&s_model);
        active = espui_view_model_active_session_count(&s_model);
        alerts = espui_view_model_attention_count(&s_model);
    }
    (void)xSemaphoreGive(s_model_lock);

    if (significant) {
        uint32_t took_us =
            (uint32_t)esp_timer_get_time() - started_us;
        ESP_LOGI(TAG,
                 "event applied type=%s seq=%llu agents=%u sessions=%u "
                 "active=%u alerts=%u took_us=%u",
                 type, (unsigned long long)sequence, (unsigned)agents,
                 (unsigned)sessions, (unsigned)active, (unsigned)alerts,
                 (unsigned)took_us);
    } else {
        ESP_LOGD(TAG, "event observed type=%s seq=%llu", type,
                 (unsigned long long)sequence);
    }
}

static void process_error(cJSON *root)
{
    /* Error envelope: {type:"error", payload:{code, message}}. */
    cJSON *payload = json_item(root, "payload");
    char code[32];
    char message[96];
    json_get_string(payload, "code", code, sizeof(code));
    json_get_string(payload, "message", message, sizeof(message));
    ESP_LOGW(TAG, "server error code=%s message=%s",
             code[0] != '\0' ? code : "unknown",
             message[0] != '\0' ? message : "-");

    bool auth_error = strcmp(code, "auth_failed") == 0 ||
                      strcmp(code, "invalid_pairing_code") == 0 ||
                      strcmp(code, "pairing_expired") == 0 ||
                      strstr(code, "token") != NULL ||
                      strstr(code, "pairing") != NULL;
    if (s_events != NULL) {
        (void)xEventGroupSetBits(s_events,
                                 auth_error ? WS_AUTH_FAILED_BIT
                                            : WS_CLOSED_BIT);
    }
}

static void process_welcome(cJSON *root, uint32_t parse_ms)
{
    cJSON *payload = json_item(root, "payload");
    cJSON *snapshot_obj = json_item(payload, "snapshot");
    if (snapshot_obj == NULL) {
        ESP_LOGE(TAG, "welcome has no snapshot");
        return;
    }

    int raw_bytes = s_summary.bytes;
    int events_skipped = s_summary.events_skipped;
    memset(s_snapshot, 0, sizeof(*s_snapshot));
    memset(&s_summary, 0, sizeof(s_summary));
    s_summary.bytes = raw_bytes;
    s_summary.parse_ms = parse_ms;
    s_summary.events_skipped = events_skipped;

    char token[256];
    char client_id[64];
    json_get_string(payload, "token", token, sizeof(token));
    json_get_string(payload, "client_id", client_id, sizeof(client_id));
    if (client_id[0] != '\0') {
        copy_str(s_client_id, sizeof(s_client_id), client_id);
    }
    if (token[0] != '\0') {
        copy_str(s_token, sizeof(s_token), token);
        esp_err_t auth_err = save_auth(token, client_id);
        if (auth_err != ESP_OK) {
            ESP_LOGW(TAG, "NVS token save failed: %s",
                     esp_err_to_name(auth_err));
        }
    }
    s_summary.token_set = s_token[0] != '\0';

    cJSON *permissions = json_item(payload, "permissions");
    int permissions_count = cJSON_GetArraySize(permissions);
    char permission_list[160] = {0};
    for (int i = 0; i < permissions_count; ++i) {
        cJSON *item = cJSON_GetArrayItem(permissions, i);
        if (cJSON_IsString(item) && item->valuestring != NULL) {
            append_csv(permission_list, sizeof(permission_list),
                       item->valuestring);
        }
    }
    s_summary.permissions = permissions_count;

    cJSON *node_obj = json_item(snapshot_obj, "node");
    if (node_obj == NULL) {
        node_obj = json_item(payload, "node");
    }
    char node_name[32], node_version[32], node_id[64];
    json_get_string(node_obj, "node_id", node_id, sizeof(node_id));
    json_get_string(node_obj, "name", node_name, sizeof(node_name));
    json_get_string(node_obj, "version", node_version, sizeof(node_version));
    copy_str(s_snapshot->node.node_id, sizeof(s_snapshot->node.node_id),
             node_id);
    copy_str(s_snapshot->node.name, sizeof(s_snapshot->node.name), node_name);
    s_snapshot->node.protocol_version =
        (uint32_t)json_get_int(node_obj, "protocol_version", 0);
    s_snapshot->node.status = MEET_NODE_ONLINE;
    s_snapshot->schema_version =
        (uint32_t)json_get_int(root, "schema_version",
                               MEET_PROTOCOL_SCHEMA_VERSION);
    s_snapshot->last_sequence = json_get_u64(snapshot_obj, "last_sequence");
    s_last_sequence = s_snapshot->last_sequence;
    s_summary.sequence = s_snapshot->last_sequence;
    s_summary.replay_truncated = json_get_bool(payload, "replay_truncated");

    cJSON *agents = json_item(snapshot_obj, "agents");
    int total_agents = cJSON_GetArraySize(agents);
    int stored_agents = 0;
    int sessions_total = 0;
    int sessions_stored = 0;
    for (int i = 0; i < total_agents; ++i) {
        cJSON *agent_obj = cJSON_GetArrayItem(agents, i);
        if (!cJSON_IsObject(agent_obj)) {
            continue;
        }
        meet_protocol_agent_t *agent = NULL;
        if (stored_agents < MEET_PROTOCOL_MAX_AGENTS) {
            agent = &s_snapshot->agents[stored_agents];
            char agent_id[40], agent_name[32], agent_status[32];
            json_get_string(agent_obj, "agent_id", agent_id,
                            sizeof(agent_id));
            json_get_string(agent_obj, "name", agent_name,
                            sizeof(agent_name));
            json_get_string(agent_obj, "status", agent_status,
                            sizeof(agent_status));
            copy_str(agent->agent_id, sizeof(agent->agent_id), agent_id);
            copy_str(agent->name, sizeof(agent->name), agent_name);
            agent->status =
                meet_protocol_agent_status_from_string(agent_status);
            agent->session_count = 0;
        }

        cJSON *sessions = json_item(agent_obj, "sessions");
        int session_count = cJSON_GetArraySize(sessions);
        sessions_total += session_count;
        if (agent != NULL) {
            /* Store active sessions first, then completed/other ones. */
            for (int pass = 0; pass < 2; ++pass) {
                for (int j = 0; j < session_count; ++j) {
                    if (sessions_stored >= ESPUI_MAX_SESSIONS ||
                        agent->session_count >= MEET_PROTOCOL_MAX_SESSIONS) {
                        break;
                    }
                    cJSON *session_obj = cJSON_GetArrayItem(sessions, j);
                    if (!cJSON_IsObject(session_obj)) {
                        continue;
                    }
                    char status_text[32] = {0};
                    json_get_string(session_obj, "status", status_text,
                                    sizeof(status_text));
                    meet_session_status_t mapped =
                        meet_protocol_session_status_from_string(status_text);
                    bool active = mapped == MEET_SESSION_RUNNING ||
                                  mapped == MEET_SESSION_WAITING_APPROVAL;
                    if ((pass == 0 && !active) || (pass == 1 && active)) {
                        continue;
                    }

                    meet_protocol_session_t *session =
                        &agent->sessions[agent->session_count];
                    char session_id[40], session_agent[40], title[48];
                    json_get_string(session_obj, "session_id", session_id,
                                    sizeof(session_id));
                    if (session_id[0] == '\0') {
                        continue;
                    }
                    json_get_string(session_obj, "agent_id", session_agent,
                                    sizeof(session_agent));
                    json_get_string(session_obj, "title", title, sizeof(title));
                    copy_str(session->session_id, sizeof(session->session_id),
                             session_id);
                    copy_str(session->agent_id, sizeof(session->agent_id),
                             session_agent[0] != '\0' ? session_agent
                                                      : agent->agent_id);
                    copy_str(session->title, sizeof(session->title), title);
                    session->status = mapped;
                    ++agent->session_count;
                    ++sessions_stored;
                }
            }
            if (agent->agent_id[0] != '\0') {
                ++stored_agents;
            } else {
                memset(agent, 0, sizeof(*agent));
            }
        }
    }
    s_snapshot->agent_count = (uint8_t)stored_agents;
    s_summary.agents = stored_agents;
    s_summary.sessions_total = sessions_total;

    cJSON *pending = json_item(snapshot_obj, "pending_permissions");
    int pending_total = cJSON_GetArraySize(pending);
    int stored_permissions = 0;
    for (int i = 0; i < pending_total &&
                    stored_permissions < MEET_PROTOCOL_MAX_PERMISSIONS;
         ++i) {
        cJSON *permission_obj = cJSON_GetArrayItem(pending, i);
        if (!cJSON_IsObject(permission_obj)) {
            continue;
        }
        meet_protocol_permission_t *permission =
            &s_snapshot->permissions[stored_permissions];
        char permission_id[40], session_id[40], description[48], status[32];
        json_get_string(permission_obj, "permission_id", permission_id,
                        sizeof(permission_id));
        if (permission_id[0] == '\0') {
            continue;
        }
        json_get_string(permission_obj, "session_id", session_id,
                        sizeof(session_id));
        json_get_string(permission_obj, "description", description,
                        sizeof(description));
        json_get_string(permission_obj, "status", status, sizeof(status));
        copy_str(permission->permission_id,
                 sizeof(permission->permission_id), permission_id);
        copy_str(permission->session_id, sizeof(permission->session_id),
                 session_id);
        copy_str(permission->summary, sizeof(permission->summary), description);
        permission->status =
            meet_protocol_permission_status_from_string(status);
        ++stored_permissions;
    }
    s_snapshot->permission_count = (uint8_t)stored_permissions;
    s_summary.pending = pending_total;

    ESP_LOGI(TAG, "welcome node=%s version=%s protocol=%u client_id=%s "
                  "permissions=[%s] replay_truncated=%d",
             node_name, node_version,
             (unsigned)s_snapshot->node.protocol_version, s_client_id,
             permission_list,
             s_summary.replay_truncated ? 1 : 0);

    if (meet_protocol_validate_snapshot(s_snapshot) != ESP_OK) {
        ESP_LOGE(TAG, "snapshot validation failed");
        return;
    }

    uint32_t now_ms = (uint32_t)(esp_timer_get_time() / 1000);
    if (s_model_lock == NULL) {
        ESP_LOGE(TAG, "model lock missing; snapshot not applied");
        return;
    }
    if (xSemaphoreTake(s_model_lock, portMAX_DELAY) != pdTRUE) {
        ESP_LOGE(TAG, "model lock unavailable; snapshot not applied");
        return;
    }
    esp_err_t apply_err = meet_state_adapter_apply_snapshot(
        &s_adapter, s_snapshot, now_ms, &s_model);
    if (apply_err != ESP_OK) {
        (void)xSemaphoreGive(s_model_lock);
        ESP_LOGE(TAG, "snapshot apply failed: %s", esp_err_to_name(apply_err));
        return;
    }

    int vm_sessions = espui_view_model_session_count(&s_model);
    s_summary.sessions_stored = vm_sessions;
    esp_err_t post_err = espui_post_view_model(&s_model);
    if (post_err != ESP_OK) {
        s_model_dirty = true;
        s_last_post_ms = 0;
    } else {
        s_model_dirty = false;
        s_last_post_ms = now_ms;
    }
    (void)xSemaphoreGive(s_model_lock);

    if (!s_mock_stopped) {
        (void)meet_mock_state_stop();
        s_mock_stopped = true;
        ESP_LOGI(TAG, "mock state stopped; real snapshot owns the ViewModel");
    }
    if (post_err != ESP_OK) {
        ESP_LOGW(TAG, "snapshot post failed: %s", esp_err_to_name(post_err));
    }

    save_last_sequence();
    set_state(s_client, MEET_NET_ONLINE);
    if (s_events != NULL) {
        (void)xEventGroupSetBits(s_events, WS_WELCOME_BIT);
    }

    ESP_LOGI(TAG,
             "snapshot applied: agents=%d sessions=%d/%d pending=%d seq=%llu",
             stored_agents, vm_sessions, sessions_total, stored_permissions,
             (unsigned long long)s_snapshot->last_sequence);
}

static void process_message(char *raw, int length)
{
    char type[24];
    if (!json_top_type(raw, type, sizeof(type))) {
        ESP_LOGW(TAG, "message without type, bytes=%d", length);
        return;
    }

    bool welcome = strcmp(type, "welcome") == 0;
    if (welcome) {
        s_summary.bytes = length;
        int events_skipped = json_elide_events(raw);
        if (events_skipped < 0) {
            ESP_LOGE(TAG, "welcome events array malformed; skip parse");
            if (s_events != NULL) {
                (void)xEventGroupSetBits(s_events, WS_CLOSED_BIT);
            }
            return;
        }
        if (length > 20000 && events_skipped == 0) {
            ESP_LOGE(TAG, "large welcome without events elision (%d bytes)",
                     length);
            if (s_events != NULL) {
                (void)xEventGroupSetBits(s_events, WS_CLOSED_BIT);
            }
            return;
        }
        s_summary.events_skipped = events_skipped;
        ESP_LOGI(TAG, "welcome received: raw_bytes=%d events_skipped=%d",
                 length, events_skipped);
    }

    uint64_t start_us = esp_timer_get_time();
    cJSON *root = cJSON_Parse(raw);
    uint32_t parse_ms =
        (uint32_t)((esp_timer_get_time() - start_us) / 1000);
    if (root == NULL) {
        const char *error_ptr = cJSON_GetErrorPtr();
        long offset = error_ptr != NULL ? (long)(error_ptr - raw) : -1;
        ESP_LOGE(TAG, "JSON parse failed type=%s offset=%ld", type, offset);
        if (welcome && s_events != NULL) {
            (void)xEventGroupSetBits(s_events, WS_CLOSED_BIT);
        }
        return;
    }

    if (welcome) {
        s_summary.parse_ms = parse_ms;
        process_welcome(root, parse_ms);
    } else if (strcmp(type, "heartbeat") == 0) {
        process_heartbeat(root);
    } else if (strcmp(type, "event") == 0) {
        process_event(root);
    } else if (strcmp(type, "error") == 0) {
        process_error(root);
    } else {
        ESP_LOGD(TAG, "ignored message type=%s bytes=%d", type, length);
    }

    cJSON_Delete(root);

    if (welcome) {
        ESP_LOGI(TAG,
                 "welcome summary: bytes=%d parse_ms=%u events_skipped=%d "
                 "agents=%d sessions=%d/%d permissions=%d pending=%d "
                 "seq=%llu replay=%d token=%s",
                 s_summary.bytes, (unsigned)s_summary.parse_ms,
                 s_summary.events_skipped, s_summary.agents,
                 s_summary.sessions_stored, s_summary.sessions_total,
                 s_summary.permissions, s_summary.pending,
                 (unsigned long long)s_summary.sequence,
                 s_summary.replay_truncated ? 1 : 0,
                 s_summary.token_set ? "set" : "empty");
        ESP_LOGI(TAG, "welcome memory: internal_free=%u psram_free=%u",
                 (unsigned)heap_caps_get_free_size(MALLOC_CAP_INTERNAL),
                 (unsigned)heap_caps_get_free_size(MALLOC_CAP_SPIRAM));
    }
}

/* ------------------------------------------------------------------------- */
/* WebSocket event handling                                                  */

static void ws_event_handler(void *arg, esp_event_base_t base, int32_t event_id,
                             void *event_data)
{
    (void)arg;
    (void)base;
    if (event_id == WEBSOCKET_EVENT_CONNECTED) {
        ESP_LOGI(TAG, "websocket connected");
        if (s_events != NULL) {
            (void)xEventGroupSetBits(s_events, WS_CONNECTED_BIT);
        }
        return;
    }
    if (event_id == WEBSOCKET_EVENT_DISCONNECTED) {
        ESP_LOGW(TAG, "websocket disconnected");
        if (s_events != NULL) {
            (void)xEventGroupSetBits(s_events, WS_CLOSED_BIT);
        }
        return;
    }
    if (event_id == WEBSOCKET_EVENT_CLOSED) {
        ESP_LOGW(TAG, "websocket closed");
        if (s_events != NULL) {
            (void)xEventGroupSetBits(s_events, WS_CLOSED_BIT);
        }
        return;
    }
    if (event_id == WEBSOCKET_EVENT_ERROR) {
        esp_websocket_event_data_t *data =
            (esp_websocket_event_data_t *)event_data;
        if (data != NULL) {
            ESP_LOGW(TAG, "websocket error type=%d handshake=%d errno=%d",
                     data->error_handle.error_type,
                     data->error_handle.esp_ws_handshake_status_code,
                     data->error_handle.esp_transport_sock_errno);
        } else {
            ESP_LOGW(TAG, "websocket error");
        }
        if (s_events != NULL) {
            (void)xEventGroupSetBits(s_events, WS_CLOSED_BIT);
        }
        return;
    }
    if (event_id != WEBSOCKET_EVENT_DATA) {
        return;
    }

    esp_websocket_event_data_t *data =
        (esp_websocket_event_data_t *)event_data;
    if (data == NULL || data->data_ptr == NULL || data->data_len <= 0) {
        return;
    }
    if (data->op_code != 0x01 && data->op_code != 0x00) {
        return; /* text or continuation only */
    }

    int total = data->payload_len > 0 ? data->payload_len : data->data_len;
    if (total > WS_MAX_MESSAGE - 1 ||
        data->payload_offset < 0 || data->data_len < 0 ||
        data->payload_offset + data->data_len > total) {
        ESP_LOGW(TAG, "websocket message too large/invalid: total=%d offset=%d "
                       "chunk=%d",
                 total, data->payload_offset, data->data_len);
        s_rx_active = false;
        return;
    }
    if (data->payload_offset == 0) {
        s_rx_len = 0;
        s_rx_expected = total;
        s_rx_active = true;
    }
    if (!s_rx_active) {
        return;
    }
    if (data->payload_offset != s_rx_len && data->payload_offset != 0) {
        ESP_LOGW(TAG, "websocket out-of-order fragment offset=%d expected=%d",
                 data->payload_offset, s_rx_len);
        s_rx_active = false;
        return;
    }
    memcpy(s_rx_buf + data->payload_offset, data->data_ptr,
           (size_t)data->data_len);
    s_rx_len = data->payload_offset + data->data_len;
    if (s_rx_len >= s_rx_expected) {
        s_rx_buf[s_rx_len] = '\0';
        s_rx_active = false;
        process_message(s_rx_buf, s_rx_len);
    }
}

/* ------------------------------------------------------------------------- */
/* Connection task                                                           */

static bool build_ws_uri(const char *node_url, char *out, size_t size)
{
    const char *prefix = "http://";
    if (node_url == NULL || strncmp(node_url, prefix, strlen(prefix)) != 0) {
        return false;
    }
    const char *authority = node_url + strlen(prefix);
    const char *slash = strchr(authority, '/');
    size_t authority_len =
        slash != NULL ? (size_t)(slash - authority) : strlen(authority);
    if (authority_len == 0 || authority_len > 120) {
        return false;
    }
    bool has_port = false;
    for (size_t i = 0; i < authority_len; ++i) {
        if (authority[i] == ':') {
            has_port = true;
            break;
        }
    }
    if (has_port) {
        (void)snprintf(out, size, "ws://%.*s/ws", (int)authority_len,
                       authority);
    } else {
        (void)snprintf(out, size, "ws://%.*s:8787/ws", (int)authority_len,
                       authority);
    }
    return true;
}

static bool wait_for_wifi(void)
{
    bool logged = false;
    bool state_set = false;
    while (s_running) {
        meet_wifi_status_t wifi;
        meet_wifi_prov_get_status(&wifi);
        if (wifi.state == MEET_WIFI_STATE_CONNECTED &&
            wifi.node_url[0] != '\0') {
            return true;
        }
        if (!state_set) {
            /* set_state is edge-triggered; call once, not every second. */
            set_state(s_client, MEET_NET_WIFI_CONNECTING);
            state_set = true;
        }
        if (!logged) {
            ESP_LOGI(TAG, "waiting for Wi-Fi connected (state=%s)",
                     meet_wifi_state_name(wifi.state));
            logged = true;
        }
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
    return false;
}

static int next_backoff(int current)
{
    switch (current) {
    case 1:
        return 2;
    case 2:
        return 5;
    case 5:
        return 10;
    case 10:
        return 30;
    default:
        return 30;
    }
}

static void delay_seconds(int seconds)
{
    for (int i = 0; i < seconds && s_running; ++i) {
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}

static void cleanup_ws(void)
{
    if (s_ws != NULL) {
        (void)esp_websocket_client_stop(s_ws);
        (void)esp_websocket_client_destroy(s_ws);
        s_ws = NULL;
    }
}

static void heartbeat_loop(void)
{
    uint32_t last_sent = (uint32_t)(esp_timer_get_time() / 1000);
    s_hb_pending = false;
    s_hb_missed = 0;

    while (s_running) {
        EventBits_t bits = xEventGroupWaitBits(
            s_events, WS_CLOSED_BIT | WS_AUTH_FAILED_BIT, pdFALSE, pdFALSE,
            pdMS_TO_TICKS(250));
        if ((bits & (WS_CLOSED_BIT | WS_AUTH_FAILED_BIT)) != 0) {
            break;
        }
        post_model_if_due();

        meet_wifi_status_t wifi;
        meet_wifi_prov_get_status(&wifi);
        if (wifi.state != MEET_WIFI_STATE_CONNECTED) {
            ESP_LOGW(TAG, "heartbeat: Wi-Fi no longer connected");
            break;
        }

        uint32_t now_ms = (uint32_t)(esp_timer_get_time() / 1000);
        if (now_ms - last_sent < HEARTBEAT_INTERVAL_MS) {
            continue;
        }
        if (s_hb_pending) {
            ++s_hb_missed;
            if (s_hb_missed >= HEARTBEAT_MAX_MISSED) {
                ESP_LOGW(TAG, "heartbeat missed %u times; reconnect",
                         (unsigned)s_hb_missed);
                break;
            }
        }
        if (!send_heartbeat()) {
            break;
        }
        last_sent = now_ms;
    }
}

static void handle_auth_failure(void)
{
    if (!s_used_token) {
        ESP_LOGW(TAG, "pairing code authentication failed");
        return;
    }

    ESP_LOGW(TAG, "token authentication failed; token cleared");
    clear_token();
    if (s_pairing_code[0] != '\0') {
        ESP_LOGW(TAG, "retrying once with stored pairing_code");
    } else {
        ESP_LOGW(TAG, "no pairing_code available; backoff");
    }
}

static void net_task_cleanup(void)
{
    cleanup_ws();
    if (s_rx_buf != NULL) {
        heap_caps_free(s_rx_buf);
        s_rx_buf = NULL;
    }
    if (s_snapshot != NULL) {
        heap_caps_free(s_snapshot);
        s_snapshot = NULL;
    }
    if (s_model_lock != NULL) {
        vSemaphoreDelete(s_model_lock);
        s_model_lock = NULL;
    }
    if (s_lifecycle_lock != NULL) {
        (void)xSemaphoreTake(s_lifecycle_lock, portMAX_DELAY);
    }
    if (s_events != NULL) {
        vEventGroupDelete(s_events);
        s_events = NULL;
    }
    if (s_lifecycle_lock != NULL) {
        (void)xSemaphoreGive(s_lifecycle_lock);
    }
    /* Publish task exit only after dropping s_lifecycle_lock; stop() may then
     * delete the lifecycle mutex. */
    s_task = NULL;
}

static void net_task(void *arg)
{
    meet_net_client_t *client = (meet_net_client_t *)arg;
    s_rx_buf = heap_caps_malloc(WS_MAX_MESSAGE, MALLOC_CAP_SPIRAM);
    s_snapshot = heap_caps_calloc(1, sizeof(*s_snapshot), MALLOC_CAP_SPIRAM);
    if (s_rx_buf == NULL || s_snapshot == NULL) {
        ESP_LOGE(TAG, "failed to allocate PSRAM receive/snapshot buffers");
        set_state(client, MEET_NET_ERROR);
        s_running = false;
        net_task_cleanup();
        vTaskDelete(NULL);
        return;
    }

    meet_state_adapter_init(&s_adapter);
    espui_view_model_init(&s_model);
    load_auth();
    int backoff = 1;

    while (s_running) {
        if (!wait_for_wifi()) {
            break;
        }

        meet_wifi_status_t wifi;
        meet_wifi_prov_get_status(&wifi);
        if (!build_ws_uri(wifi.node_url, s_ws_uri, sizeof(s_ws_uri))) {
            ESP_LOGE(TAG, "unsupported node_url (need http://host:port): %s",
                     wifi.node_url);
            set_state(client, MEET_NET_ERROR);
            delay_seconds(10);
            continue;
        }

        load_auth();
        if (s_token[0] == '\0' && s_pairing_code[0] == '\0') {
            ESP_LOGW(TAG, "no token/pairing_code in NVS; waiting for provisioning");
            set_state(client, MEET_NET_ERROR);
            delay_seconds(10);
            continue;
        }

        (void)xEventGroupClearBits(s_events, WS_CONNECTED_BIT | WS_CLOSED_BIT |
                                                 WS_WELCOME_BIT |
                                                 WS_AUTH_FAILED_BIT);
        set_state(client, MEET_NET_NODE_CONNECTING);
        ESP_LOGI(TAG, "connecting to %s", s_ws_uri);

        esp_websocket_client_config_t config = {
            .uri = s_ws_uri,
            .buffer_size = 1024,
            .disable_auto_reconnect = true,
            .network_timeout_ms = 10000,
            .task_stack = 8192,
        };
        s_ws = esp_websocket_client_init(&config);
        if (s_ws == NULL) {
            ESP_LOGE(TAG, "esp_websocket_client_init failed");
            set_state(client, MEET_NET_ERROR);
            delay_seconds(backoff);
            backoff = next_backoff(backoff);
            continue;
        }
        esp_err_t err = esp_websocket_register_events(
            s_ws, WEBSOCKET_EVENT_ANY, ws_event_handler, NULL);
        if (err != ESP_OK) {
            ESP_LOGE(TAG, "websocket event register failed: %s",
                     esp_err_to_name(err));
            cleanup_ws();
            delay_seconds(backoff);
            backoff = next_backoff(backoff);
            continue;
        }

        err = esp_websocket_client_start(s_ws);
        if (err != ESP_OK) {
            ESP_LOGE(TAG, "websocket start failed: %s", esp_err_to_name(err));
            cleanup_ws();
            delay_seconds(backoff);
            backoff = next_backoff(backoff);
            continue;
        }

        EventBits_t bits = xEventGroupWaitBits(
            s_events, WS_CONNECTED_BIT | WS_CLOSED_BIT, pdFALSE, pdFALSE,
            pdMS_TO_TICKS(CONNECT_TIMEOUT_MS));
        if ((bits & WS_CONNECTED_BIT) == 0) {
            ESP_LOGW(TAG, "websocket connect timeout");
            cleanup_ws();
            delay_seconds(backoff);
            backoff = next_backoff(backoff);
            continue;
        }

        if (!send_hello()) {
            cleanup_ws();
            delay_seconds(backoff);
            backoff = next_backoff(backoff);
            continue;
        }

        (void)xEventGroupClearBits(s_events,
                                   WS_WELCOME_BIT | WS_AUTH_FAILED_BIT |
                                       WS_CLOSED_BIT);
        bits = xEventGroupWaitBits(
            s_events, WS_WELCOME_BIT | WS_AUTH_FAILED_BIT | WS_CLOSED_BIT,
            pdFALSE, pdFALSE, pdMS_TO_TICKS(WELCOME_TIMEOUT_MS));

        if ((bits & WS_WELCOME_BIT) != 0) {
            set_state(client, MEET_NET_ONLINE);
            backoff = 1;
            heartbeat_loop();
        } else if ((bits & WS_AUTH_FAILED_BIT) != 0) {
            bool used_token = s_used_token;
            handle_auth_failure();
            if (used_token && s_pairing_code[0] != '\0') {
                backoff = 1;
            }
        } else {
            ESP_LOGW(TAG, "welcome timeout/closed before welcome");
        }

        save_last_sequence();
        cleanup_ws();
        set_state(client, MEET_NET_DISCONNECTED);
        if (!s_running) {
            break;
        }
        ESP_LOGI(TAG, "reconnect in %d s", backoff);
        delay_seconds(backoff);
        backoff = next_backoff(backoff);
    }

    net_task_cleanup();
    vTaskDelete(NULL);
}

esp_err_t meet_net_client_init(meet_net_client_t *client,
                               const meet_net_config_t *config,
                               const meet_net_callbacks_t *callbacks,
                               void *user_ctx)
{
    if (client == NULL || config == NULL || callbacks == NULL) {
        return ESP_ERR_INVALID_ARG;
    }
    memset(client, 0, sizeof(*client));
    client->config = *config;
    client->callbacks = *callbacks;
    client->user_ctx = user_ctx;
    client->initialized = true;
    client->state = MEET_NET_DISCONNECTED;
    s_client = client;
    return ESP_OK;
}

esp_err_t meet_net_client_start(meet_net_client_t *client)
{
    if (client == NULL || !client->initialized) {
        return ESP_ERR_INVALID_STATE;
    }
    if (s_running) {
        return ESP_ERR_INVALID_STATE;
    }
    if (s_lifecycle_lock == NULL) {
        s_lifecycle_lock = xSemaphoreCreateMutex();
        if (s_lifecycle_lock == NULL) {
            return ESP_ERR_NO_MEM;
        }
    }
    if (s_model_lock == NULL) {
        s_model_lock = xSemaphoreCreateMutex();
        if (s_model_lock == NULL) {
            vSemaphoreDelete(s_lifecycle_lock);
            s_lifecycle_lock = NULL;
            return ESP_ERR_NO_MEM;
        }
    }
    s_events = xEventGroupCreate();
    if (s_events == NULL) {
        vSemaphoreDelete(s_model_lock);
        s_model_lock = NULL;
        vSemaphoreDelete(s_lifecycle_lock);
        s_lifecycle_lock = NULL;
        return ESP_ERR_NO_MEM;
    }
    s_client = client;
    s_running = true;
    if (xTaskCreatePinnedToCore(net_task, "meet_net", 12288, client, 4, &s_task,
                                tskNO_AFFINITY) != pdPASS) {
        s_running = false;
        s_task = NULL;
        vEventGroupDelete(s_events);
        s_events = NULL;
        vSemaphoreDelete(s_model_lock);
        s_model_lock = NULL;
        vSemaphoreDelete(s_lifecycle_lock);
        s_lifecycle_lock = NULL;
        return ESP_ERR_NO_MEM;
    }
    return ESP_OK;
}

esp_err_t meet_net_client_stop(meet_net_client_t *client)
{
    (void)client;
    if (s_task == NULL) {
        s_running = false;
        return ESP_OK;
    }
    if (s_task == xTaskGetCurrentTaskHandle()) {
        return ESP_ERR_INVALID_STATE;
    }

    /* Only request stop + wake waiters. net_task owns WS/buffers/event group. */
    if (s_lifecycle_lock != NULL) {
        (void)xSemaphoreTake(s_lifecycle_lock, portMAX_DELAY);
    }
    s_running = false;
    if (s_events != NULL) {
        (void)xEventGroupSetBits(s_events, WS_CLOSED_BIT);
    }
    if (s_lifecycle_lock != NULL) {
        (void)xSemaphoreGive(s_lifecycle_lock);
    }

    for (int i = 0; i < 20 && s_task != NULL; ++i) {
        vTaskDelay(pdMS_TO_TICKS(100));
    }
    if (s_task != NULL) {
        ESP_LOGW(TAG, "network task did not stop within 2s");
        return ESP_ERR_TIMEOUT;
    }
    if (s_lifecycle_lock != NULL) {
        vSemaphoreDelete(s_lifecycle_lock);
        s_lifecycle_lock = NULL;
    }
    return ESP_OK;
}

esp_err_t meet_net_client_send_action(meet_net_client_t *client,
                                      const espui_action_t *action)
{
    (void)action;
    if (client == NULL || !client->initialized) {
        return ESP_ERR_INVALID_STATE;
    }
    static bool warned;
    if (!warned) {
        ESP_LOGW(TAG, "send_action is not implemented before E4b-3");
        warned = true;
    }
    return ESP_ERR_NOT_SUPPORTED;
}

meet_net_state_t meet_net_client_state(const meet_net_client_t *client)
{
    return client == NULL ? MEET_NET_DISCONNECTED : client->state;
}

const char *meet_net_state_name(meet_net_state_t state)
{
    switch (state) {
    case MEET_NET_DISCONNECTED:
        return "disconnected";
    case MEET_NET_WIFI_CONNECTING:
        return "wifi_connecting";
    case MEET_NET_NODE_CONNECTING:
        return "node_connecting";
    case MEET_NET_ONLINE:
        return "online";
    case MEET_NET_ERROR:
        return "error";
    default:
        return "unknown";
    }
}
