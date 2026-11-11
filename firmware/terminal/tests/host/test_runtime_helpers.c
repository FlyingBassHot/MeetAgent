#include <string.h>

#include "test_assert.h"
#include "action_dispatcher.h"
#include "animation_scheduler.h"
#include "asset_manager.h"
#include "input_router.h"
#include "memory_manager.h"
#include "navigation.h"

/* ------------------------------------------------------------------------- */
/* Input router / navigation                                                  */

static void test_input_router(void)
{
    espui_input_router_t router;
    espui_input_router_init(&router);
    TEST_ASSERT_EQ_INT(ESPUI_SCENE_AMBIENT, router.scene);
    TEST_ASSERT_EQ_INT(0, espui_input_router_focus_index(&router));

    espui_input_event_t event = {.type = ESPUI_INPUT_ENCODER_CW};
    TEST_ASSERT_EQ_INT(ESPUI_NAV_NEXT, espui_input_router_feed(&router, &event));
    TEST_ASSERT_EQ_INT(1, router.focus_index);
    TEST_ASSERT_EQ_INT(1, espui_input_router_focus_index(&router));

    event.type = ESPUI_INPUT_ENCODER_CCW;
    TEST_ASSERT_EQ_INT(ESPUI_NAV_PREV, espui_input_router_feed(&router, &event));
    TEST_ASSERT_EQ_INT(0, router.focus_index);

    event.type = ESPUI_INPUT_SELECT;
    TEST_ASSERT_EQ_INT(ESPUI_NAV_ACTIVATE,
                       espui_input_router_feed(&router, &event));

    event.type = ESPUI_INPUT_ENCODER_PRESS;
    TEST_ASSERT_EQ_INT(ESPUI_NAV_ACTIVATE,
                       espui_input_router_feed(&router, &event));

    event.type = ESPUI_INPUT_BACK;
    TEST_ASSERT_EQ_INT(ESPUI_NAV_BACK, espui_input_router_feed(&router, &event));

    event.type = ESPUI_INPUT_TOUCH;
    event.pressed = true;
    TEST_ASSERT_EQ_INT(ESPUI_NAV_NONE,
                       espui_input_router_feed(&router, &event));
    event.type = ESPUI_INPUT_TAP;
    TEST_ASSERT_EQ_INT(ESPUI_NAV_ACTIVATE,
                       espui_input_router_feed(&router, &event));

    event.type = ESPUI_INPUT_SWIPE_LEFT;
    TEST_ASSERT_EQ_INT(ESPUI_NAV_SCENE_FOCUS,
                       espui_input_router_feed(&router, &event));
    TEST_ASSERT_EQ_INT(ESPUI_SCENE_FOCUS, router.scene);
    TEST_ASSERT_EQ_INT(ESPUI_NAV_SCENE_ACTIVITY,
                       espui_input_router_feed(&router, &event));
    TEST_ASSERT_EQ_INT(ESPUI_SCENE_ACTIVITY, router.scene);
    TEST_ASSERT_EQ_INT(ESPUI_NAV_NONE,
                       espui_input_router_feed(&router, &event));
    event.type = ESPUI_INPUT_SWIPE_RIGHT;
    TEST_ASSERT_EQ_INT(ESPUI_NAV_SCENE_FOCUS,
                       espui_input_router_feed(&router, &event));
    TEST_ASSERT_EQ_INT(ESPUI_NAV_SCENE_AMBIENT,
                       espui_input_router_feed(&router, &event));
    event.type = ESPUI_INPUT_SWIPE_DOWN;
    TEST_ASSERT_EQ_INT(ESPUI_NAV_BACK,
                       espui_input_router_feed(&router, &event));
    event.type = ESPUI_INPUT_SWIPE_UP;
    TEST_ASSERT_EQ_INT(ESPUI_NAV_NONE,
                       espui_input_router_feed(&router, &event));

    event.type = ESPUI_INPUT_LONG_PRESS;
    TEST_ASSERT_EQ_INT(ESPUI_NAV_NONE,
                       espui_input_router_feed(&router, &event));
    TEST_ASSERT_EQ_INT(ESPUI_SCENE_AMBIENT, router.scene);

    espui_input_router_set_attention_count(&router, 2);
    TEST_ASSERT_EQ_INT(ESPUI_NAV_SCENE_ATTENTION,
                       espui_input_router_feed(&router, &event));
    TEST_ASSERT_EQ_INT(ESPUI_SCENE_ATTENTION, router.scene);

    TEST_ASSERT_EQ_INT(ESPUI_NAV_NONE,
                       espui_input_router_feed(&router, NULL));
    TEST_ASSERT_EQ_INT(ESPUI_NAV_NONE, espui_input_router_feed(NULL, &event));

    espui_input_router_set_scene(&router, ESPUI_SCENE_FOCUS);
    TEST_ASSERT_EQ_INT(ESPUI_SCENE_FOCUS, router.scene);
}

static void test_navigation_apply(void)
{
    espui_scene_manager_t manager;
    espui_scene_manager_init(&manager, NULL, NULL);

    TEST_ASSERT_EQ_INT(ESPUI_NAV_ACTIVATE,
                       espui_navigation_from_input(
                           &(espui_input_event_t){.type = ESPUI_INPUT_SELECT},
                           ESPUI_SCENE_AMBIENT));
    TEST_ASSERT_EQ_INT(ESP_ERR_INVALID_ARG,
                       espui_navigation_apply(ESPUI_NAV_NONE, NULL));
    TEST_ASSERT_EQ_INT(ESP_ERR_INVALID_ARG,
                       espui_navigation_apply(ESPUI_NAV_NEXT, NULL));

    TEST_ASSERT_EQ_INT(ESP_OK,
                       espui_navigation_apply(ESPUI_NAV_ACTIVATE, &manager));
    TEST_ASSERT_EQ_INT(ESPUI_SCENE_FOCUS, manager.current);

    TEST_ASSERT_EQ_INT(ESP_OK, espui_navigation_apply(ESPUI_NAV_BACK, &manager));
    TEST_ASSERT_EQ_INT(ESPUI_SCENE_AMBIENT, manager.current);

    TEST_ASSERT_EQ_INT(ESP_OK,
                       espui_navigation_apply(ESPUI_NAV_SCENE_ACTIVITY,
                                              &manager));
    TEST_ASSERT_EQ_INT(ESPUI_SCENE_ACTIVITY, manager.current);

    TEST_ASSERT_EQ_INT(ESP_OK,
                       espui_navigation_apply(ESPUI_NAV_SCENE_SETTINGS,
                                              &manager));
    TEST_ASSERT_EQ_INT(ESPUI_SCENE_SETTINGS, manager.current);

    /* Toggle Attention is a no-op without pending attention. */
    TEST_ASSERT_EQ_INT(ESP_OK,
                       espui_navigation_apply(ESPUI_NAV_TOGGLE_ATTENTION,
                                              &manager));
    TEST_ASSERT_EQ_INT(ESPUI_SCENE_SETTINGS, manager.current);

    TEST_ASSERT_EQ_INT(ESP_OK,
                       espui_scene_manager_set_attention_count(&manager, 1));
    TEST_ASSERT_EQ_INT(ESPUI_SCENE_ATTENTION, manager.current);

    TEST_ASSERT_EQ_INT(ESP_OK,
                       espui_navigation_apply(ESPUI_NAV_NEXT, &manager));
    TEST_ASSERT_EQ_INT(ESPUI_SCENE_ATTENTION, manager.current);
}

/* ------------------------------------------------------------------------- */
/* Action dispatcher                                                          */

typedef struct {
    int calls;
    espui_action_t last;
} action_sink_t;

static esp_err_t capture_action(const espui_action_t *action, void *user_ctx)
{
    action_sink_t *sink = (action_sink_t *)user_ctx;
    ++sink->calls;
    sink->last = *action;
    return ESP_OK;
}

static void test_action_dispatcher(void)
{
    action_sink_t sink = {0};
    espui_action_dispatcher_t dispatcher;

    espui_action_dispatcher_init(&dispatcher, capture_action, &sink);
    espui_action_t action = espui_action_interrupt("ses-1");
    TEST_ASSERT_EQ_INT(ESP_OK,
                       espui_action_dispatcher_dispatch(&dispatcher, &action));
    TEST_ASSERT_EQ_INT(1, sink.calls);
    TEST_ASSERT_EQ_INT(ESPUI_ACTION_INTERRUPT_SESSION, sink.last.type);
    TEST_ASSERT_STR_EQ("ses-1", sink.last.session_id);

    action = espui_action_permission("per-1", ESPUI_PERMISSION_ONCE);
    TEST_ASSERT_EQ_INT(ESP_OK,
                       espui_action_dispatcher_dispatch(&dispatcher, &action));
    TEST_ASSERT_EQ_INT(2, sink.calls);
    TEST_ASSERT_STR_EQ("per-1", sink.last.permission_id);
    TEST_ASSERT_EQ_INT(ESPUI_PERMISSION_ONCE, sink.last.decision);

    action = espui_action_make(ESPUI_ACTION_NONE);
    TEST_ASSERT_EQ_INT(ESP_ERR_INVALID_ARG,
                       espui_action_dispatcher_dispatch(&dispatcher, &action));
    TEST_ASSERT_EQ_INT(2, sink.calls);

    TEST_ASSERT_EQ_INT(ESP_ERR_INVALID_ARG,
                       espui_action_dispatcher_dispatch(&dispatcher, NULL));
    TEST_ASSERT_EQ_INT(ESP_ERR_INVALID_ARG,
                       espui_action_dispatcher_dispatch(NULL, &action));

    espui_action_dispatcher_init(&dispatcher, NULL, NULL);
    action = espui_action_interrupt("ses-1");
    TEST_ASSERT_EQ_INT(ESP_ERR_INVALID_STATE,
                       espui_action_dispatcher_dispatch(&dispatcher, &action));
}

/* ------------------------------------------------------------------------- */
/* Animation scheduler                                                        */

typedef struct {
    int calls;
    uint32_t last_delta;
    uint32_t total_delta;
} motion_recorder_t;

static void motion_tick(uint32_t delta_ms, void *user_ctx)
{
    motion_recorder_t *recorder = (motion_recorder_t *)user_ctx;
    ++recorder->calls;
    recorder->last_delta = delta_ms;
    recorder->total_delta += delta_ms;
}

static void test_motion_scheduler(void)
{
    espui_animation_scheduler_t scheduler;
    espui_animation_scheduler_init(&scheduler);
    TEST_ASSERT_EQ_INT(100, scheduler.speed_percent);
    TEST_ASSERT_EQ_INT(0, espui_animation_scheduler_tick(&scheduler, 10));
    TEST_ASSERT_EQ_INT(0, espui_animation_scheduler_tick(NULL, 10));

    motion_recorder_t slow = {0};
    motion_recorder_t fast = {0};
    int32_t slow_slot = espui_animation_scheduler_add(
        &scheduler, "slow", motion_tick, &slow, 100);
    int32_t fast_slot = espui_animation_scheduler_add(
        &scheduler, "fast", motion_tick, &fast, 0);
    TEST_ASSERT_EQ_INT(0, slow_slot);
    TEST_ASSERT_EQ_INT(1, fast_slot);
    TEST_ASSERT_EQ_INT(-1, espui_animation_scheduler_add(
                               &scheduler, "bad", NULL, NULL, 0));

    TEST_ASSERT_EQ_INT(1, espui_animation_scheduler_tick(&scheduler, 50));
    TEST_ASSERT_EQ_INT(1, fast.calls);
    TEST_ASSERT_EQ_INT(50, fast.last_delta);
    TEST_ASSERT_EQ_INT(0, slow.calls);

    TEST_ASSERT_EQ_INT(2, espui_animation_scheduler_tick(&scheduler, 50));
    TEST_ASSERT_EQ_INT(1, slow.calls);
    TEST_ASSERT_EQ_INT(100, slow.last_delta);
    TEST_ASSERT_EQ_INT(2, fast.calls);

    TEST_ASSERT_EQ_INT(3, espui_animation_scheduler_tick(&scheduler, 250));
    TEST_ASSERT_EQ_INT(3, slow.calls);
    TEST_ASSERT_EQ_INT(300, slow.total_delta);
    TEST_ASSERT_EQ_INT(3, fast.calls);

    TEST_ASSERT_EQ_INT(2, espui_animation_scheduler_tick(&scheduler, 50));
    TEST_ASSERT_EQ_INT(4, slow.calls);

    espui_animation_scheduler_set_speed(&scheduler, 200);
    TEST_ASSERT_EQ_INT(2, espui_animation_scheduler_tick(&scheduler, 50));
    TEST_ASSERT_EQ_INT(5, slow.calls);
    TEST_ASSERT_EQ_INT(5, fast.calls);

    espui_animation_scheduler_set_speed(&scheduler, 50);
    TEST_ASSERT_EQ_INT(1, espui_animation_scheduler_tick(&scheduler, 100));

    /* Period-0 slots fire every tick; remove it before testing pause. */
    TEST_ASSERT_TRUE(espui_animation_scheduler_remove(&scheduler, fast_slot));

    espui_animation_scheduler_set_paused(&scheduler, true);
    TEST_ASSERT_EQ_INT(0, espui_animation_scheduler_tick(&scheduler, 500));
    espui_animation_scheduler_set_paused(&scheduler, false);
    TEST_ASSERT_EQ_INT(0, espui_animation_scheduler_tick(&scheduler, 50));

    espui_animation_scheduler_set_speed(&scheduler, 0);
    TEST_ASSERT_EQ_INT(0, espui_animation_scheduler_tick(&scheduler, 1000));

    TEST_ASSERT_TRUE(espui_animation_scheduler_remove(&scheduler, slow_slot));
    TEST_ASSERT_FALSE(espui_animation_scheduler_remove(&scheduler, slow_slot));
    TEST_ASSERT_FALSE(
        espui_animation_scheduler_remove(&scheduler, slow_slot + 10));

    espui_animation_scheduler_clear(&scheduler);
    TEST_ASSERT_EQ_INT(0, scheduler.count);
    TEST_ASSERT_EQ_INT(0, espui_animation_scheduler_tick(&scheduler, 1000));
}

static void test_motion_scheduler_catchup_cap(void)
{
    espui_animation_scheduler_t scheduler;
    espui_animation_scheduler_init(&scheduler);
    motion_recorder_t recorder = {0};
    TEST_ASSERT_EQ_INT(0, espui_animation_scheduler_add(
                               &scheduler, "burst", motion_tick, &recorder,
                               10));
    espui_animation_scheduler_set_speed(&scheduler, 1000);
    TEST_ASSERT_EQ_INT(8, espui_animation_scheduler_tick(&scheduler, 1000));
    TEST_ASSERT_EQ_INT(8, recorder.calls);
    /* No runaway backlog after the catch-up cap. */
    TEST_ASSERT_TRUE(scheduler.slots[0].accumulator < 10);
}

/* ------------------------------------------------------------------------- */
/* Asset registry + memory budget                                             */

static void test_asset_registry(void)
{
    espui_asset_registry_t registry;
    espui_asset_registry_init(&registry);
    TEST_ASSERT_EQ_INT(0, espui_asset_registry_count(&registry));

    static const char font_data[] = "font-bytes";
    static const char image_data[] = "image-bytes";
    TEST_ASSERT_EQ_INT(ESP_OK,
                       espui_asset_registry_add(&registry, "terminal_mono",
                                                ESPUI_ASSET_FONT, font_data,
                                                sizeof(font_data)));
    TEST_ASSERT_EQ_INT(ESP_OK,
                       espui_asset_registry_add(&registry, "logo",
                                                ESPUI_ASSET_IMAGE, image_data,
                                                sizeof(image_data)));
    TEST_ASSERT_EQ_INT(2, espui_asset_registry_count(&registry));

    const espui_asset_t *font =
        espui_asset_registry_find(&registry, "terminal_mono");
    TEST_ASSERT_NOT_NULL(font);
    TEST_ASSERT_EQ_INT(ESPUI_ASSET_FONT, font->type);
    TEST_ASSERT_TRUE(font->data == font_data);
    TEST_ASSERT_EQ_INT(sizeof(font_data), font->size);

    TEST_ASSERT_NULL(espui_asset_registry_find(&registry, "missing"));
    TEST_ASSERT_NULL(espui_asset_registry_find(NULL, "missing"));
    TEST_ASSERT_EQ_INT(ESP_ERR_INVALID_ARG,
                       espui_asset_registry_add(&registry, "terminal_mono",
                                                ESPUI_ASSET_FONT, font_data, 1));
    TEST_ASSERT_EQ_INT(ESP_ERR_INVALID_ARG,
                       espui_asset_registry_add(&registry, "", ESPUI_ASSET_FONT,
                                                font_data, 1));
    TEST_ASSERT_EQ_INT(ESP_ERR_INVALID_ARG,
                       espui_asset_registry_add(NULL, "x", ESPUI_ASSET_DATA,
                                                font_data, 1));

    while (espui_asset_registry_count(&registry) < ESPUI_MAX_ASSETS) {
        char name[16];
        snprintf(name, sizeof(name), "asset-%d",
                 (int)espui_asset_registry_count(&registry));
        TEST_ASSERT_EQ_INT(ESP_OK,
                           espui_asset_registry_add(&registry, name,
                                                    ESPUI_ASSET_DATA, font_data,
                                                    1));
    }
    TEST_ASSERT_EQ_INT(ESPUI_ERR_FULL,
                       espui_asset_registry_add(&registry, "overflow",
                                                ESPUI_ASSET_DATA, font_data, 1));
}

static void test_memory_budget(void)
{
    espui_memory_budget_t budget;
    espui_memory_budget_init(&budget, 1000);
    TEST_ASSERT_EQ_INT(1000, budget.budget_bytes);
    TEST_ASSERT_EQ_INT(0, budget.used_bytes);
    TEST_ASSERT_EQ_INT(1000, espui_memory_budget_available(&budget));

    TEST_ASSERT_TRUE(espui_memory_budget_reserve(&budget, 400));
    TEST_ASSERT_TRUE(espui_memory_budget_reserve(&budget, 300));
    TEST_ASSERT_EQ_INT(700, budget.used_bytes);
    TEST_ASSERT_EQ_INT(700, budget.peak_bytes);
    TEST_ASSERT_EQ_INT(300, espui_memory_budget_available(&budget));

    TEST_ASSERT_FALSE(espui_memory_budget_reserve(&budget, 400));
    TEST_ASSERT_EQ_INT(700, budget.used_bytes);
    TEST_ASSERT_EQ_INT(1, budget.failures);

    espui_memory_budget_release(&budget, 300);
    TEST_ASSERT_EQ_INT(400, budget.used_bytes);
    espui_memory_budget_release(&budget, 5000);
    TEST_ASSERT_EQ_INT(0, budget.used_bytes);
    TEST_ASSERT_TRUE(espui_memory_budget_reserve(&budget, 200));
    TEST_ASSERT_EQ_INT(700, budget.peak_bytes);

    espui_memory_budget_reset_peak(&budget);
    TEST_ASSERT_EQ_INT(200, budget.peak_bytes);
    TEST_ASSERT_FALSE(espui_memory_budget_reserve(NULL, 1));
    TEST_ASSERT_FALSE(espui_memory_budget_reserve(&budget, 0));
    TEST_ASSERT_EQ_INT(0, espui_memory_budget_available(NULL));
}

void test_runtime_helpers_run(void)
{
    test_input_router();
    test_navigation_apply();
    test_action_dispatcher();
    test_motion_scheduler();
    test_motion_scheduler_catchup_cap();
    test_asset_registry();
    test_memory_budget();
}
