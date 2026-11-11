#include <string.h>

#include "test_assert.h"
#include "espui_scene.h"
#include "espui_view_model.h"

static void test_view_model_init_and_reset(void)
{
    espui_view_model_t model;

    espui_view_model_init(&model);
    TEST_ASSERT_EQ_INT(1, model.revision);
    TEST_ASSERT_EQ_INT(ESPUI_NODE_OFFLINE, model.node.status);
    TEST_ASSERT_FALSE(model.node.connected);
    TEST_ASSERT_EQ_INT(70, model.device.brightness);
    TEST_ASSERT_TRUE(model.device.motion_enabled);
    TEST_ASSERT_EQ_INT(100, model.device.animation_speed_percent);
    TEST_ASSERT_STR_EQ("safe_fallback", model.device.active_theme_id);
    TEST_ASSERT_EQ_INT(0, espui_view_model_agent_count(&model));

    espui_agent_view_t *agent =
        espui_view_model_upsert_agent(&model, "agent-a");
    TEST_ASSERT_NOT_NULL(agent);
    agent->status = ESPUI_AGENT_BUSY;
    strcpy(agent->display_name, "Agent A");

    espui_session_view_t *session =
        espui_view_model_upsert_session(&model, "ses-a");
    TEST_ASSERT_NOT_NULL(session);
    session->status = ESPUI_SESSION_RUNNING;

    espui_attention_view_t *attention = espui_view_model_push_attention(
        &model, "per-a", ESPUI_ATTENTION_APPROVAL);
    TEST_ASSERT_NOT_NULL(attention);

    uint32_t revision_before = model.revision;
    model.device.brightness = 42;
    model.device.motion_enabled = false;
    strcpy(model.device.active_theme_id, "terminal_aesthetic");

    espui_view_model_reset(&model, true);
    TEST_ASSERT_TRUE(model.revision > revision_before);
    TEST_ASSERT_EQ_INT(ESPUI_NODE_OFFLINE, model.node.status);
    TEST_ASSERT_EQ_INT(0, espui_view_model_agent_count(&model));
    TEST_ASSERT_EQ_INT(0, espui_view_model_session_count(&model));
    TEST_ASSERT_EQ_INT(0, espui_view_model_attention_count(&model));
    TEST_ASSERT_EQ_INT(42, model.device.brightness);
    TEST_ASSERT_FALSE(model.device.motion_enabled);
    TEST_ASSERT_STR_EQ("terminal_aesthetic", model.device.active_theme_id);

    espui_view_model_reset(&model, false);
    TEST_ASSERT_EQ_INT(70, model.device.brightness);
    TEST_ASSERT_STR_EQ("safe_fallback", model.device.active_theme_id);
}

static void test_view_model_agents(void)
{
    espui_view_model_t model;
    espui_view_model_init(&model);

    char id[ESPUI_ID_MAX];
    for (int i = 0; i < ESPUI_MAX_AGENTS; ++i) {
        snprintf(id, sizeof(id), "agent-%d", i);
        espui_agent_view_t *agent = espui_view_model_upsert_agent(&model, id);
        TEST_ASSERT_NOT_NULL(agent);
        TEST_ASSERT_TRUE(agent->in_use);
        TEST_ASSERT_STR_EQ(id, agent->agent_id);
        agent->active_session_count = (uint16_t)(i + 1);
    }
    TEST_ASSERT_EQ_INT(ESPUI_MAX_AGENTS, espui_view_model_agent_count(&model));
    TEST_ASSERT_NULL(espui_view_model_upsert_agent(&model, "overflow"));

    espui_agent_view_t *again =
        espui_view_model_upsert_agent(&model, "agent-3");
    TEST_ASSERT_NOT_NULL(again);
    TEST_ASSERT_EQ_INT(4, again->active_session_count);
    TEST_ASSERT_EQ_INT(ESPUI_MAX_AGENTS, espui_view_model_agent_count(&model));

    TEST_ASSERT_NOT_NULL(espui_view_model_find_agent(&model, "agent-3"));
    TEST_ASSERT_NOT_NULL(espui_view_model_find_agent_const(&model, "agent-3"));
    TEST_ASSERT_NULL(espui_view_model_find_agent(&model, "missing"));

    TEST_ASSERT_TRUE(espui_view_model_remove_agent(&model, "agent-3"));
    TEST_ASSERT_FALSE(espui_view_model_remove_agent(&model, "agent-3"));
    TEST_ASSERT_NULL(espui_view_model_find_agent(&model, "agent-3"));
    TEST_ASSERT_EQ_INT(ESPUI_MAX_AGENTS - 1,
                       espui_view_model_agent_count(&model));

    TEST_ASSERT_FALSE(espui_view_model_remove_agent(NULL, "agent-0"));
}

static void test_view_model_sessions(void)
{
    espui_view_model_t model;
    espui_view_model_init(&model);

    espui_session_view_t *running =
        espui_view_model_upsert_session(&model, "ses-running");
    espui_session_view_t *waiting =
        espui_view_model_upsert_session(&model, "ses-waiting");
    espui_session_view_t *done =
        espui_view_model_upsert_session(&model, "ses-done");
    TEST_ASSERT_NOT_NULL(running);
    TEST_ASSERT_NOT_NULL(waiting);
    TEST_ASSERT_NOT_NULL(done);

    running->status = ESPUI_SESSION_RUNNING;
    waiting->status = ESPUI_SESSION_WAITING_APPROVAL;
    done->status = ESPUI_SESSION_COMPLETED;
    TEST_ASSERT_EQ_INT(2, espui_view_model_active_session_count(&model));

    strcpy(waiting->title, "Waiting for approval");
    espui_session_view_t *again =
        espui_view_model_upsert_session(&model, "ses-waiting");
    TEST_ASSERT_NOT_NULL(again);
    TEST_ASSERT_STR_EQ("Waiting for approval", again->title);
    TEST_ASSERT_EQ_INT(3, espui_view_model_session_count(&model));

    TEST_ASSERT_TRUE(espui_view_model_remove_session(&model, "ses-running"));
    TEST_ASSERT_EQ_INT(1, espui_view_model_active_session_count(&model));
    TEST_ASSERT_FALSE(espui_view_model_remove_session(&model, "ses-running"));

    char id[ESPUI_ID_MAX];
    int extra = 0;
    while (espui_view_model_session_count(&model) < ESPUI_MAX_SESSIONS) {
        snprintf(id, sizeof(id), "extra-%d", extra++);
        TEST_ASSERT_NOT_NULL(espui_view_model_upsert_session(&model, id));
    }
    TEST_ASSERT_NULL(espui_view_model_upsert_session(&model, "overflow"));
    TEST_ASSERT_EQ_INT(ESPUI_MAX_SESSIONS,
                       espui_view_model_session_count(&model));
}

static void test_view_model_attention(void)
{
    espui_view_model_t model;
    espui_view_model_init(&model);

    espui_attention_view_t *approval = espui_view_model_push_attention(
        &model, "per-1", ESPUI_ATTENTION_APPROVAL);
    TEST_ASSERT_NOT_NULL(approval);
    strcpy(approval->summary, "read /tmp/file");

    TEST_ASSERT_NOT_NULL(espui_view_model_push_attention(
        &model, "per-2", ESPUI_ATTENTION_APPROVAL));
    TEST_ASSERT_NOT_NULL(espui_view_model_push_attention(
        &model, "err-1", ESPUI_ATTENTION_ERROR));
    TEST_ASSERT_NOT_NULL(espui_view_model_push_attention(
        &model, "done-1", ESPUI_ATTENTION_COMPLETION));

    TEST_ASSERT_EQ_INT(2, model.approval_count);
    TEST_ASSERT_EQ_INT(1, model.error_count);
    TEST_ASSERT_EQ_INT(1, model.completion_count);
    TEST_ASSERT_EQ_INT(4, espui_view_model_attention_count(&model));

    espui_attention_view_t *again = espui_view_model_push_attention(
        &model, "per-1", ESPUI_ATTENTION_APPROVAL);
    TEST_ASSERT_NOT_NULL(again);
    TEST_ASSERT_STR_EQ("per-1", again->permission_id);
    TEST_ASSERT_EQ_INT(4, espui_view_model_attention_count(&model));

    TEST_ASSERT_TRUE(espui_view_model_resolve_attention(&model, "per-1"));
    TEST_ASSERT_EQ_INT(1, model.approval_count);
    TEST_ASSERT_EQ_INT(3, espui_view_model_attention_count(&model));
    TEST_ASSERT_FALSE(espui_view_model_resolve_attention(&model, "per-1"));

    char id[ESPUI_ID_MAX];
    for (int i = 0; i < ESPUI_MAX_ATTENTION + 2; ++i) {
        snprintf(id, sizeof(id), "fill-%d", i);
        espui_view_model_push_attention(&model, id, ESPUI_ATTENTION_APPROVAL);
    }
    TEST_ASSERT_EQ_INT(ESPUI_MAX_ATTENTION,
                       espui_view_model_attention_count(&model));
}

static void test_safety_attention_count(void)
{
    espui_view_model_t model;
    espui_view_model_init(&model);

    TEST_ASSERT_EQ_INT(0, espui_view_model_safety_attention_count(&model));
    TEST_ASSERT_EQ_INT(0, espui_view_model_safety_attention_count(NULL));

    espui_view_model_note_completion(&model);
    TEST_ASSERT_EQ_INT(1, espui_view_model_attention_count(&model));
    TEST_ASSERT_EQ_INT(0, espui_view_model_safety_attention_count(&model));

    espui_view_model_note_error(&model);
    TEST_ASSERT_EQ_INT(2, espui_view_model_attention_count(&model));
    TEST_ASSERT_EQ_INT(0, espui_view_model_safety_attention_count(&model));

    TEST_ASSERT_NOT_NULL(espui_view_model_push_attention(
        &model, "err-1", ESPUI_ATTENTION_ERROR));
    TEST_ASSERT_NOT_NULL(espui_view_model_push_attention(
        &model, "done-1", ESPUI_ATTENTION_COMPLETION));
    TEST_ASSERT_EQ_INT(0, espui_view_model_safety_attention_count(&model));

    espui_attention_view_t *approval = espui_view_model_push_attention(
        &model, "per-1", ESPUI_ATTENTION_APPROVAL);
    TEST_ASSERT_NOT_NULL(approval);
    TEST_ASSERT_EQ_INT(1, espui_view_model_safety_attention_count(&model));

    TEST_ASSERT_TRUE(espui_view_model_resolve_attention(&model, "per-1"));
    TEST_ASSERT_EQ_INT(0, espui_view_model_safety_attention_count(&model));

    /* Completion/error alerts must not force or pin the Attention scene. */
    espui_scene_manager_t scenes;
    espui_scene_manager_init(&scenes, NULL, NULL);
    TEST_ASSERT_EQ_INT(ESP_OK, espui_scene_manager_request(
                                   &scenes, ESPUI_SCENE_FOCUS,
                                   ESPUI_SCENE_SOURCE_USER));
    TEST_ASSERT_EQ_INT(ESP_OK, espui_scene_manager_set_attention_count(
                                   &scenes,
                                   espui_view_model_safety_attention_count(
                                       &model)));
    TEST_ASSERT_EQ_INT(ESPUI_SCENE_FOCUS, scenes.current);

    TEST_ASSERT_NOT_NULL(espui_view_model_push_attention(
        &model, "per-2", ESPUI_ATTENTION_APPROVAL));
    TEST_ASSERT_EQ_INT(ESP_OK, espui_scene_manager_set_attention_count(
                                   &scenes,
                                   espui_view_model_safety_attention_count(
                                       &model)));
    TEST_ASSERT_EQ_INT(ESPUI_SCENE_ATTENTION, scenes.current);
}

static void test_view_model_copy_and_touch(void)
{
    espui_view_model_t model;
    espui_view_model_t copy;
    espui_view_model_init(&model);

    espui_agent_view_t *agent = espui_view_model_upsert_agent(&model, "copy-a");
    TEST_ASSERT_NOT_NULL(agent);
    strcpy(agent->display_name, "Copied");

    uint32_t revision = model.revision;
    espui_view_model_copy(&copy, &model);
    TEST_ASSERT_EQ_INT(0, memcmp(&copy, &model, sizeof(model)));
    TEST_ASSERT_EQ_INT(revision, copy.revision);

    uint32_t before = model.revision;
    espui_view_model_touch(&model);
    TEST_ASSERT_EQ_INT(before + 1, model.revision);

    espui_view_model_t dst;
    espui_view_model_t src;
    espui_view_model_init(&dst);
    espui_view_model_init(&src);
    dst.device.brightness = 42;
    strcpy(dst.device.active_theme_id, "terminal_aesthetic");
    src.device.brightness = 9;
    strcpy(src.device.active_theme_id, "other_theme");
    src.node.status = ESPUI_NODE_ONLINE;
    strcpy(src.node.node_id, "node-x");
    espui_agent_view_t *src_agent =
        espui_view_model_upsert_agent(&src, "agent-x");
    TEST_ASSERT_NOT_NULL(src_agent);
    src_agent->status = ESPUI_AGENT_BUSY;

    uint32_t dst_revision = dst.revision;
    espui_view_model_copy_business(&dst, &src);
    TEST_ASSERT_EQ_INT(42, dst.device.brightness);
    TEST_ASSERT_STR_EQ("terminal_aesthetic", dst.device.active_theme_id);
    TEST_ASSERT_EQ_INT(ESPUI_NODE_ONLINE, dst.node.status);
    TEST_ASSERT_STR_EQ("node-x", dst.node.node_id);
    TEST_ASSERT_EQ_INT(1, espui_view_model_agent_count(&dst));
    TEST_ASSERT_EQ_INT(dst_revision + 1, dst.revision);
}

static void test_shared_type_helpers(void)
{
    TEST_ASSERT_STR_EQ("online", espui_node_status_name(ESPUI_NODE_ONLINE));
    TEST_ASSERT_STR_EQ("busy", espui_agent_status_name(ESPUI_AGENT_BUSY));
    TEST_ASSERT_STR_EQ("waiting_approval",
                       espui_session_status_name(ESPUI_SESSION_WAITING_APPROVAL));
    TEST_ASSERT_STR_EQ("approval",
                       espui_attention_kind_name(ESPUI_ATTENTION_APPROVAL));
    TEST_ASSERT_STR_EQ("encoder_cw",
                       espui_input_type_name(ESPUI_INPUT_ENCODER_CW));
    TEST_ASSERT_STR_EQ("scene_attention",
                       espui_nav_intent_name(ESPUI_NAV_SCENE_ATTENTION));
    TEST_ASSERT_STR_EQ("session.interrupt",
                       espui_action_type_name(ESPUI_ACTION_INTERRUPT_SESSION));
    TEST_ASSERT_STR_EQ("once",
                       espui_permission_decision_name(ESPUI_PERMISSION_ONCE));

    espui_action_t action = espui_action_interrupt("ses-1");
    TEST_ASSERT_EQ_INT(ESPUI_ACTION_INTERRUPT_SESSION, action.type);
    TEST_ASSERT_STR_EQ("ses-1", action.session_id);

    action = espui_action_permission("per-1", ESPUI_PERMISSION_ALWAYS);
    TEST_ASSERT_EQ_INT(ESPUI_ACTION_RESPOND_PERMISSION, action.type);
    TEST_ASSERT_STR_EQ("per-1", action.permission_id);
    TEST_ASSERT_EQ_INT(ESPUI_PERMISSION_ALWAYS, action.decision);

    action = espui_action_make(ESPUI_ACTION_NONE);
    TEST_ASSERT_EQ_INT(ESPUI_PERMISSION_REJECT, action.decision);
}

void test_view_model_run(void)
{
    test_view_model_init_and_reset();
    test_view_model_agents();
    test_view_model_sessions();
    test_view_model_attention();
    test_safety_attention_count();
    test_view_model_copy_and_touch();
    test_shared_type_helpers();
}
