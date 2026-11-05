#include <string.h>

#include "test_assert.h"
#include "espui_theme.h"
#include "theme_manager.h"

/* ------------------------------------------------------------------------- */
/* Fake themes and renderer                                                   */

typedef struct {
    char events[64];
    int event_count;
    esp_err_t init_result;
    esp_err_t enter_result;
    espui_scene_id_t last_scene;
    int updates;
    int ticks;
    int destroys;
} fake_theme_t;

static fake_theme_t g_a;
static fake_theme_t g_b;
static fake_theme_t g_safe;

static void record(fake_theme_t *fake, char event)
{
    if (fake->event_count < (int)sizeof(fake->events) - 1) {
        fake->events[fake->event_count++] = event;
        fake->events[fake->event_count] = '\0';
    }
}

static esp_err_t a_init(const espui_context_t *ctx)
{
    (void)ctx;
    record(&g_a, 'I');
    return g_a.init_result;
}

static esp_err_t a_enter(espui_scene_id_t scene, const espui_view_model_t *model)
{
    (void)model;
    g_a.last_scene = scene;
    record(&g_a, 'E');
    return g_a.enter_result;
}

static void a_update(const espui_view_model_t *model)
{
    (void)model;
    ++g_a.updates;
    record(&g_a, 'U');
}

static void a_tick(uint32_t delta_ms)
{
    (void)delta_ms;
    ++g_a.ticks;
}

static void a_input(const espui_input_event_t *event)
{
    (void)event;
}

static void a_result(const espui_action_result_t *result)
{
    (void)result;
}

static void a_destroy(void)
{
    ++g_a.destroys;
    record(&g_a, 'D');
}

static esp_err_t b_init(const espui_context_t *ctx)
{
    (void)ctx;
    record(&g_b, 'I');
    return g_b.init_result;
}

static esp_err_t b_enter(espui_scene_id_t scene, const espui_view_model_t *model)
{
    (void)model;
    g_b.last_scene = scene;
    record(&g_b, 'E');
    return g_b.enter_result;
}

static void b_update(const espui_view_model_t *model)
{
    (void)model;
    ++g_b.updates;
    record(&g_b, 'U');
}

static void b_tick(uint32_t delta_ms)
{
    (void)delta_ms;
    ++g_b.ticks;
}

static void b_input(const espui_input_event_t *event)
{
    (void)event;
}

static void b_result(const espui_action_result_t *result)
{
    (void)result;
}

static void b_destroy(void)
{
    ++g_b.destroys;
    record(&g_b, 'D');
}

static esp_err_t safe_init(const espui_context_t *ctx)
{
    (void)ctx;
    record(&g_safe, 'I');
    return g_safe.init_result;
}

static esp_err_t safe_enter(espui_scene_id_t scene,
                            const espui_view_model_t *model)
{
    (void)model;
    g_safe.last_scene = scene;
    record(&g_safe, 'E');
    return g_safe.enter_result;
}

static void safe_update(const espui_view_model_t *model)
{
    (void)model;
    ++g_safe.updates;
    record(&g_safe, 'U');
}

static void safe_tick(uint32_t delta_ms)
{
    (void)delta_ms;
    ++g_safe.ticks;
}

static void safe_input(const espui_input_event_t *event)
{
    (void)event;
}

static void safe_result(const espui_action_result_t *result)
{
    (void)result;
}

static void safe_destroy(void)
{
    ++g_safe.destroys;
    record(&g_safe, 'D');
}

static const espui_theme_ops_t OPS_A = {
    .init = a_init,
    .enter_scene = a_enter,
    .update = a_update,
    .on_tick = a_tick,
    .on_input = a_input,
    .on_action_result = a_result,
    .destroy = a_destroy,
};

static const espui_theme_ops_t OPS_B = {
    .init = b_init,
    .enter_scene = b_enter,
    .update = b_update,
    .on_tick = b_tick,
    .on_input = b_input,
    .on_action_result = b_result,
    .destroy = b_destroy,
};

static const espui_theme_ops_t OPS_SAFE = {
    .init = safe_init,
    .enter_scene = safe_enter,
    .update = safe_update,
    .on_tick = safe_tick,
    .on_input = safe_input,
    .on_action_result = safe_result,
    .destroy = safe_destroy,
};

static const espui_renderer_ops_t FAKE_RENDERER_OPS = {0};
static const espui_renderer_t FAKE_RENDERER = {
    .id = "builtin.test",
    .ops = &FAKE_RENDERER_OPS,
};

static const espui_tokens_t TOKENS_A = {
    .theme_id = "theme_a",
    .colors = {.primary = 0x112233},
};

static espui_manifest_t MANIFEST_A = {
    .theme_id = "theme_a",
    .name = "Theme A",
    .version = "1.0.0",
    .api_version = ESPUI_THEME_API_VERSION,
    .renderer_id = "builtin.test",
    .display = {.orientation = ESPUI_ORIENTATION_PORTRAIT,
                .min_width = 320,
                .min_height = 480},
    .scene_mask = ESPUI_SCENE_MASK_ALL,
    .configurable_mask = ESPUI_CONFIG_BRIGHTNESS,
    .motion_profile_id = "motion_none",
};

static espui_manifest_t MANIFEST_B = {
    .theme_id = "theme_b",
    .name = "Theme B",
    .version = "1.0.0",
    .api_version = ESPUI_THEME_API_VERSION,
    .renderer_id = "builtin.test",
    .display = {.orientation = ESPUI_ORIENTATION_PORTRAIT,
                .min_width = 320,
                .min_height = 480},
    .scene_mask = ESPUI_SCENE_MASK_ALL,
    .configurable_mask = ESPUI_CONFIG_BRIGHTNESS,
    .motion_profile_id = "motion_none",
};

static espui_manifest_t MANIFEST_SAFE = {
    .theme_id = "safe_test",
    .name = "Safe Test",
    .version = "1.0.0",
    .api_version = ESPUI_THEME_API_VERSION,
    .renderer_id = "builtin.test",
    .display = {.orientation = ESPUI_ORIENTATION_PORTRAIT,
                .min_width = 320,
                .min_height = 480},
    .scene_mask = ESPUI_SCENE_MASK_ALL,
    .configurable_mask = ESPUI_CONFIG_BRIGHTNESS,
    .motion_profile_id = "motion_none",
};

static espui_manifest_t MANIFEST_BAD_API = {
    .theme_id = "bad_api",
    .name = "Bad API",
    .version = "1.0.0",
    .api_version = ESPUI_THEME_API_VERSION + 1,
    .renderer_id = "builtin.test",
    .display = {.orientation = ESPUI_ORIENTATION_PORTRAIT,
                .min_width = 320,
                .min_height = 480},
    .scene_mask = ESPUI_SCENE_MASK_ALL,
    .configurable_mask = 0,
    .motion_profile_id = "motion_none",
};

static espui_manifest_t MANIFEST_BAD_RENDERER = {
    .theme_id = "bad_renderer",
    .name = "Bad Renderer",
    .version = "1.0.0",
    .api_version = ESPUI_THEME_API_VERSION,
    .renderer_id = "renderer.missing",
    .display = {.orientation = ESPUI_ORIENTATION_PORTRAIT,
                .min_width = 320,
                .min_height = 480},
    .scene_mask = ESPUI_SCENE_MASK_ALL,
    .configurable_mask = 0,
    .motion_profile_id = "motion_none",
};

static espui_manifest_t MANIFEST_BAD_DISPLAY = {
    .theme_id = "bad_display",
    .name = "Bad Display",
    .version = "1.0.0",
    .api_version = ESPUI_THEME_API_VERSION,
    .renderer_id = "builtin.test",
    .display = {.orientation = ESPUI_ORIENTATION_PORTRAIT,
                .min_width = 800,
                .min_height = 600},
    .scene_mask = ESPUI_SCENE_MASK_ALL,
    .configurable_mask = 0,
    .motion_profile_id = "motion_none",
};

static const espui_theme_descriptor_t THEME_A = {
    .manifest = &MANIFEST_A,
    .ops = &OPS_A,
    .tokens = &TOKENS_A,
};
static const espui_theme_descriptor_t THEME_B = {
    .manifest = &MANIFEST_B,
    .ops = &OPS_B,
    .tokens = NULL,
};
static const espui_theme_descriptor_t THEME_SAFE = {
    .manifest = &MANIFEST_SAFE,
    .ops = &OPS_SAFE,
    .tokens = NULL,
};
static const espui_theme_descriptor_t THEME_BAD_API = {
    .manifest = &MANIFEST_BAD_API,
    .ops = &OPS_A,
    .tokens = NULL,
};
static const espui_theme_descriptor_t THEME_BAD_RENDERER = {
    .manifest = &MANIFEST_BAD_RENDERER,
    .ops = &OPS_A,
    .tokens = NULL,
};
static const espui_theme_descriptor_t THEME_BAD_DISPLAY = {
    .manifest = &MANIFEST_BAD_DISPLAY,
    .ops = &OPS_A,
    .tokens = NULL,
};

static espui_display_info_t DISPLAY = {
    .panel_name = "test-panel",
    .width = 320,
    .height = 480,
    .orientation = ESPUI_ORIENTATION_PORTRAIT,
    .color_depth = 16,
    .supports_touch = true,
};

static void reset_world(void)
{
    memset(&g_a, 0, sizeof(g_a));
    memset(&g_b, 0, sizeof(g_b));
    memset(&g_safe, 0, sizeof(g_safe));
    espui_renderer_registry_reset();
    espui_theme_registry_reset();
    TEST_ASSERT_EQ_INT(ESP_OK, espui_renderer_registry_add(&FAKE_RENDERER));
    TEST_ASSERT_EQ_INT(ESP_OK, espui_theme_registry_add(&THEME_A));
    TEST_ASSERT_EQ_INT(ESP_OK, espui_theme_registry_add(&THEME_B));
    TEST_ASSERT_EQ_INT(ESP_OK, espui_theme_registry_add(&THEME_SAFE));
}

static void init_manager(espui_theme_manager_t *manager,
                         const espui_theme_descriptor_t *fallback)
{
    espui_theme_manager_init(manager, NULL, fallback, &DISPLAY);
}

/* ------------------------------------------------------------------------- */
/* Renderer + theme registry                                                  */

static void test_renderer_registry(void)
{
    espui_renderer_registry_reset();
    TEST_ASSERT_EQ_INT(0, espui_renderer_registry_count());
    TEST_ASSERT_EQ_INT(ESP_OK, espui_renderer_registry_add(&FAKE_RENDERER));
    TEST_ASSERT_EQ_INT(ESP_ERR_INVALID_ARG,
                       espui_renderer_registry_add(NULL));
    TEST_ASSERT_EQ_INT(ESP_ERR_INVALID_ARG,
                       espui_renderer_registry_add(&FAKE_RENDERER));

    const espui_renderer_t *found =
        espui_renderer_registry_find("builtin.test");
    TEST_ASSERT_NOT_NULL(found);
    TEST_ASSERT_STR_EQ("builtin.test", found->id);
    TEST_ASSERT_NULL(espui_renderer_registry_find("missing"));
    TEST_ASSERT_EQ_INT(1, espui_renderer_registry_count());
    TEST_ASSERT_TRUE(espui_renderer_registry_get(0) == &FAKE_RENDERER);
    TEST_ASSERT_NULL(espui_renderer_registry_get(1));
}

static void test_theme_registry(void)
{
    espui_theme_registry_reset();
    TEST_ASSERT_EQ_INT(0, espui_theme_registry_count());
    TEST_ASSERT_EQ_INT(ESP_OK, espui_theme_registry_add(&THEME_A));
    TEST_ASSERT_EQ_INT(ESP_OK, espui_theme_registry_add(&THEME_B));
    TEST_ASSERT_EQ_INT(ESP_ERR_INVALID_ARG, espui_theme_registry_add(NULL));
    TEST_ASSERT_EQ_INT(ESP_ERR_INVALID_ARG,
                       espui_theme_registry_add(&THEME_A));

    TEST_ASSERT_TRUE(espui_theme_registry_find("theme_a") == &THEME_A);
    TEST_ASSERT_TRUE(espui_theme_registry_find("theme_b") == &THEME_B);
    TEST_ASSERT_NULL(espui_theme_registry_find("missing"));
    TEST_ASSERT_EQ_INT(2, espui_theme_registry_count());
    TEST_ASSERT_TRUE(espui_theme_registry_get(0) == &THEME_A);
    TEST_ASSERT_TRUE(espui_theme_registry_get(1) == &THEME_B);
    TEST_ASSERT_NULL(espui_theme_registry_get(2));
}

/* ------------------------------------------------------------------------- */
/* Theme manager                                                              */

static void test_apply_and_switch_preserves_model(void)
{
    reset_world();
    espui_theme_manager_t manager;
    init_manager(&manager, &THEME_SAFE);

    espui_view_model_t model;
    espui_view_model_init(&model);
    espui_agent_view_t *agent =
        espui_view_model_upsert_agent(&model, "agent-a");
    TEST_ASSERT_NOT_NULL(agent);
    agent->status = ESPUI_AGENT_BUSY;
    espui_view_model_t before = model;

    TEST_ASSERT_EQ_INT(ESP_OK, espui_theme_manager_apply(
                                   &manager, "theme_a", ESPUI_SCENE_FOCUS,
                                   &model));
    TEST_ASSERT_STR_EQ("IEU", g_a.events);
    TEST_ASSERT_EQ_INT(ESPUI_SCENE_FOCUS, g_a.last_scene);
    TEST_ASSERT_TRUE(espui_theme_manager_active(&manager) == &THEME_A);
    TEST_ASSERT_TRUE(espui_theme_manager_renderer(&manager) ==
                     &FAKE_RENDERER);
    TEST_ASSERT_TRUE(espui_theme_manager_tokens(&manager) == &TOKENS_A);
    TEST_ASSERT_EQ_INT(ESPUI_THEME_SWITCH_OK, manager.last_report.status);
    TEST_ASSERT_STR_EQ("theme_a", manager.last_report.active_id);
    TEST_ASSERT_EQ_INT(0, memcmp(&before, &model, sizeof(model)));

    TEST_ASSERT_EQ_INT(ESP_OK, espui_theme_manager_apply(
                                   &manager, "theme_b", ESPUI_SCENE_ATTENTION,
                                   &model));
    TEST_ASSERT_STR_EQ("IEUD", g_a.events);
    TEST_ASSERT_STR_EQ("IEU", g_b.events);
    TEST_ASSERT_EQ_INT(ESPUI_SCENE_ATTENTION, g_b.last_scene);
    TEST_ASSERT_TRUE(espui_theme_manager_active(&manager) == &THEME_B);
    TEST_ASSERT_TRUE(espui_theme_manager_tokens(&manager) ==
                     espui_tokens_safe_default());
    TEST_ASSERT_EQ_INT(0, memcmp(&before, &model, sizeof(model)));
    TEST_ASSERT_EQ_INT(2, manager.switches);
}

static void test_same_theme_updates_without_reinit(void)
{
    reset_world();
    espui_theme_manager_t manager;
    init_manager(&manager, &THEME_SAFE);

    espui_view_model_t model;
    espui_view_model_init(&model);

    TEST_ASSERT_EQ_INT(ESP_OK, espui_theme_manager_apply(
                                   &manager, "theme_a", ESPUI_SCENE_AMBIENT,
                                   &model));
    TEST_ASSERT_EQ_INT(ESP_OK, espui_theme_manager_apply(
                                   &manager, "theme_a", ESPUI_SCENE_AMBIENT,
                                   &model));
    TEST_ASSERT_STR_EQ("IEUU", g_a.events);
    TEST_ASSERT_EQ_INT(0, g_a.destroys);
    TEST_ASSERT_EQ_INT(ESPUI_THEME_SWITCH_SAME_THEME,
                       manager.last_report.status);
    TEST_ASSERT_EQ_INT(1, manager.switches);
}

static void test_unknown_theme_falls_back(void)
{
    reset_world();
    espui_theme_manager_t manager;
    init_manager(&manager, &THEME_SAFE);

    espui_view_model_t model;
    espui_view_model_init(&model);
    TEST_ASSERT_EQ_INT(ESP_OK, espui_theme_manager_apply(
                                   &manager, "theme_a", ESPUI_SCENE_AMBIENT,
                                   &model));

    TEST_ASSERT_EQ_INT(ESP_ERR_NOT_FOUND,
                       espui_theme_manager_apply(&manager, "not_registered",
                                                 ESPUI_SCENE_FOCUS, &model));
    TEST_ASSERT_EQ_INT(ESPUI_THEME_SWITCH_FALLBACK_USED,
                       manager.last_report.status);
    TEST_ASSERT_EQ_INT(ESPUI_THEME_REASON_UNKNOWN_THEME,
                       manager.last_report.reason);
    TEST_ASSERT_STR_EQ("safe_test", manager.last_report.active_id);
    TEST_ASSERT_TRUE(espui_theme_manager_active(&manager) == &THEME_SAFE);
    TEST_ASSERT_STR_EQ("IEUD", g_a.events);
    TEST_ASSERT_STR_EQ("IEU", g_safe.events);
    TEST_ASSERT_EQ_INT(ESPUI_SCENE_FOCUS, g_safe.last_scene);
}

static void test_incompatible_theme_rejected(void)
{
    reset_world();
    TEST_ASSERT_EQ_INT(ESP_OK, espui_theme_registry_add(&THEME_BAD_API));
    TEST_ASSERT_EQ_INT(ESP_OK, espui_theme_registry_add(&THEME_BAD_RENDERER));
    TEST_ASSERT_EQ_INT(ESP_OK, espui_theme_registry_add(&THEME_BAD_DISPLAY));

    espui_theme_manager_t manager;
    init_manager(&manager, &THEME_SAFE);
    espui_view_model_t model;
    espui_view_model_init(&model);

    TEST_ASSERT_EQ_INT(ESPUI_ERR_MANIFEST,
                       espui_theme_manager_apply(&manager, "bad_api",
                                                 ESPUI_SCENE_AMBIENT, &model));
    TEST_ASSERT_EQ_INT(ESPUI_THEME_SWITCH_FALLBACK_USED,
                       manager.last_report.status);
    TEST_ASSERT_EQ_INT(ESPUI_THEME_REASON_MANIFEST,
                       manager.last_report.reason);
    TEST_ASSERT_EQ_INT(ESPUI_MANIFEST_BAD_API,
                       manager.last_report.manifest_status);
    TEST_ASSERT_TRUE(espui_theme_manager_active(&manager) == &THEME_SAFE);

    memset(&g_safe, 0, sizeof(g_safe));
    TEST_ASSERT_EQ_INT(ESPUI_ERR_RENDERER_MISSING,
                       espui_theme_manager_apply(&manager, "bad_renderer",
                                                 ESPUI_SCENE_AMBIENT, &model));
    TEST_ASSERT_EQ_INT(ESPUI_THEME_REASON_RENDERER_MISSING,
                       manager.last_report.reason);
    TEST_ASSERT_TRUE(espui_theme_manager_active(&manager) == &THEME_SAFE);

    memset(&g_safe, 0, sizeof(g_safe));
    TEST_ASSERT_EQ_INT(ESPUI_ERR_MANIFEST,
                       espui_theme_manager_apply(&manager, "bad_display",
                                                 ESPUI_SCENE_AMBIENT, &model));
    TEST_ASSERT_EQ_INT(ESPUI_THEME_REASON_DISPLAY_UNSUPPORTED,
                       manager.last_report.reason);
    TEST_ASSERT_EQ_INT(ESPUI_MANIFEST_BAD_DISPLAY,
                       manager.last_report.manifest_status);
    TEST_ASSERT_TRUE(espui_theme_manager_active(&manager) == &THEME_SAFE);
}

static void test_init_failure_falls_back(void)
{
    reset_world();

    /* A valid theme whose init fails is represented by theme_b + flag. */
    g_b.init_result = ESP_ERR_NO_MEM;

    espui_theme_manager_t manager;
    init_manager(&manager, &THEME_SAFE);
    espui_view_model_t model;
    espui_view_model_init(&model);

    TEST_ASSERT_EQ_INT(ESPUI_ERR_THEME_INIT,
                       espui_theme_manager_apply(&manager, "theme_b",
                                                 ESPUI_SCENE_ATTENTION, &model));
    TEST_ASSERT_EQ_INT(ESPUI_THEME_SWITCH_FALLBACK_USED,
                       manager.last_report.status);
    TEST_ASSERT_EQ_INT(ESPUI_THEME_REASON_INIT_FAILED,
                       manager.last_report.reason);
    TEST_ASSERT_STR_EQ("ID", g_b.events);
    TEST_ASSERT_STR_EQ("IEU", g_safe.events);
    TEST_ASSERT_TRUE(espui_theme_manager_active(&manager) == &THEME_SAFE);
}

static void test_fallback_failure_is_reported(void)
{
    reset_world();
    espui_theme_manager_t manager;
    init_manager(&manager, &THEME_SAFE);
    g_safe.init_result = ESP_ERR_NO_MEM;

    espui_view_model_t model;
    espui_view_model_init(&model);

    TEST_ASSERT_EQ_INT(ESPUI_ERR_FALLBACK_FAILED,
                       espui_theme_manager_apply(&manager, "missing",
                                                 ESPUI_SCENE_AMBIENT, &model));
    TEST_ASSERT_EQ_INT(ESPUI_THEME_SWITCH_NO_FALLBACK,
                       manager.last_report.status);
    TEST_ASSERT_EQ_INT(ESPUI_THEME_REASON_FALLBACK_FAILED,
                       manager.last_report.reason);
    TEST_ASSERT_NULL(espui_theme_manager_active(&manager));
    TEST_ASSERT_NULL(espui_theme_manager_tokens(&manager));
}

static void test_manager_lifecycle_guards(void)
{
    espui_theme_manager_t manager;
    memset(&manager, 0, sizeof(manager));
    espui_view_model_t model;
    espui_view_model_init(&model);

    TEST_ASSERT_EQ_INT(ESPUI_ERR_NOT_INITIALIZED,
                       espui_theme_manager_apply(&manager, "theme_a",
                                                 ESPUI_SCENE_AMBIENT, &model));
    TEST_ASSERT_EQ_INT(ESPUI_THEME_SWITCH_NOT_READY,
                       manager.last_report.status);

    TEST_ASSERT_NOT_NULL(espui_theme_switch_status_name(
        ESPUI_THEME_SWITCH_FALLBACK_USED));
    TEST_ASSERT_NOT_NULL(
        espui_theme_switch_reason_name(ESPUI_THEME_REASON_INIT_FAILED));
}

void test_theme_manager_run(void)
{
    test_renderer_registry();
    test_theme_registry();
    test_apply_and_switch_preserves_model();
    test_same_theme_updates_without_reinit();
    test_unknown_theme_falls_back();
    test_incompatible_theme_rejected();
    test_init_failure_falls_back();
    test_fallback_failure_is_reported();
    test_manager_lifecycle_guards();
}
