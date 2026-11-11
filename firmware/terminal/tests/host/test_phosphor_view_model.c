#include <string.h>

#include "espui_view_model.h"
#include "phosphor_input_publisher.h"
#include "phosphor_state.h"
#include "phosphor_view_model_adapter.h"
#include "test_assert.h"

static espui_session_view_t *add_session(espui_view_model_t *model,
                                         const char *id,
                                         espui_session_status_t status)
{
    espui_session_view_t *session =
        espui_view_model_upsert_session(model, id);
    if (session != NULL) {
        session->status = status;
    }
    return session;
}

static void test_node_and_connection_mapping(void)
{
    espui_view_model_t model;
    espui_view_model_init(&model);

    model.node.status = ESPUI_NODE_ONLINE;
    phosphor_input_state_t input =
        phosphor_view_model_to_input(&model, 0, NULL);
    TEST_ASSERT_TRUE(input.node_online);

    model.node.status = ESPUI_NODE_SYNCING;
    input = phosphor_view_model_to_input(&model, 0, NULL);
    TEST_ASSERT_TRUE(input.node_online);

    model.node.status = ESPUI_NODE_CONNECTING;
    input = phosphor_view_model_to_input(&model, 0, NULL);
    TEST_ASSERT_FALSE(input.node_online);

    phosphor_connection_input_t connection = {
        .valid = true,
        .state = PHOSPHOR_CONNECTION_ERROR,
    };
    model.node.status = ESPUI_NODE_ONLINE;
    input = phosphor_view_model_to_input(&model, 0, &connection);
    TEST_ASSERT_FALSE(input.node_online);

    connection.state = PHOSPHOR_CONNECTION_OFFLINE;
    input = phosphor_view_model_to_input(&model, 0, &connection);
    TEST_ASSERT_FALSE(input.node_online);

    connection.state = PHOSPHOR_CONNECTION_CONNECTING;
    input = phosphor_view_model_to_input(&model, 0, &connection);
    TEST_ASSERT_FALSE(input.node_online);

    model.node.status = ESPUI_NODE_OFFLINE;
    connection.state = PHOSPHOR_CONNECTION_ONLINE;
    input = phosphor_view_model_to_input(&model, 0, &connection);
    TEST_ASSERT_TRUE(input.node_online);
}

static void test_session_mapping(void)
{
    espui_view_model_t model;
    espui_view_model_init(&model);
    model.node.status = ESPUI_NODE_ONLINE;

    add_session(&model, "run", ESPUI_SESSION_RUNNING);
    add_session(&model, "wait", ESPUI_SESSION_WAITING_APPROVAL);
    add_session(&model, "err", ESPUI_SESSION_FAILED);
    add_session(&model, "done", ESPUI_SESSION_COMPLETED);
    add_session(&model, "stop", ESPUI_SESSION_INTERRUPTED);
    add_session(&model, "unknown", ESPUI_SESSION_UNKNOWN);

    phosphor_input_state_t input =
        phosphor_view_model_to_input(&model, 0, NULL);
    TEST_ASSERT_EQ_INT(6, input.session_count);
    TEST_ASSERT_STR_EQ("run", input.sessions[0].session_id);
    TEST_ASSERT_EQ_INT(PHOSPHOR_SESSION_RUNNING, input.sessions[0].status);
    TEST_ASSERT_STR_EQ("wait", input.sessions[1].session_id);
    TEST_ASSERT_EQ_INT(PHOSPHOR_SESSION_WAITING, input.sessions[1].status);
    TEST_ASSERT_STR_EQ("err", input.sessions[2].session_id);
    TEST_ASSERT_EQ_INT(PHOSPHOR_SESSION_ERROR, input.sessions[2].status);
    TEST_ASSERT_STR_EQ("done", input.sessions[3].session_id);
    TEST_ASSERT_EQ_INT(PHOSPHOR_SESSION_COMPLETED, input.sessions[3].status);
    TEST_ASSERT_EQ_INT(PHOSPHOR_SESSION_IDLE, input.sessions[4].status);
    TEST_ASSERT_EQ_INT(PHOSPHOR_SESSION_IDLE, input.sessions[5].status);

    /* The resolver turns the first four sessions into satellites. */
    phosphor_state_resolver_t resolver;
    phosphor_state_resolver_init(&resolver);
    phosphor_visual_state_t visual;
    phosphor_state_resolver_update(&resolver, &input, 0, &visual);
    TEST_ASSERT_EQ_INT(1, visual.running_count);
    TEST_ASSERT_EQ_INT(1, visual.waiting_count);
    TEST_ASSERT_EQ_INT(1, visual.error_count);
    TEST_ASSERT_EQ_INT(1, visual.completed_count);
    TEST_ASSERT_EQ_INT(4, visual.satellite_count);
    TEST_ASSERT_EQ_INT(2, visual.overflow_count);
}

static void test_attention_mapping(void)
{
    espui_view_model_t model;
    espui_view_model_init(&model);
    model.node.status = ESPUI_NODE_ONLINE;
    add_session(&model, "wait", ESPUI_SESSION_WAITING_APPROVAL);

    TEST_ASSERT_NOT_NULL(espui_view_model_push_attention(
        &model, "per-ok", ESPUI_ATTENTION_APPROVAL));
    TEST_ASSERT_NOT_NULL(espui_view_model_push_attention(
        &model, "per-err", ESPUI_ATTENTION_ERROR));
    espui_view_model_recount_attention(&model);

    phosphor_input_state_t input =
        phosphor_view_model_to_input(&model, 0, NULL);
    TEST_ASSERT_EQ_INT(1, input.approval_count);
    TEST_ASSERT_EQ_INT(1, input.error_count);

    /* Transient error counter is included in error_count. */
    espui_view_model_note_error(&model);
    input = phosphor_view_model_to_input(&model, 0, NULL);
    TEST_ASSERT_EQ_INT(2, input.error_count);
}

static void test_completion_hash_and_dedupe(void)
{
    TEST_ASSERT_EQ_INT(0, phosphor_completion_hash(NULL, 0));
    uint64_t first = phosphor_completion_hash("evt-1", 7);
    TEST_ASSERT_TRUE(first != 0);
    TEST_ASSERT_EQ_INT(first, phosphor_completion_hash("evt-1", 7));
    TEST_ASSERT_TRUE(first != phosphor_completion_hash("evt-2", 7));
    TEST_ASSERT_TRUE(0 != phosphor_completion_hash("", 8));

    espui_view_model_t model;
    espui_view_model_init(&model);
    model.node.status = ESPUI_NODE_ONLINE;

    phosphor_input_state_t input =
        phosphor_view_model_to_input(&model, first, NULL);
    TEST_ASSERT_EQ_INT(first, input.completed_event_id);

    phosphor_state_resolver_t resolver;
    phosphor_state_resolver_init(&resolver);
    phosphor_visual_state_t visual;
    phosphor_state_resolver_update(&resolver, &input, 0, &visual);
    TEST_ASSERT_TRUE(visual.completion_active);

    /* Same id must not restart the pulse. */
    phosphor_state_resolver_update(&resolver, &input, 100, &visual);
    TEST_ASSERT_TRUE(visual.completion_active);
    phosphor_state_resolver_update(&resolver, &input,
                                   PHOSPHOR_COMPLETION_MS, &visual);
    TEST_ASSERT_FALSE(visual.completion_active);

    input.completed_event_id = phosphor_completion_hash("evt-2", 9);
    phosphor_state_resolver_update(&resolver, &input,
                                   PHOSPHOR_COMPLETION_MS, &visual);
    TEST_ASSERT_TRUE(visual.completion_active);
}

static void test_offline_has_no_sessions(void)
{
    espui_view_model_t model;
    espui_view_model_init(&model);
    model.node.status = ESPUI_NODE_OFFLINE;
    add_session(&model, "run", ESPUI_SESSION_RUNNING);

    phosphor_input_state_t input =
        phosphor_view_model_to_input(&model, 0, NULL);
    TEST_ASSERT_FALSE(input.node_online);
    TEST_ASSERT_EQ_INT(1, input.session_count);

    phosphor_state_resolver_t resolver;
    phosphor_state_resolver_init(&resolver);
    phosphor_visual_state_t visual;
    phosphor_state_resolver_update(&resolver, &input, 0, &visual);
    TEST_ASSERT_EQ_INT(PHOSPHOR_ACTIVITY_OFFLINE, visual.activity);
    TEST_ASSERT_EQ_INT(0, visual.attention);
    TEST_ASSERT_EQ_INT(0, visual.satellite_count);
}

static void test_publisher_latches_completion(void)
{
    phosphor_input_publisher_clear();
    phosphor_input_publisher_init();

    phosphor_input_state_t input;
    memset(&input, 0, sizeof(input));
    input.node_online = true;
    input.completed_event_id = 111;
    TEST_ASSERT_TRUE(phosphor_input_publisher_publish(&input, true));

    phosphor_input_state_t out;
    bool real = false;
    TEST_ASSERT_TRUE(phosphor_input_publisher_read(&out, &real));
    TEST_ASSERT_TRUE(real);
    TEST_ASSERT_EQ_INT(111, out.completed_event_id);

    /* An ordinary event with hash 0 keeps the pulse visible. */
    input.completed_event_id = 0;
    TEST_ASSERT_TRUE(phosphor_input_publisher_publish(&input, false));
    TEST_ASSERT_TRUE(phosphor_input_publisher_read(&out, &real));
    TEST_ASSERT_TRUE(real);
    TEST_ASSERT_EQ_INT(111, out.completed_event_id);

    /* A new completion replaces the latched id. */
    input.completed_event_id = 222;
    TEST_ASSERT_TRUE(phosphor_input_publisher_publish(&input, false));
    TEST_ASSERT_TRUE(phosphor_input_publisher_read(&out, &real));
    TEST_ASSERT_EQ_INT(222, out.completed_event_id);

    phosphor_input_publisher_clear();
    TEST_ASSERT_FALSE(phosphor_input_publisher_read(&out, &real));
    TEST_ASSERT_FALSE(real);
}

void test_phosphor_view_model_run(void)
{
    test_node_and_connection_mapping();
    test_session_mapping();
    test_attention_mapping();
    test_completion_hash_and_dedupe();
    test_offline_has_no_sessions();
    test_publisher_latches_completion();
}
