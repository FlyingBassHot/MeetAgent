/*
 * Terminal Aesthetic theme — E2 mock.
 *
 * This is NOT the E3 ASCII renderer. It uses the label renderer with the
 * terminal design tokens so the theme pipeline, switching and fallback can be
 * exercised on the target display. E3 replaces the core label with the
 * procedural ASCII renderer; the manifest id and tokens stay the same.
 */
#include <stdio.h>
#include <string.h>

#include "espui.h"
#include "espui_theme.h"
#include "espui_util.h"
#include "phosphor_lvgl_port.h"

static const espui_manifest_t s_manifest = {
    .theme_id = "terminal_aesthetic",
    .name = "Terminal Aesthetic (E2 Mock)",
    .version = "0.2.0-mock",
    .api_version = ESPUI_THEME_API_VERSION,
    .renderer_id = "builtin.label",
    .display =
        {
            .orientation = ESPUI_ORIENTATION_ANY,
            .min_width = 480,
            .min_height = 480,
        },
    .scene_mask = ESPUI_SCENE_MASK_ALL,
    .configurable_mask = ESPUI_CONFIG_ACCENT_COLOR | ESPUI_CONFIG_BRIGHTNESS |
                         ESPUI_CONFIG_ANIMATION_SPEED |
                         ESPUI_CONFIG_MOTION_ENABLED |
                         ESPUI_CONFIG_VISUAL_DENSITY |
                         ESPUI_CONFIG_IDLE_TIMEOUT | ESPUI_CONFIG_NIGHT_MODE,
    .motion_profile_id = "phosphor_default",
};

#define TERMINAL_CORE_ART                                                      \
    "       .-====-.       \n"                                                \
    "     .'  ....  '.     \n"                                                \
    "    /   .-==-.   \\    \n"                                               \
    "    \\   '----'   /    \n"                                               \
    "     '._      _.'     \n"                                                \
    "        '-..-'        "

typedef struct {
    const espui_context_t *ctx;
    const espui_renderer_ops_t *renderer;
    const espui_tokens_t *tokens;
    espui_ui_scene_t *scene;
    espui_ui_widget_t *background;
    espui_ui_widget_t *core_panel;
    espui_ui_widget_t *header;
    espui_ui_widget_t *scene_label;
    espui_ui_widget_t *core;
    espui_ui_widget_t *status;
    espui_ui_widget_t *strip;
    espui_ui_widget_t *footer;
    espui_ui_widget_t *canvas;
    espui_ui_widget_t *focus_title;
    espui_ui_widget_t *focus_agent;
    espui_ui_widget_t *focus_status;
    espui_ui_widget_t *focus_duration;
    espui_ui_widget_t *focus_recent;
    espui_ui_widget_t *focus_action_bg;
    espui_ui_widget_t *focus_action;
    espui_ui_widget_t *activity_title;
    espui_ui_widget_t *activity_body;
    espui_ui_widget_t *attention_title;
    espui_ui_widget_t *attention_row[3];
    espui_ui_widget_t *attention_detail;
    espui_ui_widget_t *attention_button_bg[3];
    espui_ui_widget_t *attention_button_label[3];
    espui_ui_widget_t *attention_hint;
    espui_widget_spec_t status_spec;
    espui_widget_spec_t strip_spec;
    espui_widget_spec_t core_panel_spec;
    espui_widget_spec_t focus_action_spec;
    espui_widget_spec_t attention_button_spec[3];
    bool canvas_attached;
    espui_scene_id_t current_scene;

    /* Focus selection: survives ViewModel updates while the session exists. */
    char focus_session_id[ESPUI_ID_MAX];

    /* Attention selection and approval state. */
    char selected_permission_id[ESPUI_ID_MAX];
    bool selected_is_approval;
    bool selected_summary_blocked;
    bool approval_pending;
    char pending_permission_id[ESPUI_ID_MAX];
    espui_permission_decision_t pending_decision;
    bool approval_acked;
    char acked_permission_id[ESPUI_ID_MAX];
    bool always_confirm_armed;
    uint32_t always_confirm_deadline_ms;
    char attention_status[ESPUI_LABEL_MAX];

    /* Interrupt button state, scoped to the selected session. */
    char interrupt_session_id[ESPUI_ID_MAX];
    bool interrupt_pending;
    bool interrupt_sent;
    char interrupt_status[ESPUI_LABEL_MAX];
} terminal_theme_state_t;

#define ATTENTION_ROWS 3
#define ATTENTION_ROW_Y 106
#define ATTENTION_ROW_PITCH 44
#define ATTENTION_ROW_HEIGHT 40
#define ATTENTION_DETAIL_Y 240
#define ATTENTION_DETAIL_HEIGHT 68
#define ATTENTION_BUTTON_Y 314
#define ATTENTION_BUTTON_HEIGHT 44
#define ATTENTION_BUTTON_WIDTH 144
#define ATTENTION_BUTTON_X(i) (16 + (i) * 152)
#define ATTENTION_HINT_Y 364
#define ATTENTION_HINT_HEIGHT 40
#define ALWAYS_CONFIRM_TIMEOUT_MS 5000
#define FOCUS_CONTENT_Y0 60
#define FOCUS_CONTENT_Y1 300
#define FOCUS_INTERRUPT_Y 320
#define FOCUS_INTERRUPT_HEIGHT 44
#define FOCUS_INTERRUPT_WIDTH 200
#define FOCUS_INTERRUPT_X 140

static terminal_theme_state_t s_state;

static void destroy_scene(void)
{
    if (s_state.scene != NULL || s_state.canvas_attached) {
        phosphor_lvgl_port_detach();
    }
    s_state.canvas_attached = false;
    if (s_state.scene != NULL && s_state.renderer != NULL &&
        s_state.renderer->scene_destroy != NULL) {
        s_state.renderer->scene_destroy(s_state.scene);
    }
    memset(&s_state, 0, sizeof(s_state));
}

static espui_ui_widget_t *make_rect(terminal_theme_state_t *state,
                                    const espui_widget_spec_t *spec)
{
    return state->renderer->widget_create(state->scene, ESPUI_WIDGET_RECT,
                                          spec);
}

static espui_ui_widget_t *make_widget(terminal_theme_state_t *state,
                                      const espui_widget_spec_t *spec,
                                      const char *text)
{
    espui_ui_widget_t *widget =
        state->renderer->widget_create(state->scene, ESPUI_WIDGET_LABEL, spec);
    if (widget == NULL) {
        return NULL;
    }
    if (text != NULL) {
        state->renderer->widget_set_text(widget, text);
    }
    return widget;
}

static void set_visible(espui_ui_widget_t *widget, bool visible)
{
    if (widget != NULL && s_state.renderer != NULL &&
        s_state.renderer->widget_set_visible != NULL) {
        (void)s_state.renderer->widget_set_visible(widget, visible);
    }
}

static void set_text(espui_ui_widget_t *widget, const char *text)
{
    if (widget != NULL && s_state.renderer != NULL && text != NULL &&
        s_state.renderer->widget_set_text != NULL) {
        (void)s_state.renderer->widget_set_text(widget, text);
    }
}

static void apply_scene_visibility(espui_scene_id_t scene)
{
    if (s_state.renderer == NULL) {
        return;
    }
    bool ambient = scene == ESPUI_SCENE_AMBIENT;
    bool focus = scene == ESPUI_SCENE_FOCUS;
    bool activity = scene == ESPUI_SCENE_ACTIVITY;
    bool attention = scene == ESPUI_SCENE_ATTENTION;
    bool show_canvas = ambient && s_state.canvas_attached &&
                       s_state.canvas != NULL;

    set_visible(s_state.background, true);
    set_visible(s_state.header, true);
    set_visible(s_state.scene_label, !ambient);
    set_visible(s_state.canvas, show_canvas);
    set_visible(s_state.core_panel, !ambient || !s_state.canvas_attached);
    set_visible(s_state.core, ambient && !s_state.canvas_attached);
    set_visible(s_state.status, ambient && !s_state.canvas_attached);
    set_visible(s_state.strip, ambient);
    set_visible(s_state.footer, !ambient);
    set_visible(s_state.focus_title, focus);
    set_visible(s_state.focus_agent, focus);
    set_visible(s_state.focus_status, focus);
    set_visible(s_state.focus_duration, focus);
    set_visible(s_state.focus_recent, focus);
    set_visible(s_state.focus_action_bg, focus);
    set_visible(s_state.focus_action, focus);
    set_visible(s_state.activity_title, activity);
    set_visible(s_state.activity_body, activity);
    set_visible(s_state.attention_title, attention);
    for (int i = 0; i < ATTENTION_ROWS; ++i) {
        set_visible(s_state.attention_row[i], false);
        set_visible(s_state.attention_button_bg[i], false);
        set_visible(s_state.attention_button_label[i], false);
    }
    set_visible(s_state.attention_detail, attention);
    set_visible(s_state.attention_hint, false);

    if (s_state.core_panel != NULL) {
        espui_widget_spec_t panel = s_state.core_panel_spec;
        if (!ambient) {
            panel.y = 60;
            panel.height = 356;
        }
        (void)s_state.renderer->widget_set_spec(s_state.core_panel, &panel);
    }
    if (s_state.strip != NULL) {
        espui_widget_spec_t strip = s_state.strip_spec;
        if (ambient && s_state.canvas_attached) {
            strip.y = 458;
            strip.height = 18;
        }
        (void)s_state.renderer->widget_set_spec(s_state.strip, &strip);
    }
}


static bool session_is_active(const espui_session_view_t *session)
{
    return session != NULL &&
           (session->status == ESPUI_SESSION_RUNNING ||
            session->status == ESPUI_SESSION_WAITING_APPROVAL);
}

static size_t collect_sessions(const espui_view_model_t *model,
                               const espui_session_view_t **list,
                               size_t capacity, bool active_only)
{
    size_t count = 0;
    if (model == NULL || list == NULL || capacity == 0) {
        return 0;
    }

    /* Active sessions first, then completed/interrupted/unknown. */
    for (size_t i = 0; i < ESPUI_MAX_SESSIONS && count < capacity; ++i) {
        const espui_session_view_t *session = &model->sessions[i];
        if (!session->in_use || !session_is_active(session)) {
            continue;
        }
        list[count++] = session;
    }
    if (active_only) {
        return count;
    }
    for (size_t i = 0; i < ESPUI_MAX_SESSIONS && count < capacity; ++i) {
        const espui_session_view_t *session = &model->sessions[i];
        if (!session->in_use || session_is_active(session)) {
            continue;
        }
        list[count++] = session;
    }
    return count;
}

static const espui_session_view_t *selected_session(
    const espui_view_model_t *model)
{
    const espui_session_view_t *list[ESPUI_MAX_SESSIONS];
    size_t count = collect_sessions(model, list, ESPUI_MAX_SESSIONS, false);
    if (count == 0) {
        s_state.focus_session_id[0] = '\0';
        return NULL;
    }

    size_t index = count;
    bool had_selection = s_state.focus_session_id[0] != '\0';
    for (size_t i = 0; i < count; ++i) {
        if (had_selection &&
            strcmp(list[i]->session_id, s_state.focus_session_id) == 0) {
            index = i;
            break;
        }
    }
    if (index >= count) {
        /* A disappeared selection falls back to the first session; the very
         * first render still honours the global focus accessor. */
        index = had_selection ? 0 : (espui_focus_index(s_state.ctx) % count);
        espui_strlcpy(s_state.focus_session_id, list[index]->session_id,
                      sizeof(s_state.focus_session_id));
    }
    return list[index];
}

static const espui_activity_view_t *latest_activity(
    const espui_view_model_t *model, const char *session_id)
{
    for (int i = (int)espui_view_model_activity_count(model) - 1; i >= 0;
         --i) {
        const espui_activity_view_t *entry =
            espui_view_model_activity_at(model, (uint8_t)i);
        if (entry == NULL) {
            continue;
        }
        if (session_id == NULL || session_id[0] == '\0' ||
            strcmp(entry->session_id, session_id) == 0) {
            return entry;
        }
    }
    return NULL;
}

static void update_ambient(const espui_view_model_t *model)
{
    uint16_t active_sessions = espui_view_model_active_session_count(model);
    const char *status = "SYSTEM IDLE";
    uint32_t color = s_state.tokens->colors.primary;

    if (model->node.status == ESPUI_NODE_OFFLINE) {
        status = "NODE OFFLINE";
        color = s_state.tokens->colors.error;
    } else if (model->node.status == ESPUI_NODE_CONNECTING) {
        status = "NODE CONNECTING";
    } else if (model->node.status == ESPUI_NODE_SYNCING) {
        status = "SYNCING STATE";
    } else if (model->approval_count > 0) {
        status = "APPROVAL REQUIRED";
        color = s_state.tokens->colors.warning;
    } else if (model->error_count > 0) {
        status = "SYSTEM ERROR";
        color = s_state.tokens->colors.error;
    } else if (active_sessions > 0) {
        status = "SYSTEM ACTIVE";
    }

    s_state.status_spec.fg_color = color;
    if (s_state.status != NULL) {
        (void)s_state.renderer->widget_set_spec(s_state.status,
                                                &s_state.status_spec);
        set_text(s_state.status, status);
    }

    char strip[ESPUI_LABEL_MAX];
    snprintf(strip, sizeof(strip), "AGENTS %02u   SESSIONS %02u   ALERTS %02u",
             (unsigned)espui_view_model_agent_count(model),
             (unsigned)active_sessions,
             (unsigned)(model->approval_count + model->error_count));
    set_text(s_state.strip, strip);
}

static void sync_interrupt_target(const espui_session_view_t *session)
{
    const char *id = session != NULL ? session->session_id : "";
    if (strcmp(s_state.interrupt_session_id, id) != 0) {
        espui_strlcpy(s_state.interrupt_session_id, id,
                      sizeof(s_state.interrupt_session_id));
        s_state.interrupt_pending = false;
        s_state.interrupt_sent = false;
        s_state.interrupt_status[0] = '\0';
    }
    if (!session_is_active(session)) {
        s_state.interrupt_pending = false;
        s_state.interrupt_sent = false;
        s_state.interrupt_status[0] = '\0';
    }
}

static void render_focus_action(const espui_session_view_t *session)
{
    if (s_state.focus_action == NULL || s_state.renderer == NULL) {
        return;
    }
    bool in_scene = s_state.current_scene == ESPUI_SCENE_FOCUS;
    bool eligible = session_is_active(session);
    set_visible(s_state.focus_action_bg, eligible && in_scene);

    espui_widget_spec_t spec = s_state.focus_action_spec;
    const char *text = "INTERRUPT UNAVAILABLE";
    if (session == NULL) {
        text = "NO ACTIVE SESSION";
    } else if (eligible) {
        bool same = strcmp(s_state.interrupt_session_id,
                           session->session_id) == 0;
        text = "INTERRUPT";
        spec.fg_color = s_state.tokens->colors.primary;
        if (same && s_state.interrupt_pending) {
            text = "INTERRUPTING...";
            spec.fg_color = s_state.tokens->colors.muted;
        } else if (same && s_state.interrupt_sent) {
            text = "INTERRUPT SENT";
            spec.fg_color = s_state.tokens->colors.warning;
        } else if (same && s_state.interrupt_status[0] != '\0') {
            text = s_state.interrupt_status;
            spec.fg_color = s_state.tokens->colors.error;
        }
    } else {
        spec.fg_color = s_state.tokens->colors.muted;
    }
    (void)s_state.renderer->widget_set_spec(s_state.focus_action, &spec);
    set_text(s_state.focus_action, text);
    set_visible(s_state.focus_action, in_scene);
}

static void update_focus(const espui_view_model_t *model)
{
    const espui_session_view_t *session = selected_session(model);
    sync_interrupt_target(session);
    if (session == NULL) {
        set_text(s_state.focus_title, "NO ACTIVE SESSION");
        set_text(s_state.focus_agent, "AGENT: -");
        set_text(s_state.focus_status, "STATUS: -");
        set_text(s_state.focus_duration, "DURATION: --:--");
        set_text(s_state.focus_recent, "RECENT: NO EVENT");
        render_focus_action(NULL);
        return;
    }

    char line[ESPUI_LABEL_MAX];
    snprintf(line, sizeof(line), "SESSION: %s",
             session->title[0] != '\0' ? session->title : session->session_id);
    set_text(s_state.focus_title, line);

    const espui_agent_view_t *agent =
        espui_view_model_find_agent_const(model, session->agent_id);
    snprintf(line, sizeof(line), "AGENT: %s [%s]",
             agent != NULL && agent->display_name[0] != '\0'
                 ? agent->display_name
                 : (session->agent_id[0] != '\0' ? session->agent_id : "-"),
             agent != NULL ? espui_agent_status_name(agent->status) : "unknown");
    set_text(s_state.focus_agent, line);

    snprintf(line, sizeof(line), "STATUS: %s",
             espui_session_status_name(session->status));
    set_text(s_state.focus_status, line);

    uint32_t seconds = session->duration_seconds;
    snprintf(line, sizeof(line), "DURATION: %02u:%02u",
             (unsigned)(seconds / 60u), (unsigned)(seconds % 60u));
    set_text(s_state.focus_duration, line);

    const espui_activity_view_t *recent =
        latest_activity(model, session->session_id);
    if (recent != NULL) {
        snprintf(line, sizeof(line), "RECENT: %s",
                 recent->short_text[0] != '\0' ? recent->short_text
                                               : recent->type);
    } else {
        snprintf(line, sizeof(line), "RECENT: NO EVENT");
    }
    set_text(s_state.focus_recent, line);
    render_focus_action(session);
}

static void update_activity(const espui_view_model_t *model)
{
    char body[512];
    size_t offset = 0;
    uint8_t count = espui_view_model_activity_count(model);
    if (count == 0) {
        set_text(s_state.activity_body, "NO RECENT EVENTS");
        return;
    }

    for (uint8_t i = 0; i < count && offset < sizeof(body); ++i) {
        const espui_activity_view_t *entry =
            espui_view_model_activity_at(model, i);
        if (entry == NULL) {
            continue;
        }
        int written = snprintf(body + offset, sizeof(body) - offset,
                               "T+%5lus  %-12.12s  %-.18s\n",
                               (unsigned long)(entry->timestamp_ms / 1000u),
                               entry->type[0] != '\0' ? entry->type : "event",
                               entry->short_text[0] != '\0'
                                   ? entry->short_text
                                   : entry->session_id);
        if (written < 0) {
            break;
        }
        offset += (size_t)written;
    }
    body[sizeof(body) - 1] = '\0';
    set_text(s_state.activity_body, body);
}

static uint8_t collect_attention(const espui_view_model_t *model,
                                 const espui_attention_view_t **items,
                                 uint8_t capacity)
{
    uint8_t count = 0;
    if (model == NULL || items == NULL) {
        return 0;
    }
    for (size_t i = 0; i < ESPUI_MAX_ATTENTION && count < capacity; ++i) {
        if (model->attention[i].in_use) {
            items[count++] = &model->attention[i];
        }
    }
    return count;
}

static bool attention_list_has(const espui_attention_view_t **items,
                               uint8_t count, const char *permission_id)
{
    if (permission_id == NULL || permission_id[0] == '\0') {
        return false;
    }
    for (uint8_t i = 0; i < count; ++i) {
        if (strcmp(items[i]->permission_id, permission_id) == 0) {
            return true;
        }
    }
    return false;
}

static void set_action_button(int index, const char *text, bool visible,
                              uint32_t bg_color, bool disabled)
{
    if (index < 0 || index >= 3 || s_state.renderer == NULL) {
        return;
    }
    espui_ui_widget_t *background = s_state.attention_button_bg[index];
    espui_ui_widget_t *label = s_state.attention_button_label[index];
    set_visible(background, visible);
    set_visible(label, visible);
    if (!visible || background == NULL || label == NULL) {
        return;
    }

    espui_widget_spec_t rect = s_state.attention_button_spec[index];
    rect.x = ATTENTION_BUTTON_X(index);
    rect.y = ATTENTION_BUTTON_Y;
    rect.width = ATTENTION_BUTTON_WIDTH;
    rect.height = ATTENTION_BUTTON_HEIGHT;
    rect.visible = true;
    rect.transparent_bg = false;
    rect.fg_color = bg_color;
    rect.bg_color = bg_color;
    (void)s_state.renderer->widget_set_spec(background, &rect);

    espui_widget_spec_t spec = rect;
    spec.x = (int16_t)(rect.x + 4);
    spec.y = (int16_t)(rect.y + 10);
    spec.width = (int16_t)(rect.width - 8);
    spec.height = 24;
    spec.transparent_bg = true;
    spec.fg_color = disabled ? s_state.tokens->colors.muted
                             : s_state.tokens->colors.background;
    spec.align = ESPUI_TEXT_ALIGN_CENTER;
    (void)s_state.renderer->widget_set_spec(label, &spec);
    set_text(label, text);
}

static void update_attention(const espui_view_model_t *model)
{
    const espui_attention_view_t *items[ESPUI_MAX_ATTENTION];
    uint8_t count = collect_attention(model, items, ESPUI_MAX_ATTENTION);
    bool in_scene = s_state.current_scene == ESPUI_SCENE_ATTENTION;

    if (!attention_list_has(items, count, s_state.pending_permission_id)) {
        s_state.approval_pending = false;
        s_state.pending_permission_id[0] = '\0';
    }
    if (!attention_list_has(items, count, s_state.acked_permission_id)) {
        s_state.approval_acked = false;
        s_state.acked_permission_id[0] = '\0';
    }

    if (count == 0) {
        s_state.selected_permission_id[0] = '\0';
        s_state.selected_is_approval = false;
        s_state.selected_summary_blocked = false;
        s_state.always_confirm_armed = false;
        s_state.attention_status[0] = '\0';
        for (int i = 0; i < ATTENTION_ROWS; ++i) {
            set_visible(s_state.attention_row[i], false);
            set_visible(s_state.attention_button_bg[i], false);
            set_visible(s_state.attention_button_label[i], false);
        }
        set_text(s_state.attention_detail, "NO PENDING ATTENTION");
        espui_widget_spec_t hint_spec = {
            .x = 28,
            .y = ATTENTION_HINT_Y,
            .width = 424,
            .height = ATTENTION_HINT_HEIGHT,
            .fg_color = s_state.tokens->colors.muted,
            .transparent_bg = true,
            .font_scale_percent = 100,
            .align = ESPUI_TEXT_ALIGN_CENTER,
            .visible = in_scene,
        };
        (void)s_state.renderer->widget_set_spec(s_state.attention_hint,
                                                &hint_spec);
        set_text(s_state.attention_hint, "APPROVALS APPEAR HERE");
        set_visible(s_state.attention_hint, in_scene);
        return;
    }

    uint8_t selected = 0;
    bool found = false;
    for (uint8_t i = 0; i < count; ++i) {
        if (s_state.selected_permission_id[0] != '\0' &&
            strcmp(items[i]->permission_id,
                   s_state.selected_permission_id) == 0) {
            selected = i;
            found = true;
            break;
        }
    }
    if (!found) {
        selected = 0;
    }

    bool selection_changed =
        strcmp(s_state.selected_permission_id, items[selected]->permission_id) !=
        0;
    if (selection_changed) {
        espui_strlcpy(s_state.selected_permission_id,
                      items[selected]->permission_id,
                      sizeof(s_state.selected_permission_id));
        s_state.always_confirm_armed = false;
        s_state.attention_status[0] = '\0';
    }

    const espui_attention_view_t *item = items[selected];
    s_state.selected_is_approval = item->kind == ESPUI_ATTENTION_APPROVAL;
    s_state.selected_summary_blocked =
        s_state.selected_is_approval &&
        (item->summary_truncated || item->summary[0] == '\0');

    char line[ESPUI_LABEL_MAX];
    for (int i = 0; i < ATTENTION_ROWS; ++i) {
        if (!in_scene || i >= (int)count) {
            set_visible(s_state.attention_row[i], false);
            continue;
        }
        const espui_attention_view_t *row = items[i];
        uint32_t age_s = 0;
        if (model->uptime_ms > row->created_at_ms) {
            age_s = (model->uptime_ms - row->created_at_ms) / 1000u;
        }
        bool row_pending =
            s_state.approval_pending &&
            strcmp(row->permission_id, s_state.pending_permission_id) == 0;
        snprintf(line, sizeof(line), "%c %u/%u %-10.10s %.14s T+%lus%s",
                 i == selected ? '>' : ' ', (unsigned)(i + 1),
                 (unsigned)count, espui_attention_kind_name(row->kind),
                 row->session_id[0] != '\0' ? row->session_id : "-",
                 (unsigned long)age_s, row_pending ? " PEND" : "");

        espui_widget_spec_t spec = {
            .x = 28,
            .y = (int16_t)(ATTENTION_ROW_Y + i * ATTENTION_ROW_PITCH),
            .width = 424,
            .height = ATTENTION_ROW_HEIGHT,
            .fg_color = i == selected ? s_state.tokens->colors.warning
                                      : s_state.tokens->colors.secondary,
            .transparent_bg = true,
            .font_scale_percent = 100,
            .align = ESPUI_TEXT_ALIGN_LEFT,
            .visible = true,
        };
        (void)s_state.renderer->widget_set_spec(s_state.attention_row[i],
                                                &spec);
        set_text(s_state.attention_row[i], line);
        set_visible(s_state.attention_row[i], true);
    }

    uint32_t age_s = 0;
    if (model->uptime_ms > item->created_at_ms) {
        age_s = (model->uptime_ms - item->created_at_ms) / 1000u;
    }
    char detail[192];
    snprintf(detail, sizeof(detail),
             "PERM %u/%u  %-10.10s  T+%lus\nSES %.20s  ID %.18s\nSUMMARY: %.34s",
             (unsigned)(selected + 1), (unsigned)count,
             espui_attention_kind_name(item->kind), (unsigned long)age_s,
             item->session_id[0] != '\0' ? item->session_id : "-",
             item->permission_id[0] != '\0' ? item->permission_id : "-",
             item->summary[0] != '\0' ? item->summary : "(empty)");
    set_text(s_state.attention_detail, detail);

    bool selected_acked =
        s_state.approval_acked &&
        strcmp(s_state.acked_permission_id, item->permission_id) == 0;
    bool disabled = s_state.approval_pending || selected_acked;
    bool show_reject = s_state.selected_is_approval;
    bool show_approve = s_state.selected_is_approval &&
                        !s_state.selected_summary_blocked;

    if (in_scene && show_reject) {
        bool reject_pending = s_state.approval_pending &&
                              s_state.pending_decision == ESPUI_PERMISSION_REJECT;
        set_action_button(0, reject_pending ? "PENDING" : "REJECT", true,
                          disabled ? s_state.tokens->colors.surface
                                   : s_state.tokens->colors.error,
                          disabled);
    } else {
        set_action_button(0, "REJECT", false, 0, true);
    }

    if (in_scene && show_approve) {
        bool once_pending = s_state.approval_pending &&
                            s_state.pending_decision == ESPUI_PERMISSION_ONCE;
        set_action_button(1, once_pending ? "PENDING" : "APPROVE ONCE", true,
                          disabled ? s_state.tokens->colors.surface
                                   : s_state.tokens->colors.secondary,
                          disabled);
        const char *always_text = "APPROVE ALWAYS";
        if (s_state.approval_pending &&
            s_state.pending_decision == ESPUI_PERMISSION_ALWAYS) {
            always_text = "PENDING";
        } else if (s_state.always_confirm_armed) {
            always_text = "CONFIRM ALWAYS";
        }
        set_action_button(2, always_text, true,
                          disabled ? s_state.tokens->colors.surface
                                   : (s_state.always_confirm_armed
                                          ? s_state.tokens->colors.highlight
                                          : s_state.tokens->colors.warning),
                          disabled);
    } else {
        set_action_button(1, "APPROVE ONCE", false, 0, true);
        set_action_button(2, "APPROVE ALWAYS", false, 0, true);
    }

    if (!in_scene) {
        set_visible(s_state.attention_hint, false);
        return;
    }
    if (s_state.selected_summary_blocked) {
        set_text(s_state.attention_hint, "OPEN MOBILE > APPROVALS");
    } else if (s_state.approval_pending) {
        set_text(s_state.attention_hint, "PENDING // WAIT FOR NODE");
    } else if (s_state.always_confirm_armed) {
        set_text(s_state.attention_hint, "CONFIRM ALWAYS: TAP AGAIN (5s)");
    } else if (s_state.attention_status[0] != '\0') {
        set_text(s_state.attention_hint, s_state.attention_status);
    } else if (selected_acked) {
        set_text(s_state.attention_hint, "ACK");
    } else if (!s_state.selected_is_approval) {
        set_text(s_state.attention_hint, "NO TERMINAL ACTION");
    } else if (count > ATTENTION_ROWS) {
        snprintf(line, sizeof(line), "+%u MORE // OPEN MOBILE",
                 (unsigned)(count - ATTENTION_ROWS));
        set_text(s_state.attention_hint, line);
    } else {
        set_text(s_state.attention_hint, "REJECT IS ALWAYS SAFE");
    }
    uint32_t hint_color = s_state.tokens->colors.muted;
    if (s_state.selected_summary_blocked) {
        hint_color = s_state.tokens->colors.warning;
    } else if (s_state.approval_pending) {
        hint_color = s_state.tokens->colors.primary;
    } else if (s_state.always_confirm_armed) {
        hint_color = s_state.tokens->colors.highlight;
    } else if (strncmp(s_state.attention_status, "ACK", 3) == 0) {
        hint_color = s_state.tokens->colors.primary;
    } else if (s_state.attention_status[0] != '\0') {
        hint_color = s_state.tokens->colors.error;
    }
    espui_widget_spec_t hint_spec = {
        .x = 28,
        .y = ATTENTION_HINT_Y,
        .width = 424,
        .height = ATTENTION_HINT_HEIGHT,
        .fg_color = hint_color,
        .transparent_bg = true,
        .font_scale_percent = 100,
        .align = ESPUI_TEXT_ALIGN_CENTER,
        .visible = true,
    };
    (void)s_state.renderer->widget_set_spec(s_state.attention_hint, &hint_spec);
    set_visible(s_state.attention_hint, true);
}

static void update_state(const espui_view_model_t *model)
{
    if (model == NULL || s_state.scene == NULL || s_state.renderer == NULL) {
        return;
    }

    update_ambient(model);
    update_focus(model);
    update_activity(model);
    update_attention(model);
    /* No full-screen commit here: widget_set_text/spec/visible invalidate
     * locally. Re-loading the root at 4 Hz caused avoidable full redraws. */
}

static esp_err_t terminal_init(const espui_context_t *ctx)
{
    if (ctx == NULL) {
        return ESP_ERR_INVALID_ARG;
    }

    destroy_scene();
    s_state.ctx = ctx;
    s_state.renderer = espui_renderer(ctx);
    s_state.tokens = espui_tokens(ctx);
    if (s_state.renderer == NULL || s_state.tokens == NULL ||
        s_state.renderer->scene_create == NULL ||
        s_state.renderer->scene_destroy == NULL ||
        s_state.renderer->scene_commit == NULL ||
        s_state.renderer->widget_create == NULL ||
        s_state.renderer->widget_set_text == NULL ||
        s_state.renderer->widget_set_spec == NULL ||
        s_state.renderer->widget_set_visible == NULL) {
        return ESP_ERR_NOT_SUPPORTED;
    }

    if (s_state.renderer->scene_create(&s_state.scene) != ESP_OK ||
        s_state.scene == NULL) {
        destroy_scene();
        return ESP_ERR_NO_MEM;
    }

    const espui_tokens_t *tokens = s_state.tokens;
    espui_widget_spec_t background = {
        .x = 0,
        .y = 0,
        .width = 480,
        .height = 480,
        .fg_color = tokens->colors.background,
        .bg_color = tokens->colors.background,
        .transparent_bg = false,
        .font_scale_percent = 100,
        .align = ESPUI_TEXT_ALIGN_LEFT,
        .visible = true,
    };
    s_state.background = s_state.renderer->widget_create(
        s_state.scene, ESPUI_WIDGET_RECT, &background);
    if (s_state.background == NULL) {
        destroy_scene();
        return ESP_ERR_NO_MEM;
    }

    espui_widget_spec_t core_panel_spec = {
        .x = 16,
        .y = 92,
        .width = 448,
        .height = 224,
        .fg_color = tokens->colors.surface,
        .bg_color = tokens->colors.surface,
        .transparent_bg = false,
        .font_scale_percent = 100,
        .align = ESPUI_TEXT_ALIGN_LEFT,
        .visible = true,
    };
    s_state.core_panel = make_rect(&s_state, &core_panel_spec);
    if (s_state.core_panel == NULL) {
        destroy_scene();
        return ESP_ERR_NO_MEM;
    }

    espui_widget_spec_t header_spec = {
        .x = 20,
        .y = 4,
        .width = 440,
        .height = 20,
        .fg_color = tokens->colors.secondary,
        .transparent_bg = true,
        .font_scale_percent = 100,
        .align = ESPUI_TEXT_ALIGN_LEFT,
        .visible = true,
    };
    espui_widget_spec_t scene_spec = {
        .x = 20,
        .y = 28,
        .width = 440,
        .height = 20,
        .fg_color = tokens->colors.muted,
        .transparent_bg = true,
        .font_scale_percent = 100,
        .align = ESPUI_TEXT_ALIGN_LEFT,
        .visible = true,
    };
    espui_widget_spec_t core_spec = {
        .x = 24,
        .y = 104,
        .width = 432,
        .height = 200,
        .fg_color = tokens->colors.primary,
        .transparent_bg = true,
        .font_scale_percent = 100,
        .align = ESPUI_TEXT_ALIGN_CENTER,
        .visible = true,
    };
    s_state.status_spec = (espui_widget_spec_t){
        .x = 28,
        .y = 334,
        .width = 424,
        .height = 40,
        .fg_color = tokens->colors.primary,
        .transparent_bg = true,
        .font_scale_percent = 120,
        .align = ESPUI_TEXT_ALIGN_CENTER,
        .visible = true,
    };
    s_state.strip_spec = (espui_widget_spec_t){
        .x = 28,
        .y = 386,
        .width = 424,
        .height = 24,
        .fg_color = tokens->colors.secondary,
        .transparent_bg = true,
        .font_scale_percent = 100,
        .align = ESPUI_TEXT_ALIGN_CENTER,
        .visible = true,
    };
    s_state.core_panel_spec = core_panel_spec;
    espui_widget_spec_t footer_spec = {
        .x = 20,
        .y = 446,
        .width = 440,
        .height = 22,
        .fg_color = tokens->colors.muted,
        .transparent_bg = true,
        .font_scale_percent = 100,
        .align = ESPUI_TEXT_ALIGN_CENTER,
        .visible = true,
    };

    espui_widget_spec_t canvas_spec = {
        .x = 24,
        .y = 24,
        .width = 432,
        .height = 432,
        .fg_color = tokens->colors.primary,
        .bg_color = tokens->colors.background,
        .transparent_bg = false,
        .font_scale_percent = 100,
        .align = ESPUI_TEXT_ALIGN_LEFT,
        .visible = false,
    };
    s_state.canvas = s_state.renderer->widget_create(
        s_state.scene, ESPUI_WIDGET_CANVAS, &canvas_spec);
    if (s_state.canvas != NULL &&
        phosphor_lvgl_port_attach(s_state.canvas, s_state.renderer) == ESP_OK) {
        s_state.canvas_attached = true;
    } else {
        s_state.canvas_attached = false;
    }

    s_state.header =
        make_widget(&s_state, &header_spec, "MEETAGENT // TERMINAL");
    s_state.scene_label =
        make_widget(&s_state, &scene_spec, "SCENE: AMBIENT");
    s_state.core = make_widget(&s_state, &core_spec, TERMINAL_CORE_ART);
    s_state.status =
        make_widget(&s_state, &s_state.status_spec, "SYSTEM IDLE");
    s_state.strip = make_widget(&s_state, &s_state.strip_spec,
                                "AGENTS 00   SESSIONS 00   ALERTS 00");
    s_state.footer =
        make_widget(&s_state, &footer_spec, "E4 // ESPUI SCENES");

    espui_widget_spec_t focus_spec = {
        .x = 28,
        .y = 78,
        .width = 424,
        .height = 26,
        .fg_color = tokens->colors.primary,
        .transparent_bg = true,
        .font_scale_percent = 100,
        .align = ESPUI_TEXT_ALIGN_LEFT,
        .visible = false,
    };
    s_state.focus_title = make_widget(&s_state, &focus_spec, "NO ACTIVE SESSION");
    focus_spec.y = 108;
    focus_spec.fg_color = tokens->colors.secondary;
    s_state.focus_agent = make_widget(&s_state, &focus_spec, "AGENT: -");
    focus_spec.y = 138;
    focus_spec.fg_color = tokens->colors.primary;
    s_state.focus_status = make_widget(&s_state, &focus_spec, "STATUS: -");
    focus_spec.y = 168;
    focus_spec.fg_color = tokens->colors.muted;
    s_state.focus_duration = make_widget(&s_state, &focus_spec, "DURATION: --:--");
    focus_spec.y = 198;
    focus_spec.height = 56;
    focus_spec.fg_color = tokens->colors.secondary;
    s_state.focus_recent = make_widget(&s_state, &focus_spec, "RECENT: NO EVENT");
    espui_widget_spec_t focus_action_bg_spec = {
        .x = FOCUS_INTERRUPT_X,
        .y = FOCUS_INTERRUPT_Y,
        .width = FOCUS_INTERRUPT_WIDTH,
        .height = FOCUS_INTERRUPT_HEIGHT,
        .fg_color = tokens->colors.surface,
        .bg_color = tokens->colors.surface,
        .transparent_bg = false,
        .font_scale_percent = 100,
        .align = ESPUI_TEXT_ALIGN_CENTER,
        .visible = false,
    };
    s_state.focus_action_bg = make_rect(&s_state, &focus_action_bg_spec);

    focus_spec.x = FOCUS_INTERRUPT_X;
    focus_spec.y = 330;
    focus_spec.width = FOCUS_INTERRUPT_WIDTH;
    focus_spec.height = 24;
    focus_spec.fg_color = tokens->colors.primary;
    focus_spec.align = ESPUI_TEXT_ALIGN_CENTER;
    s_state.focus_action_spec = focus_spec;
    s_state.focus_action =
        make_widget(&s_state, &focus_spec, "INTERRUPT");

    espui_widget_spec_t activity_spec = {
        .x = 28,
        .y = 78,
        .width = 424,
        .height = 24,
        .fg_color = tokens->colors.secondary,
        .transparent_bg = true,
        .font_scale_percent = 100,
        .align = ESPUI_TEXT_ALIGN_LEFT,
        .visible = false,
    };
    s_state.activity_title =
        make_widget(&s_state, &activity_spec, "RECENT ACTIVITY");
    activity_spec.y = 106;
    activity_spec.height = 300;
    activity_spec.fg_color = tokens->colors.primary;
    s_state.activity_body =
        make_widget(&s_state, &activity_spec, "NO RECENT EVENTS");

    espui_widget_spec_t attention_spec = {
        .x = 28,
        .y = 78,
        .width = 424,
        .height = 24,
        .fg_color = tokens->colors.warning,
        .transparent_bg = true,
        .font_scale_percent = 100,
        .align = ESPUI_TEXT_ALIGN_LEFT,
        .visible = false,
    };
    s_state.attention_title =
        make_widget(&s_state, &attention_spec, "ATTENTION REQUIRED");

    for (int i = 0; i < ATTENTION_ROWS; ++i) {
        espui_widget_spec_t row_spec = {
            .x = 28,
            .y = (int16_t)(ATTENTION_ROW_Y + i * ATTENTION_ROW_PITCH),
            .width = 424,
            .height = ATTENTION_ROW_HEIGHT,
            .fg_color = tokens->colors.secondary,
            .transparent_bg = true,
            .font_scale_percent = 100,
            .align = ESPUI_TEXT_ALIGN_LEFT,
            .visible = false,
        };
        s_state.attention_row[i] =
            make_widget(&s_state, &row_spec, "NO PENDING ATTENTION");
    }

    attention_spec.y = ATTENTION_DETAIL_Y;
    attention_spec.height = ATTENTION_DETAIL_HEIGHT;
    attention_spec.fg_color = tokens->colors.muted;
    s_state.attention_detail = make_widget(
        &s_state, &attention_spec, "NO PENDING ATTENTION");

    /* Backgrounds are created before labels so the text stays on top. */
    for (int i = 0; i < 3; ++i) {
        espui_widget_spec_t button_spec = {
            .x = ATTENTION_BUTTON_X(i),
            .y = ATTENTION_BUTTON_Y,
            .width = ATTENTION_BUTTON_WIDTH,
            .height = ATTENTION_BUTTON_HEIGHT,
            .fg_color = tokens->colors.surface,
            .bg_color = tokens->colors.surface,
            .transparent_bg = false,
            .font_scale_percent = 100,
            .align = ESPUI_TEXT_ALIGN_CENTER,
            .visible = false,
        };
        s_state.attention_button_spec[i] = button_spec;
        s_state.attention_button_bg[i] = make_rect(&s_state, &button_spec);
    }
    static const char *BUTTON_TEXT[3] = {"REJECT", "APPROVE ONCE",
                                         "APPROVE ALWAYS"};
    for (int i = 0; i < 3; ++i) {
        espui_widget_spec_t label_spec = s_state.attention_button_spec[i];
        label_spec.x = (int16_t)(label_spec.x + 4);
        label_spec.y = (int16_t)(label_spec.y + 10);
        label_spec.width = (int16_t)(label_spec.width - 8);
        label_spec.height = 24;
        label_spec.transparent_bg = true;
        label_spec.fg_color = tokens->colors.background;
        s_state.attention_button_label[i] =
            make_widget(&s_state, &label_spec, BUTTON_TEXT[i]);
    }

    espui_widget_spec_t hint_spec = {
        .x = 28,
        .y = ATTENTION_HINT_Y,
        .width = 424,
        .height = ATTENTION_HINT_HEIGHT,
        .fg_color = tokens->colors.muted,
        .transparent_bg = true,
        .font_scale_percent = 100,
        .align = ESPUI_TEXT_ALIGN_CENTER,
        .visible = false,
    };
    s_state.attention_hint =
        make_widget(&s_state, &hint_spec, "APPROVALS APPEAR HERE");
    if (s_state.header == NULL || s_state.scene_label == NULL ||
        s_state.core == NULL || s_state.status == NULL ||
        s_state.strip == NULL || s_state.footer == NULL ||
        s_state.core_panel == NULL || s_state.focus_title == NULL ||
        s_state.focus_agent == NULL || s_state.focus_status == NULL ||
        s_state.focus_duration == NULL || s_state.focus_recent == NULL ||
        s_state.focus_action_bg == NULL || s_state.focus_action == NULL ||
        s_state.activity_title == NULL || s_state.activity_body == NULL ||
        s_state.attention_title == NULL || s_state.attention_detail == NULL ||
        s_state.attention_hint == NULL) {
        destroy_scene();
        return ESP_ERR_NO_MEM;
    }
    for (int i = 0; i < ATTENTION_ROWS; ++i) {
        if (s_state.attention_row[i] == NULL) {
            destroy_scene();
            return ESP_ERR_NO_MEM;
        }
    }
    for (int i = 0; i < 3; ++i) {
        if (s_state.attention_button_bg[i] == NULL ||
            s_state.attention_button_label[i] == NULL) {
            destroy_scene();
            return ESP_ERR_NO_MEM;
        }
    }

    apply_scene_visibility(ESPUI_SCENE_AMBIENT);
    if (s_state.renderer->scene_commit(s_state.scene) != ESP_OK) {
        destroy_scene();
        return ESP_ERR_INVALID_STATE;
    }

    s_state.current_scene = ESPUI_SCENE_AMBIENT;
    phosphor_lvgl_port_set_active(s_state.canvas_attached);
    return ESP_OK;
}

static esp_err_t terminal_enter_scene(espui_scene_id_t scene,
                                      const espui_view_model_t *model)
{
    if (s_state.scene == NULL || s_state.renderer == NULL) {
        return ESP_ERR_INVALID_STATE;
    }
    s_state.current_scene = scene;
    s_state.always_confirm_armed = false;
    s_state.always_confirm_deadline_ms = 0;
    if (s_state.scene_label != NULL) {
        char buffer[ESPUI_LABEL_MAX];
        snprintf(buffer, sizeof(buffer), "SCENE: %s", espui_scene_name(scene));
        s_state.renderer->widget_set_text(s_state.scene_label, buffer);
    }
    apply_scene_visibility(scene);
    phosphor_lvgl_port_set_active(scene == ESPUI_SCENE_AMBIENT &&
                                  s_state.canvas_attached);
    /* The single root was loaded by scene_commit() in terminal_init(); scene
     * changes only toggle widget visibility and invalidate those areas. */
    update_state(model);
    return ESP_OK;
}

static void terminal_update(const espui_view_model_t *model)
{
    update_state(model);
}

static bool point_in_rect(uint16_t x, uint16_t y, int16_t rx, int16_t ry,
                          int16_t rw, int16_t rh)
{
    return x >= rx && x < (int32_t)rx + rw && y >= ry &&
           y < (int32_t)ry + rh;
}

static void focus_cycle_session(void)
{
    const espui_view_model_t *model = espui_view_model(s_state.ctx);
    if (model == NULL) {
        return;
    }
    const espui_session_view_t *active[ESPUI_MAX_SESSIONS];
    size_t count = collect_sessions(model, active, ESPUI_MAX_SESSIONS, true);
    if (count < 2) {
        return;
    }

    size_t current = count;
    for (size_t i = 0; i < count; ++i) {
        if (strcmp(active[i]->session_id, s_state.focus_session_id) == 0) {
            current = i;
            break;
        }
    }
    size_t next = current < count ? (current + 1) % count : 0;
    espui_strlcpy(s_state.focus_session_id, active[next]->session_id,
                  sizeof(s_state.focus_session_id));
    update_focus(model);
}

static void focus_handle_tap(uint16_t x, uint16_t y)
{
    const espui_view_model_t *model = espui_view_model(s_state.ctx);
    if (model == NULL) {
        return;
    }

    if (point_in_rect(x, y, FOCUS_INTERRUPT_X, FOCUS_INTERRUPT_Y,
                      FOCUS_INTERRUPT_WIDTH, FOCUS_INTERRUPT_HEIGHT)) {
        const espui_session_view_t *session = selected_session(model);
        if (!session_is_active(session) || s_state.interrupt_pending ||
            s_state.interrupt_sent) {
            return;
        }
        espui_action_t action = espui_action_interrupt(session->session_id);
        esp_err_t err = espui_dispatch_action(s_state.ctx, &action);
        if (err != ESP_OK) {
            espui_strlcpy(s_state.interrupt_status, "INTERRUPT BUSY",
                          sizeof(s_state.interrupt_status));
            update_focus(model);
            return;
        }
        espui_strlcpy(s_state.interrupt_session_id, session->session_id,
                      sizeof(s_state.interrupt_session_id));
        s_state.interrupt_pending = true;
        s_state.interrupt_sent = false;
        s_state.interrupt_status[0] = '\0';
        update_focus(model);
        return;
    }

    if (y >= FOCUS_CONTENT_Y0 && y < FOCUS_CONTENT_Y1) {
        focus_cycle_session();
    }
}

static void attention_handle_tap(uint16_t x, uint16_t y)
{
    const espui_view_model_t *model = espui_view_model(s_state.ctx);
    if (model == NULL) {
        return;
    }
    const espui_attention_view_t *items[ESPUI_MAX_ATTENTION];
    uint8_t count = collect_attention(model, items, ESPUI_MAX_ATTENTION);
    if (count == 0) {
        return;
    }

    if (y >= ATTENTION_ROW_Y &&
        y < ATTENTION_ROW_Y + ATTENTION_ROWS * ATTENTION_ROW_PITCH) {
        int row = (y - ATTENTION_ROW_Y) / ATTENTION_ROW_PITCH;
        if (row >= 0 && row < ATTENTION_ROWS && row < (int)count) {
            espui_strlcpy(s_state.selected_permission_id,
                          items[row]->permission_id,
                          sizeof(s_state.selected_permission_id));
            s_state.always_confirm_armed = false;
            s_state.attention_status[0] = '\0';
            update_attention(model);
        }
        return;
    }

    if (y < ATTENTION_BUTTON_Y ||
        y >= ATTENTION_BUTTON_Y + ATTENTION_BUTTON_HEIGHT) {
        return;
    }
    int button = -1;
    for (int i = 0; i < 3; ++i) {
        if (x >= ATTENTION_BUTTON_X(i) &&
            x < ATTENTION_BUTTON_X(i) + ATTENTION_BUTTON_WIDTH) {
            button = i;
            break;
        }
    }
    if (button < 0 || !s_state.selected_is_approval) {
        return;
    }
    if (s_state.approval_pending) {
        return;
    }
    if (s_state.selected_summary_blocked && button != 0) {
        /* Approve/Always are not offered for a truncated/empty summary. */
        return;
    }
    if (s_state.approval_acked &&
        strcmp(s_state.acked_permission_id,
               s_state.selected_permission_id) == 0) {
        return;
    }

    if (button == 2) {
        uint32_t now = espui_uptime_ms(s_state.ctx);
        if (!s_state.always_confirm_armed ||
            (int32_t)(now - s_state.always_confirm_deadline_ms) >= 0) {
            s_state.always_confirm_armed = true;
            s_state.always_confirm_deadline_ms = now + ALWAYS_CONFIRM_TIMEOUT_MS;
            s_state.attention_status[0] = '\0';
            update_attention(model);
            return;
        }
    } else {
        s_state.always_confirm_armed = false;
    }

    espui_permission_decision_t decision =
        button == 0 ? ESPUI_PERMISSION_REJECT
                    : (button == 1 ? ESPUI_PERMISSION_ONCE
                                   : ESPUI_PERMISSION_ALWAYS);
    espui_action_t action = espui_action_permission(
        s_state.selected_permission_id, decision);
    esp_err_t err = espui_dispatch_action(s_state.ctx, &action);
    if (err != ESP_OK) {
        espui_strlcpy(s_state.attention_status, "SUBMIT FAILED // RETRY",
                      sizeof(s_state.attention_status));
        update_attention(model);
        return;
    }

    s_state.approval_pending = true;
    s_state.pending_decision = decision;
    espui_strlcpy(s_state.pending_permission_id,
                  s_state.selected_permission_id,
                  sizeof(s_state.pending_permission_id));
    s_state.always_confirm_armed = false;
    s_state.attention_status[0] = '\0';
    update_attention(model);
}

static void terminal_on_tick(uint32_t delta_ms)
{
    (void)delta_ms;
    if (!s_state.always_confirm_armed) {
        return;
    }
    uint32_t now = espui_uptime_ms(s_state.ctx);
    if ((int32_t)(now - s_state.always_confirm_deadline_ms) < 0) {
        return;
    }
    s_state.always_confirm_armed = false;
    if (s_state.current_scene == ESPUI_SCENE_ATTENTION) {
        const espui_view_model_t *model = espui_view_model(s_state.ctx);
        if (model != NULL) {
            update_attention(model);
        }
    }
}

static void terminal_on_input(const espui_input_event_t *event)
{
    if (event == NULL || event->type != ESPUI_INPUT_TAP) {
        return;
    }
    if (s_state.current_scene == ESPUI_SCENE_FOCUS) {
        focus_handle_tap(event->x, event->y);
    } else if (s_state.current_scene == ESPUI_SCENE_ATTENTION) {
        attention_handle_tap(event->x, event->y);
    }
}

static void terminal_on_action_result(const espui_action_result_t *result)
{
    if (result == NULL || s_state.scene == NULL || s_state.renderer == NULL) {
        return;
    }
    const espui_action_t *action = &result->action;
    const char *status_text = "ERR";
    if (result->status == ESPUI_ACTION_STATUS_OK) {
        status_text = "ACK";
    } else if (result->status == ESPUI_ACTION_STATUS_REJECTED) {
        status_text = "REJECTED";
    } else if (result->status == ESPUI_ACTION_STATUS_TIMEOUT) {
        status_text = "TIMEOUT";
    }
    char buffer[ESPUI_LABEL_MAX];
    snprintf(buffer, sizeof(buffer), "%s // %.48s", status_text,
             result->message[0] != '\0'
                 ? result->message
                 : espui_action_type_name(action->type));
    set_text(s_state.footer, buffer);

    if (action->type == ESPUI_ACTION_RESPOND_PERMISSION) {
        bool selected_match =
            strcmp(action->permission_id, s_state.selected_permission_id) == 0;
        bool pending_match =
            s_state.approval_pending &&
            strcmp(action->permission_id, s_state.pending_permission_id) == 0;
        if (pending_match) {
            s_state.approval_pending = false;
        }
        if (selected_match) {
            if (result->status == ESPUI_ACTION_STATUS_OK) {
                espui_strlcpy(s_state.attention_status, "ACK",
                              sizeof(s_state.attention_status));
            } else if (result->status == ESPUI_ACTION_STATUS_TIMEOUT) {
                espui_strlcpy(s_state.attention_status, "TIMEOUT",
                              sizeof(s_state.attention_status));
            } else if (result->status == ESPUI_ACTION_STATUS_REJECTED) {
                snprintf(s_state.attention_status,
                         sizeof(s_state.attention_status), "REJECTED // %.36s",
                         result->message[0] != '\0' ? result->message
                                                    : "already handled");
            } else {
                snprintf(s_state.attention_status,
                         sizeof(s_state.attention_status), "ERR // %.40s",
                         result->message[0] != '\0' ? result->message
                                                    : "failed");
            }
        }
        if (pending_match && result->status == ESPUI_ACTION_STATUS_OK) {
            /* Do not remove attention optimistically; wait for
             * permission.resolved / the next snapshot. */
            s_state.approval_acked = true;
            espui_strlcpy(s_state.acked_permission_id, action->permission_id,
                          sizeof(s_state.acked_permission_id));
        }
        if (s_state.current_scene == ESPUI_SCENE_ATTENTION) {
            const espui_view_model_t *model = espui_view_model(s_state.ctx);
            if (model != NULL) {
                update_attention(model);
            }
        }
    } else if (action->type == ESPUI_ACTION_INTERRUPT_SESSION) {
        if (strcmp(action->session_id, s_state.interrupt_session_id) == 0) {
            s_state.interrupt_pending = false;
            s_state.interrupt_status[0] = '\0';
            if (result->status == ESPUI_ACTION_STATUS_OK) {
                s_state.interrupt_sent = true;
            } else if (result->status == ESPUI_ACTION_STATUS_TIMEOUT) {
                espui_strlcpy(s_state.interrupt_status, "INTERRUPT TIMEOUT",
                              sizeof(s_state.interrupt_status));
            } else if (result->status == ESPUI_ACTION_STATUS_REJECTED) {
                espui_strlcpy(s_state.interrupt_status, "INTERRUPT REJECTED",
                              sizeof(s_state.interrupt_status));
            } else {
                espui_strlcpy(s_state.interrupt_status, "INTERRUPT FAILED",
                              sizeof(s_state.interrupt_status));
            }
        }
        if (s_state.current_scene == ESPUI_SCENE_FOCUS) {
            const espui_view_model_t *model = espui_view_model(s_state.ctx);
            if (model != NULL) {
                update_focus(model);
            }
        }
    }
    /* widget_set_text invalidates its own area; no full-screen commit here. */
}

static void terminal_destroy(void)
{
    destroy_scene();
}

static const espui_theme_ops_t s_ops = {
    .init = terminal_init,
    .enter_scene = terminal_enter_scene,
    .update = terminal_update,
    .on_tick = terminal_on_tick,
    .on_input = terminal_on_input,
    .on_action_result = terminal_on_action_result,
    .destroy = terminal_destroy,
};

static const espui_theme_descriptor_t s_theme = {
    .manifest = &s_manifest,
    .ops = &s_ops,
    .tokens = &espui_tokens_terminal_default_values,
};

const espui_theme_descriptor_t *espui_theme_terminal_aesthetic(void)
{
    return &s_theme;
}
