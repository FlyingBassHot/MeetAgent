#include <string.h>

#include "espui_theme.h"
#include "espui_view_model.h"
#include "phosphor_lvgl_port.h"
#include "test_assert.h"

void host_espui_stub_set_renderer(const espui_renderer_ops_t *ops);
void host_espui_stub_set_model(const espui_view_model_t *model);
void host_espui_stub_set_focus_index(uint16_t index);
void host_phosphor_stub_reset(void);
int host_phosphor_stub_attach_calls(void);
int host_phosphor_stub_detach_calls(void);
bool host_phosphor_stub_active(void);

#define FAKE_MAX_WIDGETS 64
#define FAKE_TEXT_MAX 256

typedef struct {
    espui_widget_kind_t kind;
    espui_widget_spec_t spec;
    bool visible;
    char text[FAKE_TEXT_MAX];
    void *buffer;
    uint16_t buffer_w;
    uint16_t buffer_h;
} fake_widget_t;

typedef struct {
    int unused;
} fake_scene_t;

static fake_widget_t s_widgets[FAKE_MAX_WIDGETS];
static int s_widget_count;
static int s_scene_create_count;
static int s_scene_destroy_count;
static int s_scene_commit_count;
static int s_canvas_count;
static int s_canvas_buffer_count;
static int s_canvas_invalidate_count;
static fake_scene_t s_scene;

static void fake_reset(void)
{
    memset(s_widgets, 0, sizeof(s_widgets));
    s_widget_count = 0;
    s_scene_create_count = 0;
    s_scene_destroy_count = 0;
    s_scene_commit_count = 0;
    s_canvas_count = 0;
    s_canvas_buffer_count = 0;
    s_canvas_invalidate_count = 0;
}

static esp_err_t fake_init(void)
{
    return ESP_OK;
}

static void fake_deinit(void)
{
}

static esp_err_t fake_scene_create(espui_ui_scene_t **out_scene)
{
    if (out_scene == NULL) {
        return ESP_ERR_INVALID_ARG;
    }
    ++s_scene_create_count;
    *out_scene = (espui_ui_scene_t *)&s_scene;
    return ESP_OK;
}

static void fake_scene_destroy(espui_ui_scene_t *scene)
{
    (void)scene;
    ++s_scene_destroy_count;
}

static esp_err_t fake_scene_commit(espui_ui_scene_t *scene)
{
    (void)scene;
    ++s_scene_commit_count;
    return ESP_OK;
}

static espui_ui_widget_t *fake_widget_create(espui_ui_scene_t *scene,
                                             espui_widget_kind_t kind,
                                             const espui_widget_spec_t *spec)
{
    (void)scene;
    if (spec == NULL || s_widget_count >= FAKE_MAX_WIDGETS) {
        return NULL;
    }
    fake_widget_t *widget = &s_widgets[s_widget_count++];
    memset(widget, 0, sizeof(*widget));
    widget->kind = kind;
    widget->spec = *spec;
    widget->visible = spec->visible;
    if (kind == ESPUI_WIDGET_CANVAS) {
        ++s_canvas_count;
    }
    return (espui_ui_widget_t *)widget;
}

static esp_err_t fake_widget_set_text(espui_ui_widget_t *widget,
                                      const char *text)
{
    fake_widget_t *fake = (fake_widget_t *)widget;
    if (fake == NULL || text == NULL || fake->kind != ESPUI_WIDGET_LABEL) {
        return ESP_ERR_INVALID_ARG;
    }
    snprintf(fake->text, sizeof(fake->text), "%s", text);
    return ESP_OK;
}

static esp_err_t fake_widget_set_spec(espui_ui_widget_t *widget,
                                      const espui_widget_spec_t *spec)
{
    fake_widget_t *fake = (fake_widget_t *)widget;
    if (fake == NULL || spec == NULL) {
        return ESP_ERR_INVALID_ARG;
    }
    fake->spec = *spec;
    return ESP_OK;
}

static esp_err_t fake_widget_set_visible(espui_ui_widget_t *widget,
                                         bool visible)
{
    fake_widget_t *fake = (fake_widget_t *)widget;
    if (fake == NULL) {
        return ESP_ERR_INVALID_ARG;
    }
    fake->visible = visible;
    return ESP_OK;
}

static esp_err_t fake_canvas_set_buffer(espui_ui_widget_t *widget,
                                        void *buffer, uint16_t width,
                                        uint16_t height)
{
    fake_widget_t *fake = (fake_widget_t *)widget;
    if (fake == NULL || fake->kind != ESPUI_WIDGET_CANVAS || buffer == NULL) {
        return ESP_ERR_INVALID_ARG;
    }
    fake->buffer = buffer;
    fake->buffer_w = width;
    fake->buffer_h = height;
    ++s_canvas_buffer_count;
    return ESP_OK;
}

static esp_err_t fake_canvas_invalidate(espui_ui_widget_t *widget)
{
    fake_widget_t *fake = (fake_widget_t *)widget;
    if (fake == NULL || fake->kind != ESPUI_WIDGET_CANVAS) {
        return ESP_ERR_INVALID_ARG;
    }
    ++s_canvas_invalidate_count;
    return ESP_OK;
}

static esp_err_t fake_canvas_invalidate_area(espui_ui_widget_t *widget,
                                             int16_t x, int16_t y,
                                             uint16_t width, uint16_t height)
{
    (void)x;
    (void)y;
    (void)width;
    (void)height;
    return fake_canvas_invalidate(widget);
}

static const espui_renderer_ops_t s_fake_ops = {
    .init = fake_init,
    .deinit = fake_deinit,
    .scene_create = fake_scene_create,
    .scene_destroy = fake_scene_destroy,
    .scene_commit = fake_scene_commit,
    .widget_create = fake_widget_create,
    .widget_set_text = fake_widget_set_text,
    .widget_set_spec = fake_widget_set_spec,
    .widget_set_visible = fake_widget_set_visible,
    .widget_canvas_set_buffer = fake_canvas_set_buffer,
    .widget_canvas_invalidate = fake_canvas_invalidate,
    .widget_canvas_invalidate_area = fake_canvas_invalidate_area,
};

static fake_widget_t *find_text(const char *needle)
{
    for (int i = 0; i < s_widget_count; ++i) {
        if (strstr(s_widgets[i].text, needle) != NULL) {
            return &s_widgets[i];
        }
    }
    return NULL;
}

static fake_widget_t *find_kind(espui_widget_kind_t kind)
{
    for (int i = 0; i < s_widget_count; ++i) {
        if (s_widgets[i].kind == kind) {
            return &s_widgets[i];
        }
    }
    return NULL;
}

static void make_model(espui_view_model_t *model)
{
    espui_view_model_init(model);
    model->node.status = ESPUI_NODE_ONLINE;
    model->uptime_ms = 100000;

    espui_agent_view_t *agent =
        espui_view_model_upsert_agent(model, "agent-1");
    TEST_ASSERT_NOT_NULL(agent);
    snprintf(agent->display_name, sizeof(agent->display_name), "Builder");
    agent->status = ESPUI_AGENT_BUSY;

    espui_session_view_t *session =
        espui_view_model_upsert_session(model, "ses-run");
    TEST_ASSERT_NOT_NULL(session);
    snprintf(session->title, sizeof(session->title), "RUNNING TASK");
    snprintf(session->agent_id, sizeof(session->agent_id), "agent-1");
    session->status = ESPUI_SESSION_RUNNING;
    session->duration_seconds = 75;

    session = espui_view_model_upsert_session(model, "ses-wait");
    TEST_ASSERT_NOT_NULL(session);
    snprintf(session->title, sizeof(session->title), "WAITING TASK");
    snprintf(session->agent_id, sizeof(session->agent_id), "agent-1");
    session->status = ESPUI_SESSION_WAITING_APPROVAL;

    session = espui_view_model_upsert_session(model, "ses-err");
    TEST_ASSERT_NOT_NULL(session);
    snprintf(session->title, sizeof(session->title), "FAILED TASK");
    session->status = ESPUI_SESSION_FAILED;

    session = espui_view_model_upsert_session(model, "ses-done");
    TEST_ASSERT_NOT_NULL(session);
    snprintf(session->title, sizeof(session->title), "DONE TASK");
    session->status = ESPUI_SESSION_COMPLETED;

    TEST_ASSERT_NOT_NULL(espui_view_model_push_attention(
        model, "per-1", ESPUI_ATTENTION_APPROVAL));
    TEST_ASSERT_NOT_NULL(espui_view_model_push_attention(
        model, "per-2", ESPUI_ATTENTION_ERROR));
    espui_view_model_recount_attention(model);
    model->attention[0].created_at_ms = 90000;
    snprintf(model->attention[0].summary,
             sizeof(model->attention[0].summary), "read .env");
    snprintf(model->attention[0].session_id,
             sizeof(model->attention[0].session_id), "ses-wait");

    for (int i = 0; i < 10; ++i) {
        char type[ESPUI_ACTIVITY_TYPE_MAX];
        char text[ESPUI_ACTIVITY_TEXT_MAX];
        snprintf(type, sizeof(type), "event.%d", i);
        snprintf(text, sizeof(text), "text-%d", i);
        espui_view_model_push_activity(model, type, "ses-run", text,
                                       (uint32_t)(1000 + i));
    }
}

static void test_terminal_scenes(void)
{
    fake_reset();
    host_phosphor_stub_reset();
    host_espui_stub_set_renderer(&s_fake_ops);
    host_espui_stub_set_focus_index(0);

    espui_view_model_t model;
    make_model(&model);
    host_espui_stub_set_model(&model);

    const espui_theme_descriptor_t *theme = espui_theme_terminal_aesthetic();
    TEST_ASSERT_NOT_NULL(theme);
    TEST_ASSERT_EQ_INT(ESP_OK, theme->ops->init((const espui_context_t *)1));
    TEST_ASSERT_EQ_INT(1, s_scene_create_count);
    TEST_ASSERT_EQ_INT(1, s_canvas_count);
    TEST_ASSERT_EQ_INT(1, s_canvas_buffer_count);
    TEST_ASSERT_EQ_INT(1, host_phosphor_stub_attach_calls());
    int widgets_after_init = s_widget_count;

    TEST_ASSERT_EQ_INT(ESP_OK,
                       theme->ops->enter_scene(ESPUI_SCENE_AMBIENT, &model));
    fake_widget_t *canvas = find_kind(ESPUI_WIDGET_CANVAS);
    TEST_ASSERT_NOT_NULL(canvas);
    TEST_ASSERT_TRUE(canvas->visible);
    TEST_ASSERT_TRUE(host_phosphor_stub_active());
    TEST_ASSERT_EQ_INT(24, canvas->spec.x);
    TEST_ASSERT_EQ_INT(24, canvas->spec.y);
    TEST_ASSERT_EQ_INT(432, canvas->spec.width);
    TEST_ASSERT_EQ_INT(432, canvas->spec.height);
    fake_widget_t *strip = find_text("AGENTS");
    TEST_ASSERT_NOT_NULL(strip);
    TEST_ASSERT_TRUE(strip->visible);
    TEST_ASSERT_EQ_INT(458, strip->spec.y);
    TEST_ASSERT_EQ_INT(18, strip->spec.height);
    TEST_ASSERT_NOT_NULL(find_text("SCENE: ambient"));
    TEST_ASSERT_FALSE(find_text("SCENE: ambient")->visible);

    TEST_ASSERT_EQ_INT(ESP_OK,
                       theme->ops->enter_scene(ESPUI_SCENE_FOCUS, &model));
    TEST_ASSERT_FALSE(canvas->visible);
    TEST_ASSERT_FALSE(host_phosphor_stub_active());
    fake_widget_t *focus = find_text("SESSION: RUNNING TASK");
    TEST_ASSERT_NOT_NULL(focus);
    TEST_ASSERT_TRUE(focus->visible);
    TEST_ASSERT_NOT_NULL(find_text("AGENT: Builder"));
    TEST_ASSERT_NOT_NULL(find_text("[ INTERRUPT: STAGE 3 ]"));
    fake_widget_t *focus_panel = NULL;
    for (int i = 0; i < s_widget_count; ++i) {
        if (s_widgets[i].kind == ESPUI_WIDGET_RECT &&
            s_widgets[i].spec.height == 356) {
            focus_panel = &s_widgets[i];
            break;
        }
    }
    TEST_ASSERT_NOT_NULL(focus_panel);
    TEST_ASSERT_TRUE(focus_panel->visible);
    TEST_ASSERT_EQ_INT(60, focus_panel->spec.y);

    host_espui_stub_set_focus_index(1);
    TEST_ASSERT_EQ_INT(ESP_OK,
                       theme->ops->enter_scene(ESPUI_SCENE_FOCUS, &model));
    TEST_ASSERT_NOT_NULL(find_text("SESSION: WAITING TASK"));
    host_espui_stub_set_focus_index(0);

    TEST_ASSERT_EQ_INT(ESP_OK,
                       theme->ops->enter_scene(ESPUI_SCENE_ACTIVITY, &model));
    fake_widget_t *activity = find_text("T+");
    TEST_ASSERT_NOT_NULL(activity);
    TEST_ASSERT_TRUE(activity->visible);
    TEST_ASSERT_NOT_NULL(strstr(activity->text, "event.9"));
    TEST_ASSERT_NOT_NULL(strstr(activity->text, "event.2"));
    TEST_ASSERT_NULL(strstr(activity->text, "event.1"));

    TEST_ASSERT_EQ_INT(ESP_OK,
                       theme->ops->enter_scene(ESPUI_SCENE_ATTENTION, &model));
    fake_widget_t *list = find_text("read .env");
    TEST_ASSERT_NOT_NULL(list);
    TEST_ASSERT_TRUE(list->visible);
    fake_widget_t *detail = find_text("PERMISSION: per-1");
    TEST_ASSERT_NOT_NULL(detail);
    TEST_ASSERT_TRUE(detail->visible);

    int attach_calls = host_phosphor_stub_attach_calls();
    int detach_calls = host_phosphor_stub_detach_calls();
    for (int i = 0; i < 100; ++i) {
        espui_scene_id_t scene = (espui_scene_id_t)(i % 4);
        TEST_ASSERT_EQ_INT(ESP_OK, theme->ops->enter_scene(scene, &model));
    }
    TEST_ASSERT_EQ_INT(widgets_after_init, s_widget_count);
    TEST_ASSERT_EQ_INT(1, s_canvas_count);
    TEST_ASSERT_EQ_INT(attach_calls, host_phosphor_stub_attach_calls());
    TEST_ASSERT_EQ_INT(detach_calls, host_phosphor_stub_detach_calls());

    theme->ops->destroy();
    TEST_ASSERT_EQ_INT(1, s_scene_destroy_count);
    TEST_ASSERT_EQ_INT(1, host_phosphor_stub_detach_calls());

    /* Safe theme must still initialize and ignore the canvas runtime. */
    fake_reset();
    host_phosphor_stub_reset();
    host_espui_stub_set_renderer(&s_fake_ops);
    const espui_theme_descriptor_t *safe = espui_theme_safe();
    TEST_ASSERT_EQ_INT(ESP_OK, safe->ops->init((const espui_context_t *)1));
    TEST_ASSERT_EQ_INT(ESP_OK,
                       safe->ops->enter_scene(ESPUI_SCENE_AMBIENT, &model));
    TEST_ASSERT_EQ_INT(0, s_canvas_count);
    TEST_ASSERT_EQ_INT(0, host_phosphor_stub_attach_calls());
    safe->ops->destroy();
}

void test_terminal_scenes_run(void)
{
    test_terminal_scenes();
}
