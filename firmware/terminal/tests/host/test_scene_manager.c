#include <string.h>

#include "test_assert.h"
#include "espui_scene.h"

typedef struct {
    int calls;
    espui_scene_id_t last;
    espui_scene_id_t history[32];
    int history_count;
} scene_recorder_t;

static void record_scene(espui_scene_id_t scene, void *user_ctx)
{
    scene_recorder_t *recorder = (scene_recorder_t *)user_ctx;
    ++recorder->calls;
    recorder->last = scene;
    if (recorder->history_count < (int)(sizeof(recorder->history) /
                                        sizeof(recorder->history[0]))) {
        recorder->history[recorder->history_count++] = scene;
    }
}

static void test_scene_names(void)
{
    TEST_ASSERT_STR_EQ("ambient", espui_scene_name(ESPUI_SCENE_AMBIENT));
    TEST_ASSERT_STR_EQ("focus", espui_scene_name(ESPUI_SCENE_FOCUS));
    TEST_ASSERT_STR_EQ("activity", espui_scene_name(ESPUI_SCENE_ACTIVITY));
    TEST_ASSERT_STR_EQ("attention", espui_scene_name(ESPUI_SCENE_ATTENTION));
    TEST_ASSERT_STR_EQ("settings", espui_scene_name(ESPUI_SCENE_SETTINGS));
    TEST_ASSERT_STR_EQ("invalid", espui_scene_name(ESPUI_SCENE_INVALID));
    TEST_ASSERT_TRUE(espui_scene_is_valid(ESPUI_SCENE_AMBIENT));
    TEST_ASSERT_TRUE(espui_scene_is_valid(ESPUI_SCENE_SETTINGS));
    TEST_ASSERT_FALSE(espui_scene_is_valid(ESPUI_SCENE_COUNT));
    TEST_ASSERT_FALSE(espui_scene_is_valid(ESPUI_SCENE_INVALID));
}

static void test_scene_navigation_stack(void)
{
    espui_scene_manager_t manager;
    scene_recorder_t recorder = {0};

    espui_scene_manager_init(&manager, record_scene, &recorder);
    TEST_ASSERT_EQ_INT(ESPUI_SCENE_AMBIENT, manager.current);
    TEST_ASSERT_EQ_INT(0, manager.depth);
    TEST_ASSERT_EQ_INT(0, recorder.calls);

    TEST_ASSERT_EQ_INT(ESP_OK, espui_scene_manager_request(
                                   &manager, ESPUI_SCENE_FOCUS,
                                   ESPUI_SCENE_SOURCE_USER));
    TEST_ASSERT_EQ_INT(ESPUI_SCENE_FOCUS, manager.current);
    TEST_ASSERT_EQ_INT(ESPUI_SCENE_AMBIENT, manager.previous);
    TEST_ASSERT_EQ_INT(1, manager.depth);
    TEST_ASSERT_EQ_INT(1, recorder.calls);
    TEST_ASSERT_EQ_INT(ESPUI_SCENE_FOCUS, recorder.last);

    TEST_ASSERT_EQ_INT(ESP_OK, espui_scene_manager_request(
                                   &manager, ESPUI_SCENE_ACTIVITY,
                                   ESPUI_SCENE_SOURCE_USER));
    TEST_ASSERT_EQ_INT(ESPUI_SCENE_ACTIVITY, manager.current);
    TEST_ASSERT_EQ_INT(2, manager.depth);

    TEST_ASSERT_EQ_INT(ESP_OK, espui_scene_manager_back(&manager));
    TEST_ASSERT_EQ_INT(ESPUI_SCENE_FOCUS, manager.current);
    TEST_ASSERT_EQ_INT(1, manager.depth);

    TEST_ASSERT_EQ_INT(ESP_OK, espui_scene_manager_back(&manager));
    TEST_ASSERT_EQ_INT(ESPUI_SCENE_AMBIENT, manager.current);
    TEST_ASSERT_EQ_INT(0, manager.depth);

    /* Back at the root is a no-op, not an error. */
    TEST_ASSERT_EQ_INT(ESP_OK, espui_scene_manager_back(&manager));
    TEST_ASSERT_EQ_INT(ESPUI_SCENE_AMBIENT, manager.current);

    /* Same-scene requests do not notify or grow the stack. */
    int calls_before = recorder.calls;
    TEST_ASSERT_EQ_INT(ESP_OK, espui_scene_manager_request(
                                   &manager, ESPUI_SCENE_AMBIENT,
                                   ESPUI_SCENE_SOURCE_USER));
    TEST_ASSERT_EQ_INT(calls_before, recorder.calls);
    TEST_ASSERT_EQ_INT(0, manager.depth);

    TEST_ASSERT_EQ_INT(ESP_ERR_INVALID_ARG,
                       espui_scene_manager_request(&manager, ESPUI_SCENE_COUNT,
                                                   ESPUI_SCENE_SOURCE_USER));
    TEST_ASSERT_EQ_INT(ESP_ERR_INVALID_ARG, espui_scene_manager_back(NULL));
}

static void test_scene_attention_priority(void)
{
    espui_scene_manager_t manager;
    scene_recorder_t recorder = {0};

    espui_scene_manager_init(&manager, record_scene, &recorder);
    TEST_ASSERT_EQ_INT(ESP_OK, espui_scene_manager_request(
                                   &manager, ESPUI_SCENE_FOCUS,
                                   ESPUI_SCENE_SOURCE_USER));

    TEST_ASSERT_EQ_INT(ESP_OK,
                       espui_scene_manager_set_attention_count(&manager, 2));
    TEST_ASSERT_EQ_INT(ESPUI_SCENE_ATTENTION, manager.current);
    TEST_ASSERT_EQ_INT(ESPUI_SCENE_FOCUS, manager.resume_scene);
    TEST_ASSERT_TRUE(espui_scene_manager_in_attention(&manager));

    /* While approvals are pending, normal navigation cannot hide Attention. */
    TEST_ASSERT_EQ_INT(ESP_ERR_INVALID_STATE,
                       espui_scene_manager_request(&manager,
                                                   ESPUI_SCENE_ACTIVITY,
                                                   ESPUI_SCENE_SOURCE_USER));
    TEST_ASSERT_EQ_INT(ESPUI_SCENE_ATTENTION, manager.current);
    TEST_ASSERT_EQ_INT(ESP_ERR_INVALID_STATE, espui_scene_manager_back(&manager));

    /* Reducing but not clearing the queue keeps Attention sticky. */
    TEST_ASSERT_EQ_INT(ESP_OK,
                       espui_scene_manager_set_attention_count(&manager, 1));
    TEST_ASSERT_EQ_INT(ESPUI_SCENE_ATTENTION, manager.current);

    /* Clearing the queue resumes the previous user scene. */
    TEST_ASSERT_EQ_INT(ESP_OK,
                       espui_scene_manager_set_attention_count(&manager, 0));
    TEST_ASSERT_EQ_INT(ESPUI_SCENE_FOCUS, manager.current);
    TEST_ASSERT_FALSE(espui_scene_manager_in_attention(&manager));
}

static void test_scene_force_and_bounded_stack(void)
{
    espui_scene_manager_t manager;
    scene_recorder_t recorder = {0};

    espui_scene_manager_init(&manager, record_scene, &recorder);
    TEST_ASSERT_EQ_INT(ESP_OK, espui_scene_manager_force(
                                   &manager, ESPUI_SCENE_SETTINGS,
                                   ESPUI_SCENE_SOURCE_SYSTEM));
    TEST_ASSERT_EQ_INT(ESPUI_SCENE_SETTINGS, manager.current);

    for (int i = 0; i < ESPUI_MAX_SCENE_STACK + 4; ++i) {
        espui_scene_id_t scene = (i % 2 == 0) ? ESPUI_SCENE_FOCUS
                                              : ESPUI_SCENE_ACTIVITY;
        TEST_ASSERT_EQ_INT(ESP_OK, espui_scene_manager_request(
                                       &manager, scene,
                                       ESPUI_SCENE_SOURCE_USER));
    }
    TEST_ASSERT_TRUE(manager.depth <= ESPUI_MAX_SCENE_STACK);
    TEST_ASSERT_EQ_INT(ESPUI_MAX_SCENE_STACK, manager.depth);

    TEST_ASSERT_EQ_INT(ESP_ERR_INVALID_ARG,
                       espui_scene_manager_set_attention_count(NULL, 1));
}

void test_scene_manager_run(void)
{
    test_scene_names();
    test_scene_navigation_stack();
    test_scene_attention_priority();
    test_scene_force_and_bounded_stack();
}
