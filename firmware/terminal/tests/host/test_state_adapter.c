#include <string.h>

#include "test_assert.h"
#include "state_adapter.h"

static meet_protocol_snapshot_t make_snapshot(void)
{
    meet_protocol_snapshot_t snapshot;
    memset(&snapshot, 0, sizeof(snapshot));
    snapshot.schema_version = MEET_PROTOCOL_SCHEMA_VERSION;
    snapshot.node.status = MEET_NODE_ONLINE;
    strcpy(snapshot.node.node_id, "node-home");
    strcpy(snapshot.node.name, "Home Node");
    snapshot.node.protocol_version = 1;

    snapshot.agent_count = 1;
    meet_protocol_agent_t *agent = &snapshot.agents[0];
    strcpy(agent->agent_id, "opencode-main");
    strcpy(agent->name, "OpenCode Main");
    agent->status = MEET_AGENT_BUSY;
    agent->session_count = 2;

    strcpy(agent->sessions[0].session_id, "ses-1");
    strcpy(agent->sessions[0].agent_id, "opencode-main");
    strcpy(agent->sessions[0].title, "MeetAgent V2.1");
    agent->sessions[0].status = MEET_SESSION_RUNNING;
    agent->sessions[0].duration_seconds = 42;

    strcpy(agent->sessions[1].session_id, "ses-2");
    strcpy(agent->sessions[1].agent_id, "opencode-main");
    strcpy(agent->sessions[1].title, "Docs");
    agent->sessions[1].status = MEET_SESSION_COMPLETED;

    snapshot.permission_count = 2;
    strcpy(snapshot.permissions[0].permission_id, "per-1");
    strcpy(snapshot.permissions[0].session_id, "ses-1");
    strcpy(snapshot.permissions[0].summary, "read /tmp/file");
    snapshot.permissions[0].status = MEET_PERMISSION_PENDING;

    strcpy(snapshot.permissions[1].permission_id, "per-2");
    strcpy(snapshot.permissions[1].session_id, "ses-1");
    strcpy(snapshot.permissions[1].summary, "already approved");
    snapshot.permissions[1].status = MEET_PERMISSION_APPROVED;

    snapshot.last_sequence = 128;
    return snapshot;
}

static void test_protocol_status_conversion(void)
{
    TEST_ASSERT_STR_EQ("online", meet_protocol_node_status_name(MEET_NODE_ONLINE));
    TEST_ASSERT_STR_EQ("busy", meet_protocol_agent_status_name(MEET_AGENT_BUSY));
    TEST_ASSERT_STR_EQ("running",
                       meet_protocol_session_status_name(MEET_SESSION_RUNNING));
    TEST_ASSERT_STR_EQ("pending",
                       meet_protocol_permission_status_name(MEET_PERMISSION_PENDING));

    TEST_ASSERT_EQ_INT(MEET_NODE_ONLINE,
                       meet_protocol_node_status_from_string("online"));
    TEST_ASSERT_EQ_INT(MEET_NODE_CONNECTING,
                       meet_protocol_node_status_from_string("connecting"));
    TEST_ASSERT_EQ_INT(MEET_NODE_OFFLINE,
                       meet_protocol_node_status_from_string(NULL));

    TEST_ASSERT_EQ_INT(MEET_AGENT_BUSY,
                       meet_protocol_agent_status_from_string("running"));
    TEST_ASSERT_EQ_INT(MEET_AGENT_BUSY,
                       meet_protocol_agent_status_from_string("busy"));
    TEST_ASSERT_EQ_INT(MEET_AGENT_IDLE,
                       meet_protocol_agent_status_from_string("idle"));
    TEST_ASSERT_EQ_INT(MEET_AGENT_ERROR,
                       meet_protocol_agent_status_from_string("error"));
    TEST_ASSERT_EQ_INT(MEET_AGENT_UNKNOWN,
                       meet_protocol_agent_status_from_string("nonsense"));

    TEST_ASSERT_EQ_INT(
        MEET_SESSION_WAITING_APPROVAL,
        meet_protocol_session_status_from_string("waiting_approval"));
    TEST_ASSERT_EQ_INT(MEET_SESSION_COMPLETED,
                       meet_protocol_session_status_from_string("completed"));
    TEST_ASSERT_EQ_INT(MEET_SESSION_FAILED,
                       meet_protocol_session_status_from_string("failed"));
    TEST_ASSERT_EQ_INT(MEET_SESSION_INTERRUPTED,
                       meet_protocol_session_status_from_string("cancelled"));

    TEST_ASSERT_EQ_INT(MEET_PERMISSION_PENDING,
                       meet_protocol_permission_status_from_string("pending"));
    TEST_ASSERT_EQ_INT(MEET_PERMISSION_APPROVED,
                       meet_protocol_permission_status_from_string("approved"));
    TEST_ASSERT_EQ_INT(MEET_PERMISSION_UNKNOWN,
                       meet_protocol_permission_status_from_string("nope"));
}

static void test_snapshot_validation(void)
{
    meet_protocol_snapshot_t snapshot = make_snapshot();
    TEST_ASSERT_EQ_INT(ESP_OK, meet_protocol_validate_snapshot(&snapshot));
    TEST_ASSERT_EQ_INT(ESP_ERR_INVALID_ARG,
                       meet_protocol_validate_snapshot(NULL));

    snapshot = make_snapshot();
    snapshot.schema_version = MEET_PROTOCOL_SCHEMA_VERSION + 1;
    TEST_ASSERT_EQ_INT(ESP_ERR_INVALID_ARG,
                       meet_protocol_validate_snapshot(&snapshot));

    snapshot = make_snapshot();
    snapshot.node.node_id[0] = '\0';
    TEST_ASSERT_EQ_INT(ESP_ERR_INVALID_ARG,
                       meet_protocol_validate_snapshot(&snapshot));

    snapshot = make_snapshot();
    snapshot.agent_count = MEET_PROTOCOL_MAX_AGENTS + 1;
    TEST_ASSERT_EQ_INT(ESP_ERR_INVALID_ARG,
                       meet_protocol_validate_snapshot(&snapshot));

    snapshot = make_snapshot();
    snapshot.agents[0].agent_id[0] = '\0';
    TEST_ASSERT_EQ_INT(ESP_ERR_INVALID_ARG,
                       meet_protocol_validate_snapshot(&snapshot));

    snapshot = make_snapshot();
    snapshot.agents[0].session_count = MEET_PROTOCOL_MAX_SESSIONS + 1;
    TEST_ASSERT_EQ_INT(ESP_ERR_INVALID_ARG,
                       meet_protocol_validate_snapshot(&snapshot));

    snapshot = make_snapshot();
    snapshot.permission_count = MEET_PROTOCOL_MAX_PERMISSIONS + 1;
    TEST_ASSERT_EQ_INT(ESP_ERR_INVALID_ARG,
                       meet_protocol_validate_snapshot(&snapshot));
}

static void test_apply_snapshot(void)
{
    espui_view_model_t model;
    espui_view_model_init(&model);
    model.device.brightness = 55;
    strcpy(model.device.active_theme_id, "terminal_aesthetic");

    meet_state_adapter_t adapter;
    meet_state_adapter_init(&adapter);

    meet_protocol_snapshot_t snapshot = make_snapshot();
    TEST_ASSERT_EQ_INT(ESP_OK, meet_state_adapter_apply_snapshot(
                                   &adapter, &snapshot, 1234, &model));

    TEST_ASSERT_EQ_INT(1, adapter.applied_snapshots);
    TEST_ASSERT_EQ_INT(128, adapter.last_sequence);
    TEST_ASSERT_EQ_INT(ESPUI_NODE_ONLINE, model.node.status);
    TEST_ASSERT_TRUE(model.node.connected);
    TEST_ASSERT_FALSE(model.node.syncing);
    TEST_ASSERT_STR_EQ("node-home", model.node.node_id);
    TEST_ASSERT_STR_EQ("Home Node", model.node.display_name);
    TEST_ASSERT_EQ_INT(1234, model.node.last_sync_ms);

    TEST_ASSERT_EQ_INT(1, espui_view_model_agent_count(&model));
    const espui_agent_view_t *agent =
        espui_view_model_find_agent_const(&model, "opencode-main");
    TEST_ASSERT_NOT_NULL(agent);
    TEST_ASSERT_EQ_INT(ESPUI_AGENT_BUSY, agent->status);
    TEST_ASSERT_STR_EQ("OpenCode Main", agent->display_name);
    TEST_ASSERT_EQ_INT(1, agent->active_session_count);

    TEST_ASSERT_EQ_INT(2, espui_view_model_session_count(&model));
    const espui_session_view_t *session =
        espui_view_model_find_session_const(&model, "ses-1");
    TEST_ASSERT_NOT_NULL(session);
    TEST_ASSERT_EQ_INT(ESPUI_SESSION_RUNNING, session->status);
    TEST_ASSERT_STR_EQ("MeetAgent V2.1", session->title);
    TEST_ASSERT_EQ_INT(42, session->duration_seconds);
    TEST_ASSERT_EQ_INT(1234, session->updated_at_ms);

    TEST_ASSERT_EQ_INT(1, espui_view_model_active_session_count(&model));
    TEST_ASSERT_EQ_INT(1, espui_view_model_attention_count(&model));
    TEST_ASSERT_EQ_INT(1, model.approval_count);
    TEST_ASSERT_STR_EQ("read /tmp/file", model.attention[0].summary);
    TEST_ASSERT_FALSE(model.attention[0].summary_truncated);
    TEST_ASSERT_STR_EQ("ses-1", model.attention[0].session_id);
    TEST_ASSERT_EQ_INT(ESPUI_ATTENTION_APPROVAL, model.attention[0].kind);
    TEST_ASSERT_EQ_INT(1234, model.attention[0].created_at_ms);

    TEST_ASSERT_EQ_INT(55, model.device.brightness);
    TEST_ASSERT_STR_EQ("terminal_aesthetic", model.device.active_theme_id);
}

static void test_apply_snapshot_replaces_state(void)
{
    espui_view_model_t model;
    espui_view_model_init(&model);
    meet_state_adapter_t adapter;
    meet_state_adapter_init(&adapter);

    meet_protocol_snapshot_t snapshot = make_snapshot();
    TEST_ASSERT_EQ_INT(ESP_OK, meet_state_adapter_apply_snapshot(
                                   &adapter, &snapshot, 10, &model));

    meet_protocol_snapshot_t empty;
    memset(&empty, 0, sizeof(empty));
    empty.schema_version = MEET_PROTOCOL_SCHEMA_VERSION;
    empty.node.status = MEET_NODE_ONLINE;
    strcpy(empty.node.node_id, "node-home");
    empty.last_sequence = 129;
    TEST_ASSERT_EQ_INT(ESP_OK, meet_state_adapter_apply_snapshot(
                                   &adapter, &empty, 20, &model));

    TEST_ASSERT_EQ_INT(0, espui_view_model_agent_count(&model));
    TEST_ASSERT_EQ_INT(0, espui_view_model_session_count(&model));
    TEST_ASSERT_EQ_INT(0, espui_view_model_attention_count(&model));
    TEST_ASSERT_EQ_INT(129, adapter.last_sequence);
    TEST_ASSERT_EQ_INT(2, adapter.applied_snapshots);
    TEST_ASSERT_EQ_INT(ESPUI_NODE_ONLINE, model.node.status);
    TEST_ASSERT_EQ_INT(20, model.node.last_sync_ms);
}

static void test_invalid_snapshot_does_not_mutate(void)
{
    espui_view_model_t model;
    espui_view_model_init(&model);
    espui_agent_view_t *agent = espui_view_model_upsert_agent(&model, "keep-me");
    TEST_ASSERT_NOT_NULL(agent);

    meet_state_adapter_t adapter;
    meet_state_adapter_init(&adapter);
    espui_view_model_t before = model;

    meet_protocol_snapshot_t snapshot = make_snapshot();
    snapshot.schema_version = 99;
    TEST_ASSERT_EQ_INT(ESP_ERR_INVALID_ARG,
                       meet_state_adapter_apply_snapshot(&adapter, &snapshot,
                                                         1, &model));
    TEST_ASSERT_EQ_INT(0, memcmp(&before, &model, sizeof(model)));
    TEST_ASSERT_EQ_INT(0, adapter.last_sequence);
    TEST_ASSERT_EQ_INT(0, adapter.applied_snapshots);

    TEST_ASSERT_EQ_INT(ESP_ERR_INVALID_ARG,
                       meet_state_adapter_apply_snapshot(NULL, &snapshot, 1,
                                                         &model));
    snapshot = make_snapshot();
    TEST_ASSERT_EQ_INT(ESP_ERR_INVALID_ARG,
                       meet_state_adapter_apply_snapshot(&adapter, &snapshot, 1,
                                                         NULL));
}

static meet_protocol_event_t make_event(const char *type, uint64_t sequence,
                                        const char *agent_id,
                                        const char *session_id,
                                        const char *payload_json)
{
    meet_protocol_event_t event;
    memset(&event, 0, sizeof(event));
    event.schema_version = MEET_PROTOCOL_SCHEMA_VERSION;
    event.sequence = sequence;
    snprintf(event.type, sizeof(event.type), "%s", type);
    if (agent_id != NULL) {
        snprintf(event.agent_id, sizeof(event.agent_id), "%s", agent_id);
    }
    if (session_id != NULL) {
        snprintf(event.session_id, sizeof(event.session_id), "%s", session_id);
    }
    event.payload_json = payload_json;
    event.payload_len = payload_json != NULL ? strlen(payload_json) : 0;
    return event;
}

static void test_event_agent_status_and_sequence(void)
{
    meet_state_adapter_t adapter;
    meet_state_adapter_init(&adapter);
    espui_view_model_t model;
    espui_view_model_init(&model);

    meet_protocol_event_t event = make_event(
        MEET_PROTOCOL_EVENT_AGENT_STATUS_CHANGED, 100, "agent-1", NULL,
        "{\"status\":\"busy\",\"previous\":\"idle\"}");
    TEST_ASSERT_EQ_INT(ESP_OK, meet_state_adapter_apply_event(&adapter, &event,
                                                              1234, &model));
    TEST_ASSERT_EQ_INT(100, adapter.last_sequence);
    const espui_agent_view_t *agent =
        espui_view_model_find_agent_const(&model, "agent-1");
    TEST_ASSERT_NOT_NULL(agent);
    TEST_ASSERT_EQ_INT(ESPUI_AGENT_BUSY, agent->status);

    uint32_t revision = model.revision;
    event = make_event(MEET_PROTOCOL_EVENT_AGENT_STATUS_CHANGED, 100, "agent-1",
                       NULL, "{\"status\":\"idle\"}");
    TEST_ASSERT_EQ_INT(ESP_OK, meet_state_adapter_apply_event(&adapter, &event,
                                                              1234, &model));
    TEST_ASSERT_EQ_INT(100, adapter.last_sequence);
    TEST_ASSERT_EQ_INT(1, adapter.ignored_events);
    TEST_ASSERT_EQ_INT(revision, model.revision);

    event = make_event(MEET_PROTOCOL_EVENT_AGENT_DISCONNECTED, 101, "agent-1",
                       NULL, "{}");
    TEST_ASSERT_EQ_INT(ESP_OK, meet_state_adapter_apply_event(&adapter, &event,
                                                              1234, &model));
    agent = espui_view_model_find_agent_const(&model, "agent-1");
    TEST_ASSERT_EQ_INT(ESPUI_AGENT_OFFLINE, agent->status);

    event = make_event(MEET_PROTOCOL_EVENT_AGENT_ERROR, 102, "agent-1", NULL,
                       "{\"error\":\"boom\"}");
    TEST_ASSERT_EQ_INT(ESP_OK, meet_state_adapter_apply_event(&adapter, &event,
                                                              1234, &model));
    agent = espui_view_model_find_agent_const(&model, "agent-1");
    TEST_ASSERT_EQ_INT(ESPUI_AGENT_ERROR, agent->status);

    event = make_event(MEET_PROTOCOL_EVENT_AGENT_CONNECTED, 103, "agent-1",
                       NULL, "{}");
    TEST_ASSERT_EQ_INT(ESP_OK, meet_state_adapter_apply_event(&adapter, &event,
                                                              1234, &model));
    agent = espui_view_model_find_agent_const(&model, "agent-1");
    TEST_ASSERT_EQ_INT(ESPUI_AGENT_IDLE, agent->status);
    TEST_ASSERT_EQ_INT(103, adapter.last_sequence);
}

static void test_event_session_lifecycle(void)
{
    meet_state_adapter_t adapter;
    meet_state_adapter_init(&adapter);
    espui_view_model_t model;
    espui_view_model_init(&model);
    TEST_ASSERT_NOT_NULL(espui_view_model_upsert_agent(&model, "agent-1"));

    meet_protocol_event_t event = make_event(
        MEET_PROTOCOL_EVENT_SESSION_CREATED, 200, "agent-1", "ses-1",
        "{\"session_id\":\"ses-1\",\"title\":\"First\","
        "\"status\":\"running\"}");
    TEST_ASSERT_EQ_INT(ESP_OK, meet_state_adapter_apply_event(&adapter, &event,
                                                              500, &model));
    const espui_session_view_t *session =
        espui_view_model_find_session_const(&model, "ses-1");
    TEST_ASSERT_NOT_NULL(session);
    TEST_ASSERT_EQ_INT(ESPUI_SESSION_RUNNING, session->status);
    TEST_ASSERT_STR_EQ("First", session->title);
    TEST_ASSERT_EQ_INT(500, session->updated_at_ms);
    const espui_agent_view_t *agent =
        espui_view_model_find_agent_const(&model, "agent-1");
    TEST_ASSERT_EQ_INT(1, agent->active_session_count);

    event = make_event(MEET_PROTOCOL_EVENT_SESSION_STATUS_CHANGED, 201,
                       "agent-1", "ses-1",
                       "{\"session_id\":\"ses-1\","
                       "\"status\":\"waiting_approval\"}");
    TEST_ASSERT_EQ_INT(ESP_OK, meet_state_adapter_apply_event(&adapter, &event,
                                                              600, &model));
    session = espui_view_model_find_session_const(&model, "ses-1");
    TEST_ASSERT_EQ_INT(ESPUI_SESSION_WAITING_APPROVAL, session->status);
    TEST_ASSERT_EQ_INT(600, session->updated_at_ms);
    agent = espui_view_model_find_agent_const(&model, "agent-1");
    TEST_ASSERT_EQ_INT(1, agent->active_session_count);

    event = make_event(MEET_PROTOCOL_EVENT_SESSION_STATUS_CHANGED, 202,
                       "agent-1", "ses-1",
                       "{\"session_id\":\"ses-1\","
                       "\"status\":\"completed\"}");
    TEST_ASSERT_EQ_INT(ESP_OK, meet_state_adapter_apply_event(&adapter, &event,
                                                              700, &model));
    agent = espui_view_model_find_agent_const(&model, "agent-1");
    TEST_ASSERT_EQ_INT(0, agent->active_session_count);

    event = make_event(MEET_PROTOCOL_EVENT_SESSION_UPDATED, 203, "agent-1",
                       "ses-1",
                       "{\"session_id\":\"ses-1\","
                       "\"title\":\"Renamed\",\"status\":\"running\"}");
    TEST_ASSERT_EQ_INT(ESP_OK, meet_state_adapter_apply_event(&adapter, &event,
                                                              800, &model));
    session = espui_view_model_find_session_const(&model, "ses-1");
    TEST_ASSERT_STR_EQ("Renamed", session->title);
    TEST_ASSERT_EQ_INT(ESPUI_SESSION_RUNNING, session->status);
    agent = espui_view_model_find_agent_const(&model, "agent-1");
    TEST_ASSERT_EQ_INT(1, agent->active_session_count);

    event = make_event(MEET_PROTOCOL_EVENT_SESSION_DELETED, 204, "agent-1",
                       "ses-1",
                       "{\"session_id\":\"ses-1\",\"status\":\"deleted\"}");
    TEST_ASSERT_EQ_INT(ESP_OK, meet_state_adapter_apply_event(&adapter, &event,
                                                              900, &model));
    TEST_ASSERT_NULL(espui_view_model_find_session_const(&model, "ses-1"));
    agent = espui_view_model_find_agent_const(&model, "agent-1");
    TEST_ASSERT_EQ_INT(0, agent->active_session_count);

    /* A late replay of the deleted session must not resurrect it. */
    event = make_event(MEET_PROTOCOL_EVENT_SESSION_CREATED, 203, "agent-1",
                       "ses-1",
                       "{\"status\":\"running\",\"title\":\"Old\"}");
    TEST_ASSERT_EQ_INT(ESP_OK, meet_state_adapter_apply_event(&adapter, &event,
                                                              1000, &model));
    TEST_ASSERT_NULL(espui_view_model_find_session_const(&model, "ses-1"));
    TEST_ASSERT_EQ_INT(204, adapter.last_sequence);
}

static void test_event_permission_and_alerts(void)
{
    meet_state_adapter_t adapter;
    meet_state_adapter_init(&adapter);
    espui_view_model_t model;
    espui_view_model_init(&model);

    meet_protocol_event_t event = make_event(
        MEET_PROTOCOL_EVENT_PERMISSION_REQUESTED, 300, "agent-1", "ses-1",
        "{\"permission_id\":\"per-1\","
        "\"description\":\"read \\\"file\\\"\","
        "\"action\":\"read\",\"status\":\"pending\"}");
    TEST_ASSERT_EQ_INT(ESP_OK, meet_state_adapter_apply_event(&adapter, &event,
                                                              1100, &model));
    TEST_ASSERT_EQ_INT(1, model.approval_count);
    TEST_ASSERT_EQ_INT(1, espui_view_model_attention_count(&model));
    const espui_attention_view_t *attention = &model.attention[0];
    TEST_ASSERT_STR_EQ("per-1", attention->permission_id);
    TEST_ASSERT_STR_EQ("read \"file\"", attention->summary);
    TEST_ASSERT_STR_EQ("ses-1", attention->session_id);
    TEST_ASSERT_EQ_INT(1100, attention->created_at_ms);

    event = make_event(MEET_PROTOCOL_EVENT_PERMISSION_RESOLVED, 301, "agent-1",
                       "ses-1",
                       "{\"permission_id\":\"per-1\","
                       "\"status\":\"approved\",\"decision\":\"once\"}");
    TEST_ASSERT_EQ_INT(ESP_OK, meet_state_adapter_apply_event(&adapter, &event,
                                                              1200, &model));
    TEST_ASSERT_EQ_INT(0, model.approval_count);
    TEST_ASSERT_EQ_INT(0, espui_view_model_attention_count(&model));

    event = make_event(MEET_PROTOCOL_EVENT_TOOL_FAILED, 302, "agent-1", NULL,
                       "{\"call_id\":\"c1\",\"status\":\"failed\"}");
    TEST_ASSERT_EQ_INT(ESP_OK, meet_state_adapter_apply_event(&adapter, &event,
                                                              1300, &model));
    TEST_ASSERT_EQ_INT(1, model.error_count);
    event.sequence = 303;
    TEST_ASSERT_EQ_INT(ESP_OK, meet_state_adapter_apply_event(&adapter, &event,
                                                              1300, &model));
    TEST_ASSERT_EQ_INT(2, model.error_count);

    event = make_event(MEET_PROTOCOL_EVENT_MESSAGE_COMPLETED, 304, "agent-1",
                       NULL, "{}");
    TEST_ASSERT_EQ_INT(ESP_OK, meet_state_adapter_apply_event(&adapter, &event,
                                                              1400, &model));
    TEST_ASSERT_EQ_INT(1, model.completion_count);
    event.sequence = 305;
    TEST_ASSERT_EQ_INT(ESP_OK, meet_state_adapter_apply_event(&adapter, &event,
                                                              1400, &model));
    TEST_ASSERT_EQ_INT(2, model.completion_count);
}

static const espui_attention_view_t *find_attention(
    const espui_view_model_t *model, const char *permission_id)
{
    for (size_t i = 0; i < ESPUI_MAX_ATTENTION; ++i) {
        if (model->attention[i].in_use &&
            strcmp(model->attention[i].permission_id, permission_id) == 0) {
            return &model->attention[i];
        }
    }
    return NULL;
}

static void test_summary_truncation(void)
{
    meet_state_adapter_t adapter;
    meet_state_adapter_init(&adapter);
    espui_view_model_t model;
    espui_view_model_init(&model);

    /* Snapshot: a full 48-byte protocol field is conservatively unsafe. */
    meet_protocol_snapshot_t snapshot = make_snapshot();
    memset(snapshot.permissions[0].summary, 'x',
           sizeof(snapshot.permissions[0].summary) - 1);
    snapshot.permissions[0].summary[sizeof(snapshot.permissions[0].summary) - 1] =
        '\0';
    TEST_ASSERT_EQ_INT(ESP_OK, meet_state_adapter_apply_snapshot(
                                   &adapter, &snapshot, 100, &model));
    const espui_attention_view_t *attention =
        find_attention(&model, "per-1");
    TEST_ASSERT_NOT_NULL(attention);
    TEST_ASSERT_TRUE(attention->summary_truncated);

    /* Event extraction keeps a longer local buffer, so >=48 is detectable. */
    meet_state_adapter_init(&adapter);
    espui_view_model_init(&model);
    char long_summary[80];
    memset(long_summary, 'y', sizeof(long_summary) - 1);
    long_summary[sizeof(long_summary) - 1] = '\0';
    char payload[256];
    snprintf(payload, sizeof(payload),
             "{\"permission_id\":\"per-1\",\"description\":\"%s\"}",
             long_summary);
    meet_protocol_event_t event = make_event(
        MEET_PROTOCOL_EVENT_PERMISSION_REQUESTED, 600, "agent-1", "ses-1",
        payload);
    TEST_ASSERT_EQ_INT(ESP_OK, meet_state_adapter_apply_event(&adapter, &event,
                                                              1100, &model));
    attention = find_attention(&model, "per-1");
    TEST_ASSERT_NOT_NULL(attention);
    TEST_ASSERT_TRUE(attention->summary_truncated);
    TEST_ASSERT_EQ_INT(ESPUI_TITLE_MAX - 1, strlen(attention->summary));

    /* Re-pushing the same permission clears the stale truncation flag. */
    TEST_ASSERT_NOT_NULL(espui_view_model_push_attention(
        &model, "per-1", ESPUI_ATTENTION_APPROVAL));
    attention = find_attention(&model, "per-1");
    TEST_ASSERT_NOT_NULL(attention);
    TEST_ASSERT_FALSE(attention->summary_truncated);

    /* Short event summaries stay actionable. */
    event = make_event(MEET_PROTOCOL_EVENT_PERMISSION_REQUESTED, 601,
                       "agent-1", "ses-1",
                       "{\"permission_id\":\"per-2\","
                       "\"description\":\"read .env\"}");
    TEST_ASSERT_EQ_INT(ESP_OK, meet_state_adapter_apply_event(&adapter, &event,
                                                              1200, &model));
    attention = find_attention(&model, "per-2");
    TEST_ASSERT_NOT_NULL(attention);
    TEST_ASSERT_FALSE(attention->summary_truncated);
    TEST_ASSERT_STR_EQ("read .env", attention->summary);
}

static void test_event_alert_cap_and_unknown(void)
{
    meet_state_adapter_t adapter;
    meet_state_adapter_init(&adapter);
    espui_view_model_t model;
    espui_view_model_init(&model);

    for (uint64_t seq = 400; seq < 400 + ESPUI_ALERT_COUNT_MAX + 5; ++seq) {
        meet_protocol_event_t event = make_event(
            MEET_PROTOCOL_EVENT_TOOL_FAILED, seq, "agent-1", NULL, "{}");
        TEST_ASSERT_EQ_INT(ESP_OK, meet_state_adapter_apply_event(
                                       &adapter, &event, 1500, &model));
    }
    TEST_ASSERT_EQ_INT(ESPUI_ALERT_COUNT_MAX, model.error_count);

    uint64_t last = adapter.last_sequence;
    espui_view_model_t before = model;
    meet_protocol_event_t event = make_event(
        MEET_PROTOCOL_EVENT_MESSAGE_DELTA, last + 1, "agent-1", "ses-1",
        "{\"delta\":\"sensitive text\"}");
    TEST_ASSERT_EQ_INT(ESP_OK, meet_state_adapter_apply_event(&adapter, &event,
                                                              1600, &model));
    TEST_ASSERT_EQ_INT(last + 1, adapter.last_sequence);
    /* Unknown/stream events advance sequence without changing ViewModel. */
    TEST_ASSERT_EQ_INT(0, memcmp(&before, &model, sizeof(model)));

    event.schema_version = 99;
    event.sequence = last + 2;
    TEST_ASSERT_EQ_INT(ESP_ERR_INVALID_ARG,
                       meet_state_adapter_apply_event(&adapter, &event, 1700,
                                                      &model));
    TEST_ASSERT_EQ_INT(last + 1, adapter.last_sequence);

    TEST_ASSERT_EQ_INT(ESP_ERR_INVALID_ARG,
                       meet_state_adapter_apply_event(NULL, &event, 1, &model));
    TEST_ASSERT_EQ_INT(ESP_ERR_INVALID_ARG,
                       meet_state_adapter_apply_event(&adapter, NULL, 1, &model));
    TEST_ASSERT_EQ_INT(ESP_ERR_INVALID_ARG,
                       meet_state_adapter_apply_event(&adapter, &event, 1, NULL));
}

static void test_event_oversized_ids_are_dropped(void)
{
    meet_state_adapter_t adapter;
    meet_state_adapter_init(&adapter);
    espui_view_model_t model;
    espui_view_model_init(&model);

    char long_id[65];
    memset(long_id, 'A', sizeof(long_id) - 1);
    long_id[sizeof(long_id) - 1] = '\0';

    char payload[256];
    snprintf(payload, sizeof(payload),
             "{\"agent_id\":\"%s\",\"status\":\"busy\"}", long_id);
    meet_protocol_event_t event = make_event(
        MEET_PROTOCOL_EVENT_AGENT_CONNECTED, 500, NULL, NULL, payload);
    TEST_ASSERT_EQ_INT(ESP_OK, meet_state_adapter_apply_event(&adapter, &event,
                                                              1000, &model));
    TEST_ASSERT_EQ_INT(500, adapter.last_sequence);
    TEST_ASSERT_EQ_INT(0, espui_view_model_agent_count(&model));

    snprintf(payload, sizeof(payload),
             "{\"session_id\":\"%s\",\"title\":\"too long\","
             "\"status\":\"running\"}",
             long_id);
    event = make_event(MEET_PROTOCOL_EVENT_SESSION_CREATED, 501, NULL, NULL,
                       payload);
    TEST_ASSERT_EQ_INT(ESP_OK, meet_state_adapter_apply_event(&adapter, &event,
                                                              1001, &model));
    TEST_ASSERT_EQ_INT(501, adapter.last_sequence);
    TEST_ASSERT_EQ_INT(0, espui_view_model_session_count(&model));

    snprintf(payload, sizeof(payload),
             "{\"permission_id\":\"per-1\",\"session_id\":\"%s\","
             "\"description\":\"too long\"}",
             long_id);
    event = make_event(MEET_PROTOCOL_EVENT_PERMISSION_REQUESTED, 502, NULL,
                       NULL, payload);
    TEST_ASSERT_EQ_INT(ESP_OK, meet_state_adapter_apply_event(&adapter, &event,
                                                              1002, &model));
    TEST_ASSERT_EQ_INT(502, adapter.last_sequence);
    TEST_ASSERT_EQ_INT(0, espui_view_model_attention_count(&model));

    /* 39 chars + NUL fits ESPUI_ID_MAX (40); boundary must still work. */
    char id39[40];
    memset(id39, 'B', sizeof(id39) - 1);
    id39[sizeof(id39) - 1] = '\0';
    snprintf(payload, sizeof(payload),
             "{\"agent_id\":\"%s\",\"status\":\"idle\"}", id39);
    event = make_event(MEET_PROTOCOL_EVENT_AGENT_CONNECTED, 503, NULL, NULL,
                       payload);
    TEST_ASSERT_EQ_INT(ESP_OK, meet_state_adapter_apply_event(&adapter, &event,
                                                              1003, &model));
    TEST_ASSERT_NOT_NULL(espui_view_model_find_agent_const(&model, id39));
}

void test_state_adapter_run(void)
{
    test_protocol_status_conversion();
    test_snapshot_validation();
    test_apply_snapshot();
    test_apply_snapshot_replaces_state();
    test_invalid_snapshot_does_not_mutate();
    test_event_agent_status_and_sequence();
    test_event_session_lifecycle();
    test_event_permission_and_alerts();
    test_event_alert_cap_and_unknown();
    test_summary_truncation();
    test_event_oversized_ids_are_dropped();
}
