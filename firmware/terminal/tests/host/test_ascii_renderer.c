#include <string.h>

#include "test_assert.h"
#include "animation_scheduler.h"
#include "ascii_canvas.h"
#include "ascii_renderer.h"
#include "espui_motion.h"
#include "espui_scene.h"

static espui_ascii_renderer_t g_renderer;
static espui_ascii_renderer_t g_renderer_b;

static espui_ascii_config_t make_config(void)
{
    espui_ascii_config_t config = {0};
    config.cols = 48;
    config.rows = 18;
    config.glyph_ramp = ESPUI_ASCII_DEFAULT_RAMP;
    config.target_fps = 15;
    config.completed_pulse_ms = 1000;
    config.motion_profile = espui_motion_profile_find("phosphor_default");
    config.density_percent = 100;
    config.animation_speed_percent = 100;
    return config;
}

static void model_init_idle(espui_view_model_t *model)
{
    espui_view_model_init(model);
    model->node.status = ESPUI_NODE_ONLINE;
    model->node.connected = true;
    strcpy(model->node.node_id, "node-demo");
    strcpy(model->node.display_name, "Demo Node");
    espui_agent_view_t *agent =
        espui_view_model_upsert_agent(model, "opencode-main");
    if (agent != NULL) {
        strcpy(agent->display_name, "OpenCode Main");
        agent->status = ESPUI_AGENT_IDLE;
    }
}

static void model_set_working(espui_view_model_t *model)
{
    espui_agent_view_t *agent =
        espui_view_model_upsert_agent(model, "opencode-main");
    agent->status = ESPUI_AGENT_BUSY;
    agent->active_session_count = 1;
    espui_session_view_t *session =
        espui_view_model_upsert_session(model, "ses-demo");
    session->status = ESPUI_SESSION_RUNNING;
    strcpy(session->agent_id, "opencode-main");
    strcpy(session->title, "MeetAgent Terminal");
}

static void model_set_approval(espui_view_model_t *model)
{
    model_set_working(model);
    (void)espui_view_model_push_attention(model, "per-demo",
                                          ESPUI_ATTENTION_APPROVAL);
}

static void model_set_error(espui_view_model_t *model)
{
    espui_agent_view_t *agent =
        espui_view_model_upsert_agent(model, "opencode-main");
    agent->status = ESPUI_AGENT_ERROR;
    (void)espui_view_model_push_attention(model, "err-demo",
                                          ESPUI_ATTENTION_ERROR);
}

static void model_set_completed(espui_view_model_t *model)
{
    espui_agent_view_t *agent =
        espui_view_model_upsert_agent(model, "opencode-main");
    agent->status = ESPUI_AGENT_IDLE;
    (void)espui_view_model_push_attention(model, "done-demo",
                                          ESPUI_ATTENTION_COMPLETION);
}

static void model_set_offline(espui_view_model_t *model)
{
    model->node.status = ESPUI_NODE_OFFLINE;
    model->node.connected = false;
    espui_agent_view_t *agent =
        espui_view_model_upsert_agent(model, "opencode-main");
    agent->status = ESPUI_AGENT_OFFLINE;
}

static uint32_t sum_region(const espui_ascii_frame_t *frame, int x0, int y0,
                           int x1, int y1)
{
    uint32_t sum = 0;
    for (int y = y0; y <= y1; ++y) {
        for (int x = x0; x <= x1; ++x) {
            sum += frame->intensity[y][x];
        }
    }
    return sum;
}

static int count_color(const espui_ascii_frame_t *frame,
                       espui_ascii_color_t color)
{
    int count = 0;
    for (int y = 0; y < frame->rows; ++y) {
        for (int x = 0; x < frame->cols; ++x) {
            if (frame->color[y][x] == (uint8_t)color) {
                ++count;
            }
        }
    }
    return count;
}

static bool frame_contains(const espui_ascii_frame_t *frame, const char *text)
{
    for (int y = 0; y < frame->rows; ++y) {
        if (strstr(frame->cells[y], text) != NULL) {
            return true;
        }
    }
    return false;
}

static void test_init_and_dimensions(void)
{
    espui_ascii_renderer_t *renderer = &g_renderer;
    TEST_ASSERT_EQ_INT(ESP_ERR_INVALID_ARG,
                       espui_ascii_renderer_init(NULL, NULL));
    TEST_ASSERT_EQ_INT(ESP_OK, espui_ascii_renderer_init(renderer, NULL));
    TEST_ASSERT_TRUE(renderer->initialized);
    TEST_ASSERT_EQ_INT(ESPUI_ASCII_DEFAULT_COLS, renderer->frame.cols);
    TEST_ASSERT_EQ_INT(ESPUI_ASCII_DEFAULT_ROWS, renderer->frame.rows);
    TEST_ASSERT_EQ_INT(10, renderer->glyph_count);
    TEST_ASSERT_EQ_INT(ESPUI_ASCII_DEFAULT_FPS, renderer->target_fps);
    TEST_ASSERT_STR_EQ(ESPUI_ASCII_DEFAULT_RAMP, renderer->glyphs);
    espui_ascii_renderer_deinit(renderer);

    espui_ascii_config_t config = make_config();
    TEST_ASSERT_EQ_INT(ESP_OK, espui_ascii_renderer_init(renderer, &config));
    TEST_ASSERT_EQ_INT(48, renderer->frame.cols);
    TEST_ASSERT_EQ_INT(18, renderer->frame.rows);
    TEST_ASSERT_EQ_INT(1000, renderer->completed_pulse_ms);
    TEST_ASSERT_EQ_INT(100, renderer->motion[ESPUI_VISUAL_WORKING].speed_percent);
    TEST_ASSERT_EQ_INT(80, renderer->motion[ESPUI_VISUAL_WORKING].density_percent);
    TEST_ASSERT_EQ_INT(70, renderer->motion[ESPUI_VISUAL_WORKING].amplitude_percent);

    TEST_ASSERT_EQ_INT(' ', espui_ascii_glyph_for_intensity(renderer, 0));
    TEST_ASSERT_EQ_INT('=', espui_ascii_glyph_for_intensity(renderer, 50));
    TEST_ASSERT_EQ_INT('@', espui_ascii_glyph_for_intensity(renderer, 100));
    TEST_ASSERT_STR_EQ("idle", espui_visual_state_name(ESPUI_VISUAL_IDLE));
    TEST_ASSERT_STR_EQ("WORKING", espui_visual_state_label(ESPUI_VISUAL_WORKING));
    TEST_ASSERT_EQ_INT(ESPUI_ASCII_COLOR_WARNING,
                       espui_ascii_state_color(ESPUI_VISUAL_APPROVAL));
    TEST_ASSERT_EQ_INT(ESPUI_ASCII_COLOR_ERROR,
                       espui_ascii_state_color(ESPUI_VISUAL_ERROR));
    TEST_ASSERT_EQ_INT(ESPUI_ASCII_COLOR_MUTED,
                       espui_ascii_state_color(ESPUI_VISUAL_OFFLINE));

    espui_view_model_t model;
    model_init_idle(&model);
    TEST_ASSERT_EQ_INT(ESP_OK,
                       espui_ascii_renderer_apply_view_model(renderer, &model,
                                                             ESPUI_SCENE_AMBIENT));
    TEST_ASSERT_TRUE(renderer->frame_count == 1);
    for (int y = 0; y < renderer->frame.rows; ++y) {
        TEST_ASSERT_EQ_INT('\0', renderer->frame.cells[y][renderer->frame.cols]);
        TEST_ASSERT_TRUE(strlen(renderer->frame.cells[y]) <=
                         renderer->frame.cols);
    }
    TEST_ASSERT_TRUE(frame_contains(&renderer->frame, "MEETAGENT"));
    TEST_ASSERT_TRUE(frame_contains(&renderer->frame, "IDLE"));
    TEST_ASSERT_TRUE(frame_contains(&renderer->frame, "SYSTEM IDLE"));

    config = make_config();
    config.cols = 10;
    TEST_ASSERT_EQ_INT(ESP_ERR_INVALID_ARG,
                       espui_ascii_renderer_init(renderer, &config));
    config = make_config();
    config.rows = ESPUI_ASCII_MAX_ROWS + 1;
    TEST_ASSERT_EQ_INT(ESP_ERR_INVALID_ARG,
                       espui_ascii_renderer_init(renderer, &config));
    config = make_config();
    config.glyph_ramp = "x";
    TEST_ASSERT_EQ_INT(ESP_ERR_INVALID_ARG,
                       espui_ascii_renderer_init(renderer, &config));
    config = make_config();
    config.glyph_ramp = "...............x";
    TEST_ASSERT_EQ_INT(ESP_ERR_INVALID_ARG,
                       espui_ascii_renderer_init(renderer, &config));
    espui_ascii_renderer_deinit(renderer);
}

static void test_state_resolution_priority(void)
{
    espui_view_model_t model;

    TEST_ASSERT_EQ_INT(ESPUI_VISUAL_OFFLINE,
                       espui_ascii_resolve_visual_state(NULL, ESPUI_SCENE_AMBIENT));

    model_init_idle(&model);
    TEST_ASSERT_EQ_INT(ESPUI_VISUAL_IDLE,
                       espui_ascii_resolve_visual_state(&model,
                                                        ESPUI_SCENE_AMBIENT));

    model.node.status = ESPUI_NODE_SYNCING;
    TEST_ASSERT_EQ_INT(ESPUI_VISUAL_WORKING,
                       espui_ascii_resolve_visual_state(&model,
                                                        ESPUI_SCENE_AMBIENT));
    model.node.status = ESPUI_NODE_ONLINE;

    model_set_working(&model);
    TEST_ASSERT_EQ_INT(ESPUI_VISUAL_WORKING,
                       espui_ascii_resolve_visual_state(&model,
                                                        ESPUI_SCENE_AMBIENT));

    model_set_approval(&model);
    TEST_ASSERT_EQ_INT(ESPUI_VISUAL_APPROVAL,
                       espui_ascii_resolve_visual_state(&model,
                                                        ESPUI_SCENE_ATTENTION));
    model_set_error(&model);
    /* Approval is safety-relevant and outranks a simultaneous error. */
    TEST_ASSERT_EQ_INT(ESPUI_VISUAL_APPROVAL,
                       espui_ascii_resolve_visual_state(&model,
                                                        ESPUI_SCENE_ATTENTION));

    (void)espui_view_model_resolve_attention(&model, "per-demo");
    TEST_ASSERT_EQ_INT(ESPUI_VISUAL_ERROR,
                       espui_ascii_resolve_visual_state(&model,
                                                        ESPUI_SCENE_ATTENTION));

    (void)espui_view_model_resolve_attention(&model, "err-demo");
    model_set_completed(&model);
    TEST_ASSERT_EQ_INT(ESPUI_VISUAL_COMPLETED,
                       espui_ascii_resolve_visual_state(&model,
                                                        ESPUI_SCENE_ATTENTION));

    (void)espui_view_model_resolve_attention(&model, "done-demo");
    model_set_working(&model);
    TEST_ASSERT_EQ_INT(ESPUI_VISUAL_WORKING,
                       espui_ascii_resolve_visual_state(&model,
                                                        ESPUI_SCENE_FOCUS));

    model_set_offline(&model);
    TEST_ASSERT_EQ_INT(ESPUI_VISUAL_OFFLINE,
                       espui_ascii_resolve_visual_state(&model,
                                                        ESPUI_SCENE_AMBIENT));

    model_init_idle(&model);
    espui_ascii_status_t status =
        espui_ascii_status_from_view_model(&model, ESPUI_SCENE_AMBIENT);
    TEST_ASSERT_EQ_INT(ESPUI_VISUAL_IDLE, status.state);
    TEST_ASSERT_EQ_INT(ESPUI_NODE_ONLINE, status.node_status);
    TEST_ASSERT_TRUE(status.connected);
    TEST_ASSERT_EQ_INT(1, status.agent_count);
    TEST_ASSERT_EQ_INT(0, status.approval_count);
    TEST_ASSERT_EQ_INT(0, status.active_sessions);
}

static void test_motion_scaling(void)
{
    espui_ascii_renderer_t *renderer = &g_renderer;
    espui_ascii_config_t config = make_config();
    TEST_ASSERT_EQ_INT(ESP_OK, espui_ascii_renderer_init(renderer, &config));

    TEST_ASSERT_EQ_INT(25, renderer->motion[ESPUI_VISUAL_IDLE].speed_percent);
    TEST_ASSERT_EQ_INT(25,
                       renderer->motion[ESPUI_VISUAL_IDLE].density_percent);
    TEST_ASSERT_EQ_INT(18,
                       renderer->motion[ESPUI_VISUAL_IDLE].amplitude_percent);
    TEST_ASSERT_EQ_INT(35,
                       renderer->motion[ESPUI_VISUAL_APPROVAL].speed_percent);
    TEST_ASSERT_EQ_INT(60,
                       renderer->motion[ESPUI_VISUAL_COMPLETED].speed_percent);
    TEST_ASSERT_EQ_INT(0,
                       renderer->motion[ESPUI_VISUAL_OFFLINE].speed_percent);
    TEST_ASSERT_EQ_INT(0,
                       renderer->motion[ESPUI_VISUAL_OFFLINE].amplitude_percent);

    TEST_ASSERT_EQ_INT(ESP_OK,
                       espui_ascii_renderer_set_user_speed(renderer, 50));
    TEST_ASSERT_EQ_INT(50,
                       renderer->motion[ESPUI_VISUAL_WORKING].speed_percent);
    TEST_ASSERT_EQ_INT(12,
                       renderer->motion[ESPUI_VISUAL_IDLE].speed_percent);
    TEST_ASSERT_EQ_INT(ESP_OK,
                       espui_ascii_renderer_set_density(renderer, 50));
    TEST_ASSERT_EQ_INT(40,
                       renderer->motion[ESPUI_VISUAL_WORKING].density_percent);
    TEST_ASSERT_EQ_INT(ESP_OK, espui_ascii_renderer_set_motion_profile(
                                   renderer, &espui_motion_profile_none));
    for (int i = 0; i < ESPUI_VISUAL_STATE_COUNT; ++i) {
        TEST_ASSERT_EQ_INT(0, renderer->motion[i].speed_percent);
        TEST_ASSERT_EQ_INT(0, renderer->motion[i].amplitude_percent);
    }
    TEST_ASSERT_EQ_INT(ESP_ERR_INVALID_ARG,
                       espui_ascii_renderer_set_motion_profile(renderer, NULL));
    espui_ascii_renderer_deinit(renderer);
}

static void test_geometry_and_density(void)
{
    espui_ascii_renderer_t *renderer = &g_renderer;
    espui_ascii_config_t config = make_config();
    TEST_ASSERT_EQ_INT(ESP_OK, espui_ascii_renderer_init(renderer, &config));

    espui_view_model_t idle;
    model_init_idle(&idle);
    TEST_ASSERT_EQ_INT(ESP_OK, espui_ascii_renderer_apply_view_model(
                                   renderer, &idle, ESPUI_SCENE_AMBIENT));
    uint32_t idle_sum = sum_region(&renderer->frame, 3, 3,
                                   renderer->frame.cols - 4,
                                   renderer->frame.rows - 4);

    espui_view_model_t working;
    model_init_idle(&working);
    model_set_working(&working);
    TEST_ASSERT_EQ_INT(ESP_OK, espui_ascii_renderer_apply_view_model(
                                   renderer, &working, ESPUI_SCENE_FOCUS));
    uint32_t working_sum = sum_region(&renderer->frame, 3, 3,
                                      renderer->frame.cols - 4,
                                      renderer->frame.rows - 4);
    TEST_ASSERT_TRUE(working_sum > idle_sum);
    TEST_ASSERT_TRUE(frame_contains(&renderer->frame, "SYSTEM ACTIVE"));
    TEST_ASSERT_TRUE(frame_contains(&renderer->frame, "WORKING"));
    TEST_ASSERT_TRUE(count_color(&renderer->frame, ESPUI_ASCII_COLOR_MUTED) > 0);

    espui_view_model_t approval;
    model_init_idle(&approval);
    model_set_approval(&approval);
    TEST_ASSERT_EQ_INT(ESP_OK, espui_ascii_renderer_apply_view_model(
                                   renderer, &approval, ESPUI_SCENE_ATTENTION));
    TEST_ASSERT_TRUE(frame_contains(&renderer->frame, "APPROVAL REQUIRED"));
    TEST_ASSERT_TRUE(count_color(&renderer->frame, ESPUI_ASCII_COLOR_WARNING) >
                     0);

    espui_view_model_t error;
    model_init_idle(&error);
    model_set_error(&error);
    TEST_ASSERT_EQ_INT(ESP_OK, espui_ascii_renderer_apply_view_model(
                                   renderer, &error, ESPUI_SCENE_ATTENTION));
    TEST_ASSERT_TRUE(frame_contains(&renderer->frame, "SYSTEM ERROR"));
    TEST_ASSERT_TRUE(count_color(&renderer->frame, ESPUI_ASCII_COLOR_ERROR) > 0);

    espui_view_model_t completed;
    model_init_idle(&completed);
    model_set_completed(&completed);
    TEST_ASSERT_EQ_INT(ESP_OK, espui_ascii_renderer_apply_view_model(
                                   renderer, &completed, ESPUI_SCENE_ATTENTION));
    TEST_ASSERT_TRUE(frame_contains(&renderer->frame, "SESSION COMPLETED"));
    TEST_ASSERT_TRUE(
        count_color(&renderer->frame, ESPUI_ASCII_COLOR_HIGHLIGHT) > 0);

    espui_view_model_t offline;
    model_init_idle(&offline);
    model_set_offline(&offline);
    TEST_ASSERT_EQ_INT(ESP_OK, espui_ascii_renderer_apply_view_model(
                                   renderer, &offline, ESPUI_SCENE_AMBIENT));
    TEST_ASSERT_TRUE(frame_contains(&renderer->frame, "NODE OFFLINE"));
    TEST_ASSERT_TRUE(count_color(&renderer->frame, ESPUI_ASCII_COLOR_MUTED) > 0);

    espui_ascii_renderer_deinit(renderer);
}

static void test_completed_pulse_decay(void)
{
    espui_ascii_renderer_t *renderer = &g_renderer;
    espui_ascii_config_t config = make_config();
    TEST_ASSERT_EQ_INT(ESP_OK, espui_ascii_renderer_init(renderer, &config));

    espui_view_model_t model;
    model_init_idle(&model);
    model_set_completed(&model);
    TEST_ASSERT_EQ_INT(ESP_OK, espui_ascii_renderer_apply_view_model(
                                   renderer, &model, ESPUI_SCENE_ATTENTION));
    uint32_t initial = sum_region(&renderer->frame, 3, 3,
                                  renderer->frame.cols - 4,
                                  renderer->frame.rows - 4);

    espui_ascii_renderer_advance(renderer, 500);
    TEST_ASSERT_EQ_INT(ESP_OK, espui_ascii_renderer_render(renderer));
    uint32_t mid = sum_region(&renderer->frame, 3, 3,
                              renderer->frame.cols - 4,
                              renderer->frame.rows - 4);

    espui_ascii_renderer_advance(renderer, 700);
    TEST_ASSERT_EQ_INT(ESP_OK, espui_ascii_renderer_render(renderer));
    uint32_t late = sum_region(&renderer->frame, 3, 3,
                               renderer->frame.cols - 4,
                               renderer->frame.rows - 4);

    TEST_ASSERT_TRUE(initial > mid);
    TEST_ASSERT_TRUE(mid > late);
    TEST_ASSERT_TRUE(frame_contains(&renderer->frame, "SESSION COMPLETED"));
    espui_ascii_renderer_deinit(renderer);
}

static void test_offline_is_static(void)
{
    espui_ascii_renderer_t *renderer = &g_renderer;
    espui_ascii_config_t config = make_config();
    TEST_ASSERT_EQ_INT(ESP_OK, espui_ascii_renderer_init(renderer, &config));

    espui_view_model_t model;
    model_init_idle(&model);
    model_set_offline(&model);
    TEST_ASSERT_EQ_INT(ESP_OK, espui_ascii_renderer_apply_view_model(
                                   renderer, &model, ESPUI_SCENE_AMBIENT));

    static espui_ascii_frame_t snapshot;
    snapshot = renderer->frame;
    espui_ascii_renderer_advance(renderer, 1000);
    TEST_ASSERT_EQ_INT(ESP_OK, espui_ascii_renderer_render(renderer));
    TEST_ASSERT_EQ_INT(0, memcmp(snapshot.cells, renderer->frame.cells,
                                 sizeof(snapshot.cells)));
    TEST_ASSERT_EQ_INT(0, memcmp(snapshot.intensity, renderer->frame.intensity,
                                 sizeof(snapshot.intensity)));
    TEST_ASSERT_EQ_INT(0, memcmp(snapshot.color, renderer->frame.color,
                                 sizeof(snapshot.color)));
    espui_ascii_renderer_deinit(renderer);
}

static void test_determinism_and_multiframe(void)
{
    espui_ascii_config_t config = make_config();
    TEST_ASSERT_EQ_INT(ESP_OK,
                       espui_ascii_renderer_init(&g_renderer, &config));
    TEST_ASSERT_EQ_INT(ESP_OK,
                       espui_ascii_renderer_init(&g_renderer_b, &config));

    espui_view_model_t model;
    model_init_idle(&model);
    model_set_working(&model);

    TEST_ASSERT_EQ_INT(ESP_OK, espui_ascii_renderer_apply_view_model(
                                   &g_renderer, &model, ESPUI_SCENE_FOCUS));
    static espui_ascii_frame_t first;
    first = g_renderer.frame;
    espui_ascii_renderer_advance(&g_renderer, 400);
    TEST_ASSERT_EQ_INT(ESP_OK, espui_ascii_renderer_render(&g_renderer));
    TEST_ASSERT_TRUE(memcmp(first.cells, g_renderer.frame.cells,
                            sizeof(first.cells)) != 0);

    TEST_ASSERT_EQ_INT(ESP_OK, espui_ascii_renderer_apply_view_model(
                                   &g_renderer_b, &model, ESPUI_SCENE_FOCUS));
    espui_ascii_renderer_advance(&g_renderer_b, 400);
    TEST_ASSERT_EQ_INT(ESP_OK, espui_ascii_renderer_render(&g_renderer_b));
    TEST_ASSERT_EQ_INT(0, memcmp(g_renderer.frame.cells, g_renderer_b.frame.cells,
                                 sizeof(g_renderer.frame.cells)));
    TEST_ASSERT_EQ_INT(0, memcmp(g_renderer.frame.intensity,
                                 g_renderer_b.frame.intensity,
                                 sizeof(g_renderer.frame.intensity)));

    espui_ascii_renderer_deinit(&g_renderer);
    espui_ascii_renderer_deinit(&g_renderer_b);
}

static void test_scheduler_binding(void)
{
    espui_animation_scheduler_t scheduler;
    espui_animation_scheduler_init(&scheduler);
    espui_ascii_config_t config = make_config();
    TEST_ASSERT_EQ_INT(ESP_OK,
                       espui_ascii_renderer_init(&g_renderer, &config));

    espui_view_model_t model;
    model_init_idle(&model);
    model_set_working(&model);
    TEST_ASSERT_EQ_INT(ESP_OK, espui_ascii_renderer_apply_view_model(
                                   &g_renderer, &model, ESPUI_SCENE_FOCUS));

    int32_t slot = espui_ascii_renderer_bind_scheduler(&g_renderer, &scheduler);
    TEST_ASSERT_TRUE(slot >= 0);
    uint32_t before = g_renderer.frame_count;
    TEST_ASSERT_EQ_INT(1, espui_animation_scheduler_tick(&scheduler, 66));
    TEST_ASSERT_EQ_INT(before + 1, g_renderer.frame_count);

    espui_animation_scheduler_set_paused(&scheduler, true);
    before = g_renderer.frame_count;
    TEST_ASSERT_EQ_INT(0, espui_animation_scheduler_tick(&scheduler, 1000));
    TEST_ASSERT_EQ_INT(before, g_renderer.frame_count);

    espui_animation_scheduler_set_paused(&scheduler, false);
    TEST_ASSERT_EQ_INT(1, espui_animation_scheduler_tick(&scheduler, 66));
    TEST_ASSERT_EQ_INT(before + 1, g_renderer.frame_count);

    TEST_ASSERT_TRUE(
        espui_ascii_renderer_unbind_scheduler(&g_renderer, &scheduler));
    before = g_renderer.frame_count;
    TEST_ASSERT_EQ_INT(0, espui_animation_scheduler_tick(&scheduler, 66));
    TEST_ASSERT_EQ_INT(before, g_renderer.frame_count);
    TEST_ASSERT_FALSE(
        espui_ascii_renderer_unbind_scheduler(&g_renderer, &scheduler));

    espui_ascii_renderer_deinit(&g_renderer);
    TEST_ASSERT_EQ_INT(-1, espui_ascii_renderer_bind_scheduler(NULL, NULL));
}

typedef struct {
    espui_ascii_renderer_t *renderer;
    espui_view_model_t *model;
} scene_bridge_t;

static void scene_bridge_callback(espui_scene_id_t scene, void *user_ctx)
{
    scene_bridge_t *bridge = (scene_bridge_t *)user_ctx;
    (void)espui_ascii_renderer_apply_view_model(bridge->renderer, bridge->model,
                                                scene);
}

static void test_scene_manager_integration(void)
{
    espui_ascii_config_t config = make_config();
    TEST_ASSERT_EQ_INT(ESP_OK,
                       espui_ascii_renderer_init(&g_renderer, &config));

    espui_view_model_t model;
    model_init_idle(&model);
    model_set_approval(&model);

    scene_bridge_t bridge = {.renderer = &g_renderer, .model = &model};
    espui_scene_manager_t scenes;
    espui_scene_manager_init(&scenes, scene_bridge_callback, &bridge);

    TEST_ASSERT_EQ_INT(ESP_OK,
                       espui_scene_manager_set_attention_count(&scenes, 1));
    TEST_ASSERT_EQ_INT(ESPUI_SCENE_ATTENTION, scenes.current);
    TEST_ASSERT_EQ_INT(ESPUI_VISUAL_APPROVAL, g_renderer.status.state);
    TEST_ASSERT_TRUE(frame_contains(&g_renderer.frame, "APPROVAL REQUIRED"));

    (void)espui_view_model_resolve_attention(&model, "per-demo");
    TEST_ASSERT_EQ_INT(ESP_OK,
                       espui_scene_manager_set_attention_count(&scenes, 0));
    TEST_ASSERT_EQ_INT(ESPUI_SCENE_AMBIENT, scenes.current);
    TEST_ASSERT_EQ_INT(ESPUI_VISUAL_WORKING, g_renderer.status.state);

    espui_ascii_renderer_deinit(&g_renderer);
}

static void test_canvas_clipping_and_text(void)
{
    espui_ascii_frame_t frame;
    memset(&frame, 0, sizeof(frame));
    frame.cols = 12;
    frame.rows = 5;
    espui_ascii_canvas_clear(&frame);

    espui_ascii_canvas_text(&frame, -3, 1, "ABC", ESPUI_ASCII_COLOR_PRIMARY,
                            255);
    TEST_ASSERT_EQ_INT(' ', frame.cells[1][0]);
    espui_ascii_canvas_text(&frame, 10, 1, "XYZ", ESPUI_ASCII_COLOR_PRIMARY,
                            255);
    TEST_ASSERT_EQ_INT('X', frame.cells[1][10]);
    TEST_ASSERT_EQ_INT('Y', frame.cells[1][11]);
    TEST_ASSERT_EQ_INT('\0', frame.cells[1][12]);

    espui_ascii_canvas_text(&frame, 0, 0, "AB", ESPUI_ASCII_COLOR_HIGHLIGHT, 255);
    espui_ascii_canvas_put(&frame, 0, 0, '#', ESPUI_ASCII_COLOR_ERROR, 10);
    TEST_ASSERT_EQ_INT('A', frame.cells[0][0]);
    TEST_ASSERT_EQ_INT(ESPUI_ASCII_COLOR_HIGHLIGHT, frame.color[0][0]);

    espui_ascii_canvas_line(&frame, 0, 2, 11, 2, ESPUI_ASCII_COLOR_SECONDARY,
                            100);
    TEST_ASSERT_EQ_INT('-', frame.cells[2][5]);
    espui_ascii_canvas_ring(&frame, 6, 2, 2, '*', 0, 0, 0,
                            ESPUI_ASCII_COLOR_PRIMARY, 100);
    espui_ascii_canvas_box(&frame, 0, 0, 12, 5, 2, ESPUI_ASCII_COLOR_MUTED,
                           90);
    TEST_ASSERT_EQ_INT('+', frame.cells[0][11]);
    TEST_ASSERT_EQ_INT('|', frame.cells[1][0]);
    TEST_ASSERT_EQ_INT(' ', frame.cells[0][5]);
    espui_ascii_canvas_scanline(&frame, 3, ':', ESPUI_ASCII_COLOR_MUTED, 50, 2);
    TEST_ASSERT_EQ_INT(':', frame.cells[3][1]);
}

void test_ascii_renderer_run(void)
{
    test_init_and_dimensions();
    test_state_resolution_priority();
    test_motion_scaling();
    test_geometry_and_density();
    test_completed_pulse_decay();
    test_offline_is_static();
    test_determinism_and_multiframe();
    test_scheduler_binding();
    test_scene_manager_integration();
    test_canvas_clipping_and_text();
}
