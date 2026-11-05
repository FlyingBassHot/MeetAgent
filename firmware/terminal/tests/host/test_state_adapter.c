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

static void test_apply_event_is_stubbed(void)
{
    meet_state_adapter_t adapter;
    meet_state_adapter_init(&adapter);
    espui_view_model_t model;
    espui_view_model_init(&model);

    meet_protocol_event_t event;
    memset(&event, 0, sizeof(event));
    event.schema_version = MEET_PROTOCOL_SCHEMA_VERSION;
    event.sequence = 200;

    TEST_ASSERT_EQ_INT(ESP_ERR_NOT_SUPPORTED,
                       meet_state_adapter_apply_event(&adapter, &event, 1,
                                                      &model));
    TEST_ASSERT_EQ_INT(0, adapter.last_sequence);
}

void test_state_adapter_run(void)
{
    test_protocol_status_conversion();
    test_snapshot_validation();
    test_apply_snapshot();
    test_apply_snapshot_replaces_state();
    test_invalid_snapshot_does_not_mutate();
    test_apply_event_is_stubbed();
}
