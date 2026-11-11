#include <string.h>

#include "espui_theme.h"
#include "espui_view_model.h"
#include "phosphor_lvgl_port.h"
#include "test_assert.h"

void host_espui_stub_set_renderer(const espui_renderer_ops_t *ops);
void host_espui_stub_set_model(const espui_view_model_t *model);
void host_espui_stub_set_focus_index(uint16_t index);
void host_espui_stub_set_uptime_ms(uint32_t uptime_ms);
void host_espui_stub_set_dispatch_result(esp_err_t result);
void host_espui_stub_reset_actions(void);
int host_espui_stub_action_count(void);
const espui_action_t *host_espui_stub_action_at(int index);
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
        if (s_widgets[i].kind == ESPUI_WIDGET_LABEL &&
            strstr(s_widgets[i].text, needle) != NULL) {
            return &s_widgets[i];
        }
    }
    return NULL;
}

static fake_widget_t *find_visible_text(const char *needle)
{
    fake_widget_t *widget = find_text(needle);
    return widget != NULL && widget->visible ? widget : NULL;
}

static fake_widget_t *find_exact(const char *text)
{
    for (int i = 0; i < s_widget_count; ++i) {
        if (s_widgets[i].kind == ESPUI_WIDGET_LABEL &&
            strcmp(s_widgets[i].text, text) == 0) {
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

static void add_attention(espui_view_model_t *model, const char *permission_id,
                          const char *session_id, const char *summary,
                          espui_attention_kind_t kind)
{
    espui_attention_view_t *attention = espui_view_model_push_attention(
        model, permission_id, kind);
    TEST_ASSERT_NOT_NULL(attention);
    if (attention == NULL) {
        return;
    }
    snprintf(attention->session_id, sizeof(attention->session_id), "%s",
             session_id != NULL ? session_id : "");
    snprintf(attention->summary, sizeof(attention->summary), "%s",
             summary != NULL ? summary : "");
    attention->created_at_ms = 90000;
}

static void make_approval_model(espui_view_model_t *model, int approvals)
{
    espui_view_model_init(model);
    model->node.status = ESPUI_NODE_ONLINE;
    model->uptime_ms = 100000;

    espui_agent_view_t *agent = espui_view_model_upsert_agent(model, "agent-1");
    TEST_ASSERT_NOT_NULL(agent);
    if (agent != NULL) {
        snprintf(agent->display_name, sizeof(agent->display_name), "Builder");
        agent->status = ESPUI_AGENT_BUSY;
    }

    for (int i = 0; i < approvals; ++i) {
        char session_id[ESPUI_ID_MAX];
        char permission_id[ESPUI_ID_MAX];
        snprintf(session_id, sizeof(session_id), "ses-wait-%d", i);
        snprintf(permission_id, sizeof(permission_id), "per-%d", i + 1);
        espui_session_view_t *session =
            espui_view_model_upsert_session(model, session_id);
        TEST_ASSERT_NOT_NULL(session);
        if (session != NULL) {
            snprintf(session->agent_id, sizeof(session->agent_id), "agent-1");
            session->status = ESPUI_SESSION_WAITING_APPROVAL;
        }
        add_attention(model, permission_id, session_id, "read .env",
                      ESPUI_ATTENTION_APPROVAL);
    }
}

static void make_focus_model(espui_view_model_t *model)
{
    espui_view_model_init(model);
    model->node.status = ESPUI_NODE_ONLINE;
    model->uptime_ms = 100000;

    espui_agent_view_t *agent = espui_view_model_upsert_agent(model, "agent-1");
    TEST_ASSERT_NOT_NULL(agent);
    if (agent != NULL) {
        snprintf(agent->display_name, sizeof(agent->display_name), "Builder");
        agent->status = ESPUI_AGENT_BUSY;
    }

    espui_session_view_t *session =
        espui_view_model_upsert_session(model, "ses-run");
    TEST_ASSERT_NOT_NULL(session);
    if (session != NULL) {
        snprintf(session->title, sizeof(session->title), "RUNNING TASK");
        snprintf(session->agent_id, sizeof(session->agent_id), "agent-1");
        session->status = ESPUI_SESSION_RUNNING;
        session->duration_seconds = 75;
    }

    session = espui_view_model_upsert_session(model, "ses-wait");
    TEST_ASSERT_NOT_NULL(session);
    if (session != NULL) {
        snprintf(session->title, sizeof(session->title), "WAITING TASK");
        snprintf(session->agent_id, sizeof(session->agent_id), "agent-1");
        session->status = ESPUI_SESSION_WAITING_APPROVAL;
    }

    session = espui_view_model_upsert_session(model, "ses-err");
    TEST_ASSERT_NOT_NULL(session);
    if (session != NULL) {
        snprintf(session->title, sizeof(session->title), "FAILED TASK");
        session->status = ESPUI_SESSION_FAILED;
    }

    session = espui_view_model_upsert_session(model, "ses-done");
    TEST_ASSERT_NOT_NULL(session);
    if (session != NULL) {
        snprintf(session->title, sizeof(session->title), "DONE TASK");
        session->status = ESPUI_SESSION_COMPLETED;
    }

    add_attention(model, "per-1", "ses-wait", "read .env",
                  ESPUI_ATTENTION_APPROVAL);
    add_attention(model, "err-1", "ses-err", "tool failed",
                  ESPUI_ATTENTION_ERROR);
}

static void make_activity(espui_view_model_t *model, const char *session_id)
{
    for (int i = 0; i < 10; ++i) {
        char type[ESPUI_ACTIVITY_TYPE_MAX];
        char text[ESPUI_ACTIVITY_TEXT_MAX];
        snprintf(type, sizeof(type), "event.%d", i);
        snprintf(text, sizeof(text), "text-%d", i);
        espui_view_model_push_activity(model, type, session_id, text,
                                       (uint32_t)(1000 + i));
    }
}

static const espui_theme_descriptor_t *theme(void)
{
    const espui_theme_descriptor_t *theme = espui_theme_terminal_aesthetic();
    TEST_ASSERT_NOT_NULL(theme);
    return theme;
}

static void setup_theme_with_focus(espui_view_model_t *model,
                                   espui_scene_id_t scene,
                                   uint16_t focus_index)
{
    fake_reset();
    host_phosphor_stub_reset();
    host_espui_stub_set_renderer(&s_fake_ops);
    host_espui_stub_set_model(model);
    host_espui_stub_set_focus_index(focus_index);
    host_espui_stub_set_uptime_ms(0);
    host_espui_stub_reset_actions();

    TEST_ASSERT_EQ_INT(ESP_OK, theme()->ops->init((const espui_context_t *)1));
    TEST_ASSERT_EQ_INT(ESP_OK, theme()->ops->enter_scene(scene, model));
}

static void setup_theme(espui_view_model_t *model, espui_scene_id_t scene)
{
    setup_theme_with_focus(model, scene, 0);
}

static void tap(uint16_t x, uint16_t y)
{
    espui_input_event_t event;
    memset(&event, 0, sizeof(event));
    event.type = ESPUI_INPUT_TAP;
    event.x = x;
    event.y = y;
    event.pressed = true;
    theme()->ops->on_input(&event);
}

static espui_action_result_t permission_result(const char *permission_id,
                                               espui_permission_decision_t dec,
                                               espui_action_status_t status,
                                               const char *message)
{
    espui_action_result_t result;
    memset(&result, 0, sizeof(result));
    result.action = espui_action_permission(permission_id, dec);
    result.status = status;
    snprintf(result.message, sizeof(result.message), "%s",
             message != NULL ? message : "");
    return result;
}

static espui_action_result_t interrupt_result(const char *session_id,
                                              espui_action_status_t status,
                                              const char *message)
{
    espui_action_result_t result;
    memset(&result, 0, sizeof(result));
    result.action = espui_action_interrupt(session_id);
    result.status = status;
    snprintf(result.message, sizeof(result.message), "%s",
             message != NULL ? message : "");
    return result;
}

static void test_scene_layouts_and_commit_policy(void)
{
    espui_view_model_t model;
    make_focus_model(&model);
    make_activity(&model, "ses-run");
    setup_theme(&model, ESPUI_SCENE_AMBIENT);

    TEST_ASSERT_EQ_INT(1, s_scene_create_count);
    TEST_ASSERT_EQ_INT(1, s_canvas_count);
    TEST_ASSERT_EQ_INT(1, s_canvas_buffer_count);
    TEST_ASSERT_EQ_INT(1, host_phosphor_stub_attach_calls());
    TEST_ASSERT_EQ_INT(1, s_scene_commit_count);
    int widgets_after_init = s_widget_count;
    TEST_ASSERT_TRUE(widgets_after_init <= 32);

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

    int commits = s_scene_commit_count;
    theme()->ops->update(&model);
    TEST_ASSERT_EQ_INT(commits, s_scene_commit_count);
    TEST_ASSERT_EQ_INT(widgets_after_init, s_widget_count);

    TEST_ASSERT_EQ_INT(ESP_OK,
                       theme()->ops->enter_scene(ESPUI_SCENE_FOCUS, &model));
    TEST_ASSERT_EQ_INT(commits, s_scene_commit_count);
    TEST_ASSERT_FALSE(canvas->visible);
    TEST_ASSERT_FALSE(host_phosphor_stub_active());
    fake_widget_t *focus = find_text("SESSION: RUNNING TASK");
    TEST_ASSERT_NOT_NULL(focus);
    TEST_ASSERT_TRUE(focus->visible);
    TEST_ASSERT_NOT_NULL(find_text("AGENT: Builder"));
    TEST_ASSERT_NOT_NULL(find_exact("INTERRUPT"));
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

    TEST_ASSERT_EQ_INT(ESP_OK,
                       theme()->ops->enter_scene(ESPUI_SCENE_ACTIVITY, &model));
    TEST_ASSERT_EQ_INT(commits, s_scene_commit_count);
    fake_widget_t *activity = find_text("T+");
    TEST_ASSERT_NOT_NULL(activity);
    TEST_ASSERT_TRUE(activity->visible);
    TEST_ASSERT_NOT_NULL(strstr(activity->text, "event.9"));
    TEST_ASSERT_NOT_NULL(strstr(activity->text, "event.2"));
    TEST_ASSERT_NULL(strstr(activity->text, "event.1"));

    TEST_ASSERT_EQ_INT(ESP_OK,
                       theme()->ops->enter_scene(ESPUI_SCENE_ATTENTION, &model));
    TEST_ASSERT_EQ_INT(commits, s_scene_commit_count);
    fake_widget_t *row = find_text("1/2");
    TEST_ASSERT_NOT_NULL(row);
    TEST_ASSERT_TRUE(row->visible);
    TEST_ASSERT_EQ_INT(106, row->spec.y);
    TEST_ASSERT_NOT_NULL(strstr(row->text, "> 1/2"));
    fake_widget_t *detail = find_text("SUMMARY: read .env");
    TEST_ASSERT_NOT_NULL(detail);
    TEST_ASSERT_TRUE(detail->visible);
    TEST_ASSERT_EQ_INT(240, detail->spec.y);
    TEST_ASSERT_NOT_NULL(find_visible_text("REJECT"));
    TEST_ASSERT_NOT_NULL(find_visible_text("APPROVE ONCE"));
    TEST_ASSERT_NOT_NULL(find_visible_text("APPROVE ALWAYS"));

    espui_action_result_t result =
        interrupt_result("ses-run", ESPUI_ACTION_STATUS_FAILED, "offline");
    int commits_before_result = s_scene_commit_count;
    theme()->ops->on_action_result(&result);
    TEST_ASSERT_EQ_INT(commits_before_result, s_scene_commit_count);

    int widgets = s_widget_count;
    int attach_calls = host_phosphor_stub_attach_calls();
    int detach_calls = host_phosphor_stub_detach_calls();
    for (int i = 0; i < 100; ++i) {
        espui_scene_id_t scene = (espui_scene_id_t)(i % 4);
        TEST_ASSERT_EQ_INT(ESP_OK, theme()->ops->enter_scene(scene, &model));
    }
    TEST_ASSERT_EQ_INT(widgets, s_widget_count);
    TEST_ASSERT_EQ_INT(1, s_canvas_count);
    TEST_ASSERT_EQ_INT(attach_calls, host_phosphor_stub_attach_calls());
    TEST_ASSERT_EQ_INT(detach_calls, host_phosphor_stub_detach_calls());

    theme()->ops->destroy();
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

static void test_attention_selection(void)
{
    espui_view_model_t model;
    make_approval_model(&model, 2);
    setup_theme(&model, ESPUI_SCENE_ATTENTION);

    fake_widget_t *row1 = find_text("> 1/2");
    TEST_ASSERT_NOT_NULL(row1);
    TEST_ASSERT_EQ_INT(106, row1->spec.y);
    fake_widget_t *row2 = find_text("2/2");
    TEST_ASSERT_NOT_NULL(row2);
    TEST_ASSERT_EQ_INT(150, row2->spec.y);
    TEST_ASSERT_NOT_NULL(find_text("SUMMARY: read .env"));
    TEST_ASSERT_EQ_INT(0, host_espui_stub_action_count());

    tap(40, 160);
    row2 = find_text("> 2/2");
    TEST_ASSERT_NOT_NULL(row2);
    TEST_ASSERT_EQ_INT(150, row2->spec.y);
    TEST_ASSERT_NOT_NULL(find_text("ID per-2"));

    /* Selection survives an unrelated ViewModel update. */
    theme()->ops->update(&model);
    TEST_ASSERT_NOT_NULL(find_text("> 2/2"));

    /* Removing the selected permission falls back to the first row. */
    TEST_ASSERT_TRUE(espui_view_model_resolve_attention(&model, "per-2"));
    theme()->ops->update(&model);
    TEST_ASSERT_NOT_NULL(find_text("> 1/1"));
    TEST_ASSERT_NOT_NULL(find_text("ID per-1"));

    /* Three visible rows, fixed 44px pitch, third row selectable. */
    espui_view_model_t three;
    make_approval_model(&three, 3);
    setup_theme(&three, ESPUI_SCENE_ATTENTION);
    fake_widget_t *row3 = find_text("3/3");
    TEST_ASSERT_NOT_NULL(row3);
    TEST_ASSERT_EQ_INT(194, row3->spec.y);
    tap(40, 204);
    TEST_ASSERT_NOT_NULL(find_text("> 3/3"));
    TEST_ASSERT_NOT_NULL(find_text("ID per-3"));
    TEST_ASSERT_EQ_INT(0, host_espui_stub_action_count());
}

static void test_attention_once_reject_and_results(void)
{
    espui_view_model_t model;
    make_approval_model(&model, 1);
    setup_theme(&model, ESPUI_SCENE_ATTENTION);

    tap(200, 330); /* APPROVE ONCE */
    TEST_ASSERT_EQ_INT(1, host_espui_stub_action_count());
    const espui_action_t *action = host_espui_stub_action_at(0);
    TEST_ASSERT_NOT_NULL(action);
    TEST_ASSERT_EQ_INT(ESPUI_ACTION_RESPOND_PERMISSION, action->type);
    TEST_ASSERT_EQ_INT(ESPUI_PERMISSION_ONCE, action->decision);
    TEST_ASSERT_STR_EQ("per-1", action->permission_id);
    TEST_ASSERT_NOT_NULL(find_text("PENDING"));
    TEST_ASSERT_NOT_NULL(find_text("PENDING // WAIT FOR NODE"));
    TEST_ASSERT_NOT_NULL(find_text(" PEND"));

    /* Duplicate taps while pending must not enqueue again. */
    tap(200, 330);
    TEST_ASSERT_EQ_INT(1, host_espui_stub_action_count());

    espui_action_result_t result = permission_result(
        "per-1", ESPUI_PERMISSION_ONCE, ESPUI_ACTION_STATUS_OK, "accepted");
    int commits_before_result = s_scene_commit_count;
    theme()->ops->on_action_result(&result);
    TEST_ASSERT_EQ_INT(commits_before_result, s_scene_commit_count);
    TEST_ASSERT_NOT_NULL(find_text("ACK"));
    /* No optimistic removal: the attention entry stays until resolved. */
    TEST_ASSERT_EQ_INT(1, model.approval_count);
    TEST_ASSERT_NOT_NULL(find_text("> 1/1"));
    tap(200, 330);
    TEST_ASSERT_EQ_INT(1, host_espui_stub_action_count());

    TEST_ASSERT_TRUE(espui_view_model_resolve_attention(&model, "per-1"));
    theme()->ops->update(&model);
    TEST_ASSERT_NOT_NULL(find_text("NO PENDING ATTENTION"));

    /* Reject is a single tap and failures restore the buttons. */
    make_approval_model(&model, 1);
    setup_theme(&model, ESPUI_SCENE_ATTENTION);
    tap(80, 330); /* REJECT */
    TEST_ASSERT_EQ_INT(1, host_espui_stub_action_count());
    action = host_espui_stub_action_at(0);
    TEST_ASSERT_NOT_NULL(action);
    TEST_ASSERT_EQ_INT(ESPUI_PERMISSION_REJECT, action->decision);

    result = permission_result("per-1", ESPUI_PERMISSION_REJECT,
                               ESPUI_ACTION_STATUS_FAILED, "offline");
    theme()->ops->on_action_result(&result);
    TEST_ASSERT_NOT_NULL(find_text("ERR // offline"));
    TEST_ASSERT_NOT_NULL(find_visible_text("REJECT"));
    tap(80, 330);
    TEST_ASSERT_EQ_INT(2, host_espui_stub_action_count());

    result = permission_result("per-1", ESPUI_PERMISSION_REJECT,
                               ESPUI_ACTION_STATUS_TIMEOUT, "command timeout");
    theme()->ops->on_action_result(&result);
    TEST_ASSERT_NOT_NULL(find_text("TIMEOUT"));

    result = permission_result("per-1", ESPUI_PERMISSION_REJECT,
                               ESPUI_ACTION_STATUS_REJECTED, "already handled");
    theme()->ops->on_action_result(&result);
    TEST_ASSERT_NOT_NULL(find_text("REJECTED // already handled"));
}

static void test_attention_always_confirmation(void)
{
    espui_view_model_t model;
    make_approval_model(&model, 2);
    setup_theme(&model, ESPUI_SCENE_ATTENTION);

    tap(390, 330); /* APPROVE ALWAYS, first tap only arms */
    TEST_ASSERT_EQ_INT(0, host_espui_stub_action_count());
    TEST_ASSERT_NOT_NULL(find_visible_text("CONFIRM ALWAYS"));
    TEST_ASSERT_NOT_NULL(find_text("CONFIRM ALWAYS: TAP AGAIN (5s)"));

    /* Switching selection cancels the confirmation. */
    tap(40, 160);
    TEST_ASSERT_NULL(find_visible_text("CONFIRM ALWAYS"));
    tap(40, 120);
    TEST_ASSERT_EQ_INT(0, host_espui_stub_action_count());
    tap(390, 330);
    TEST_ASSERT_NOT_NULL(find_visible_text("CONFIRM ALWAYS"));

    tap(390, 330); /* second tap within 5s submits */
    TEST_ASSERT_EQ_INT(1, host_espui_stub_action_count());
    const espui_action_t *action = host_espui_stub_action_at(0);
    TEST_ASSERT_NOT_NULL(action);
    TEST_ASSERT_EQ_INT(ESPUI_ACTION_RESPOND_PERMISSION, action->type);
    TEST_ASSERT_EQ_INT(ESPUI_PERMISSION_ALWAYS, action->decision);
    TEST_ASSERT_STR_EQ("per-1", action->permission_id);

    /* Timeout cancels without submitting. */
    make_approval_model(&model, 1);
    setup_theme(&model, ESPUI_SCENE_ATTENTION);
    tap(390, 330);
    TEST_ASSERT_NOT_NULL(find_visible_text("CONFIRM ALWAYS"));
    host_espui_stub_set_uptime_ms(6000);
    theme()->ops->on_tick(100);
    TEST_ASSERT_NULL(find_visible_text("CONFIRM ALWAYS"));
    TEST_ASSERT_NOT_NULL(find_visible_text("APPROVE ALWAYS"));
    TEST_ASSERT_EQ_INT(0, host_espui_stub_action_count());

    /* Reject during confirmation cancels the armed Approve Always. */
    tap(390, 330);
    tap(80, 330);
    TEST_ASSERT_EQ_INT(1, host_espui_stub_action_count());
    action = host_espui_stub_action_at(0);
    TEST_ASSERT_NOT_NULL(action);
    TEST_ASSERT_EQ_INT(ESPUI_PERMISSION_REJECT, action->decision);
}

static void test_attention_truncated_summary(void)
{
    espui_view_model_t model;
    make_approval_model(&model, 2);
    model.attention[0].summary_truncated = true;
    setup_theme(&model, ESPUI_SCENE_ATTENTION);

    TEST_ASSERT_NOT_NULL(find_text("OPEN MOBILE > APPROVALS"));
    fake_widget_t *approve_once = find_text("APPROVE ONCE");
    TEST_ASSERT_NOT_NULL(approve_once);
    TEST_ASSERT_FALSE(approve_once->visible);
    fake_widget_t *approve_always = find_text("APPROVE ALWAYS");
    TEST_ASSERT_NOT_NULL(approve_always);
    TEST_ASSERT_FALSE(approve_always->visible);
    TEST_ASSERT_NOT_NULL(find_visible_text("REJECT"));

    tap(200, 330);
    TEST_ASSERT_EQ_INT(0, host_espui_stub_action_count());
    tap(390, 330);
    TEST_ASSERT_EQ_INT(0, host_espui_stub_action_count());
    tap(80, 330);
    TEST_ASSERT_EQ_INT(1, host_espui_stub_action_count());
    const espui_action_t *action = host_espui_stub_action_at(0);
    TEST_ASSERT_NOT_NULL(action);
    TEST_ASSERT_EQ_INT(ESPUI_PERMISSION_REJECT, action->decision);

    /* Empty summaries are treated the same as truncated ones. */
    make_approval_model(&model, 1);
    model.attention[0].summary[0] = '\0';
    setup_theme(&model, ESPUI_SCENE_ATTENTION);
    TEST_ASSERT_NOT_NULL(find_text("OPEN MOBILE > APPROVALS"));
    fake_widget_t *once = find_text("APPROVE ONCE");
    TEST_ASSERT_NOT_NULL(once);
    TEST_ASSERT_FALSE(once->visible);
    tap(200, 330);
    TEST_ASSERT_EQ_INT(0, host_espui_stub_action_count());
}

static void test_focus_cycle_and_interrupt(void)
{
    espui_view_model_t model;
    make_focus_model(&model);
    setup_theme(&model, ESPUI_SCENE_FOCUS);

    TEST_ASSERT_NOT_NULL(find_visible_text("SESSION: RUNNING TASK"));
    tap(200, 150);
    TEST_ASSERT_NOT_NULL(find_visible_text("SESSION: WAITING TASK"));
    tap(200, 150);
    TEST_ASSERT_NOT_NULL(find_visible_text("SESSION: RUNNING TASK"));

    /* Completed sessions never join the cycle. */
    for (int i = 0; i < 4; ++i) {
        tap(200, 150);
        TEST_ASSERT_NULL(find_visible_text("SESSION: DONE TASK"));
        TEST_ASSERT_NULL(find_visible_text("SESSION: FAILED TASK"));
    }

    tap(240, 340); /* Interrupt the selected session. */
    TEST_ASSERT_EQ_INT(1, host_espui_stub_action_count());
    const espui_action_t *action = host_espui_stub_action_at(0);
    TEST_ASSERT_NOT_NULL(action);
    TEST_ASSERT_EQ_INT(ESPUI_ACTION_INTERRUPT_SESSION, action->type);
    TEST_ASSERT_STR_EQ("ses-run", action->session_id);
    TEST_ASSERT_NOT_NULL(find_visible_text("INTERRUPTING..."));

    tap(240, 340);
    TEST_ASSERT_EQ_INT(1, host_espui_stub_action_count());

    espui_action_result_t result =
        interrupt_result("ses-run", ESPUI_ACTION_STATUS_OK, "accepted");
    theme()->ops->on_action_result(&result);
    TEST_ASSERT_NOT_NULL(find_visible_text("INTERRUPT SENT"));
    tap(240, 340);
    TEST_ASSERT_EQ_INT(1, host_espui_stub_action_count());

    /* The interrupt target follows the current focus selection. */
    tap(200, 150);
    TEST_ASSERT_NOT_NULL(find_visible_text("SESSION: WAITING TASK"));
    TEST_ASSERT_NOT_NULL(find_visible_text("INTERRUPT"));
    tap(240, 340);
    TEST_ASSERT_EQ_INT(2, host_espui_stub_action_count());
    action = host_espui_stub_action_at(1);
    TEST_ASSERT_NOT_NULL(action);
    TEST_ASSERT_EQ_INT(ESPUI_ACTION_INTERRUPT_SESSION, action->type);
    TEST_ASSERT_STR_EQ("ses-wait", action->session_id);

    result = interrupt_result("ses-wait", ESPUI_ACTION_STATUS_FAILED,
                              "offline");
    theme()->ops->on_action_result(&result);
    TEST_ASSERT_NOT_NULL(find_visible_text("INTERRUPT FAILED"));

    /* Completed/unknown selections do not expose a clickable button. */
    make_focus_model(&model);
    setup_theme_with_focus(&model, ESPUI_SCENE_FOCUS, 3);
    TEST_ASSERT_NOT_NULL(find_visible_text("SESSION: DONE TASK"));
    TEST_ASSERT_NOT_NULL(find_visible_text("INTERRUPT UNAVAILABLE"));
    fake_widget_t *bg = NULL;
    for (int i = 0; i < s_widget_count; ++i) {
        if (s_widgets[i].kind == ESPUI_WIDGET_RECT &&
            s_widgets[i].spec.y == 320 && s_widgets[i].spec.height == 44) {
            bg = &s_widgets[i];
            break;
        }
    }
    TEST_ASSERT_NOT_NULL(bg);
    TEST_ASSERT_FALSE(bg->visible);
    tap(240, 340);
    TEST_ASSERT_EQ_INT(0, host_espui_stub_action_count());
}

static void test_action_dispatch_failure(void)
{
    espui_view_model_t model;
    make_approval_model(&model, 1);
    setup_theme(&model, ESPUI_SCENE_ATTENTION);

    host_espui_stub_set_dispatch_result(ESP_ERR_TIMEOUT);
    tap(200, 330);
    TEST_ASSERT_EQ_INT(1, host_espui_stub_action_count());
    TEST_ASSERT_NOT_NULL(find_text("SUBMIT FAILED // RETRY"));
    TEST_ASSERT_NOT_NULL(find_visible_text("APPROVE ONCE"));

    host_espui_stub_set_dispatch_result(ESP_OK);
    tap(200, 330);
    TEST_ASSERT_EQ_INT(2, host_espui_stub_action_count());
    TEST_ASSERT_NOT_NULL(find_text("PENDING"));
}

void test_terminal_scenes_run(void)
{
    test_scene_layouts_and_commit_policy();
    test_attention_selection();
    test_attention_once_reject_and_results();
    test_attention_always_confirmation();
    test_attention_truncated_summary();
    test_focus_cycle_and_interrupt();
    test_action_dispatch_failure();
}
