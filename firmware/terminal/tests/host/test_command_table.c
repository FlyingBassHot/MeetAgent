#include <string.h>

#include "command_table.h"
#include "test_assert.h"

static espui_action_t action_for(const char *session_id)
{
    espui_action_t action;
    memset(&action, 0, sizeof(action));
    action.type = ESPUI_ACTION_INTERRUPT_SESSION;
    strcpy(action.session_id, session_id);
    return action;
}

static void test_add_capacity_and_duplicate(void)
{
    meet_pending_commands_t table;
    meet_pending_commands_init(&table);
    TEST_ASSERT_EQ_INT(0, meet_pending_commands_count(&table));

    for (int i = 0; i < MEET_PENDING_COMMAND_MAX; ++i) {
        char command_id[MEET_PENDING_COMMAND_ID_MAX];
        snprintf(command_id, sizeof(command_id), "cmd-%d", i);
        espui_action_t action = action_for("ses-1");
        TEST_ASSERT_EQ_INT(ESP_OK, meet_pending_commands_add(
                                       &table, command_id, &action,
                                       (uint32_t)(100 + i)));
    }
    TEST_ASSERT_EQ_INT(MEET_PENDING_COMMAND_MAX,
                       meet_pending_commands_count(&table));

    espui_action_t action = action_for("ses-1");
    TEST_ASSERT_EQ_INT(ESP_ERR_NO_MEM,
                       meet_pending_commands_add(&table, "cmd-full", &action, 0));
    TEST_ASSERT_EQ_INT(ESP_ERR_INVALID_STATE,
                       meet_pending_commands_add(&table, "cmd-0", &action, 0));
    TEST_ASSERT_EQ_INT(ESP_ERR_INVALID_ARG,
                       meet_pending_commands_add(NULL, "cmd-x", &action, 0));
    TEST_ASSERT_EQ_INT(ESP_ERR_INVALID_ARG,
                       meet_pending_commands_add(&table, "", &action, 0));
    TEST_ASSERT_EQ_INT(ESP_ERR_INVALID_ARG,
                       meet_pending_commands_add(&table, "cmd-x", NULL, 0));
}

static void test_take_matches_and_copies_action(void)
{
    meet_pending_commands_t table;
    meet_pending_commands_init(&table);

    espui_action_t interrupt = action_for("ses-test");
    interrupt.type = ESPUI_ACTION_INTERRUPT_SESSION;
    TEST_ASSERT_EQ_INT(ESP_OK, meet_pending_commands_add(
                                   &table, "cmd-interrupt", &interrupt, 100));

    espui_action_t permission;
    memset(&permission, 0, sizeof(permission));
    permission.type = ESPUI_ACTION_RESPOND_PERMISSION;
    strcpy(permission.permission_id, "per-1");
    permission.decision = ESPUI_PERMISSION_REJECT;
    TEST_ASSERT_EQ_INT(ESP_OK, meet_pending_commands_add(
                                   &table, "cmd-permission", &permission, 200));

    meet_pending_command_t found;
    memset(&found, 0, sizeof(found));
    TEST_ASSERT_EQ_INT(ESP_OK, meet_pending_commands_take(
                                   &table, "cmd-permission", &found));
    TEST_ASSERT_STR_EQ("cmd-permission", found.command_id);
    TEST_ASSERT_EQ_INT(200, found.sent_ms);
    TEST_ASSERT_EQ_INT(ESPUI_ACTION_RESPOND_PERMISSION, found.action.type);
    TEST_ASSERT_STR_EQ("per-1", found.action.permission_id);
    TEST_ASSERT_EQ_INT(ESPUI_PERMISSION_REJECT, found.action.decision);
    TEST_ASSERT_EQ_INT(1, meet_pending_commands_count(&table));

    TEST_ASSERT_EQ_INT(ESP_ERR_NOT_FOUND, meet_pending_commands_take(
                                              &table, "cmd-permission", &found));
    TEST_ASSERT_EQ_INT(ESP_ERR_NOT_FOUND,
                       meet_pending_commands_take(&table, "missing", &found));
    TEST_ASSERT_EQ_INT(ESP_ERR_INVALID_ARG,
                       meet_pending_commands_take(NULL, "cmd", &found));
    TEST_ASSERT_EQ_INT(ESP_ERR_INVALID_ARG,
                       meet_pending_commands_take(&table, "", &found));
    TEST_ASSERT_EQ_INT(ESP_ERR_INVALID_ARG,
                       meet_pending_commands_take(&table, "cmd", NULL));
}

static void test_expire_timeout_and_wrap(void)
{
    meet_pending_commands_t table;
    meet_pending_commands_init(&table);

    espui_action_t action = action_for("ses-old");
    TEST_ASSERT_EQ_INT(ESP_OK, meet_pending_commands_add(
                                   &table, "cmd-old", &action, 1000));
    action = action_for("ses-new");
    TEST_ASSERT_EQ_INT(ESP_OK, meet_pending_commands_add(
                                   &table, "cmd-new", &action, 6000));
    meet_pending_command_t expired[MEET_PENDING_COMMAND_MAX];
    memset(expired, 0, sizeof(expired));
    TEST_ASSERT_EQ_INT(0, meet_pending_commands_take_expired(
                              &table, 15999, 15000, expired,
                              MEET_PENDING_COMMAND_MAX));
    TEST_ASSERT_EQ_INT(2, meet_pending_commands_count(&table));

    TEST_ASSERT_EQ_INT(1, meet_pending_commands_take_expired(
                              &table, 16000, 15000, expired,
                              MEET_PENDING_COMMAND_MAX));
    TEST_ASSERT_STR_EQ("cmd-old", expired[0].command_id);
    TEST_ASSERT_STR_EQ("ses-old", expired[0].action.session_id);
    TEST_ASSERT_EQ_INT(1, meet_pending_commands_count(&table));

    TEST_ASSERT_EQ_INT(0, meet_pending_commands_take_expired(
                              &table, 20000, 15000, expired,
                              MEET_PENDING_COMMAND_MAX));
    TEST_ASSERT_EQ_INT(1, meet_pending_commands_count(&table));

    meet_pending_commands_t wrap_table;
    meet_pending_commands_init(&wrap_table);
    action = action_for("ses-wrap");
    TEST_ASSERT_EQ_INT(ESP_OK, meet_pending_commands_add(
                                   &wrap_table, "cmd-wrap", &action,
                                   0xFFFFFF00u));
    TEST_ASSERT_EQ_INT(1, meet_pending_commands_take_expired(
                              &wrap_table, 0x100, 500, expired,
                              MEET_PENDING_COMMAND_MAX));
    TEST_ASSERT_STR_EQ("cmd-wrap", expired[0].command_id);
    TEST_ASSERT_EQ_INT(0, meet_pending_commands_count(&wrap_table));
}

static void test_drain_connection_lost(void)
{
    meet_pending_commands_t table;
    meet_pending_commands_init(&table);
    for (int i = 0; i < 3; ++i) {
        char command_id[MEET_PENDING_COMMAND_ID_MAX];
        snprintf(command_id, sizeof(command_id), "cmd-%d", i);
        espui_action_t action = action_for("ses-1");
        TEST_ASSERT_EQ_INT(ESP_OK, meet_pending_commands_add(
                                       &table, command_id, &action,
                                       (uint32_t)i));
    }

    meet_pending_command_t drained[MEET_PENDING_COMMAND_MAX];
    memset(drained, 0, sizeof(drained));
    TEST_ASSERT_EQ_INT(3, meet_pending_commands_drain(
                              &table, drained, MEET_PENDING_COMMAND_MAX));
    TEST_ASSERT_STR_EQ("cmd-0", drained[0].command_id);
    TEST_ASSERT_STR_EQ("cmd-1", drained[1].command_id);
    TEST_ASSERT_STR_EQ("cmd-2", drained[2].command_id);
    TEST_ASSERT_EQ_INT(0, meet_pending_commands_count(&table));
    TEST_ASSERT_EQ_INT(0, meet_pending_commands_drain(
                              &table, drained, MEET_PENDING_COMMAND_MAX));
    TEST_ASSERT_EQ_INT(0, meet_pending_commands_drain(NULL, drained, 1));
    TEST_ASSERT_EQ_INT(0, meet_pending_commands_drain(&table, NULL, 1));
}

void test_command_table_run(void)
{
    test_add_capacity_and_duplicate();
    test_take_matches_and_copies_action();
    test_expire_timeout_and_wrap();
    test_drain_connection_lost();
}
