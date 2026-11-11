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
    espui_ui_widget_t *info_panel;
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
    espui_ui_widget_t *focus_action;
    espui_ui_widget_t *activity_title;
    espui_ui_widget_t *activity_body;
    espui_ui_widget_t *attention_title;
    espui_ui_widget_t *attention_list;
    espui_ui_widget_t *attention_detail;
    espui_widget_spec_t status_spec;
    espui_widget_spec_t strip_spec;
    espui_widget_spec_t core_panel_spec;
    bool canvas_attached;
    espui_scene_id_t current_scene;
} terminal_theme_state_t;

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
    set_visible(s_state.info_panel, false);
    set_visible(s_state.focus_title, focus);
    set_visible(s_state.focus_agent, focus);
    set_visible(s_state.focus_status, focus);
    set_visible(s_state.focus_duration, focus);
    set_visible(s_state.focus_recent, focus);
    set_visible(s_state.focus_action, focus);
    set_visible(s_state.activity_title, activity);
    set_visible(s_state.activity_body, activity);
    set_visible(s_state.attention_title, attention);
    set_visible(s_state.attention_list, attention);
    set_visible(s_state.attention_detail, attention);

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


static const espui_session_view_t *selected_session(
    const espui_view_model_t *model)
{
    const espui_session_view_t *list[ESPUI_MAX_SESSIONS];
    size_t count = 0;
    size_t index = 0;

    /* Active sessions first, then completed/interrupted/unknown. */
    for (size_t i = 0; i < ESPUI_MAX_SESSIONS; ++i) {
        const espui_session_view_t *session = &model->sessions[i];
        if (!session->in_use) {
            continue;
        }
        if (session->status == ESPUI_SESSION_RUNNING ||
            session->status == ESPUI_SESSION_WAITING_APPROVAL) {
            list[count++] = session;
        }
    }
    for (size_t i = 0; i < ESPUI_MAX_SESSIONS; ++i) {
        const espui_session_view_t *session = &model->sessions[i];
        if (!session->in_use) {
            continue;
        }
        if (session->status == ESPUI_SESSION_RUNNING ||
            session->status == ESPUI_SESSION_WAITING_APPROVAL) {
            continue;
        }
        list[count++] = session;
    }
    if (count == 0) {
        return NULL;
    }
    index = espui_focus_index(s_state.ctx) % count;
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

static void update_focus(const espui_view_model_t *model)
{
    const espui_session_view_t *session = selected_session(model);
    if (session == NULL) {
        set_text(s_state.focus_title, "NO ACTIVE SESSION");
        set_text(s_state.focus_agent, "AGENT: -");
        set_text(s_state.focus_status, "STATUS: -");
        set_text(s_state.focus_duration, "DURATION: --:--");
        set_text(s_state.focus_recent, "RECENT: NO EVENT");
        set_text(s_state.focus_action, "[ INTERRUPT: STAGE 3 ]");
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
    set_text(s_state.focus_action, "[ INTERRUPT: STAGE 3 ]");
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

static void update_attention(const espui_view_model_t *model)
{
    char list[512];
    size_t offset = 0;
    uint8_t shown = 0;
    uint8_t total = 0;
    const espui_attention_view_t *first = NULL;

    for (size_t i = 0; i < ESPUI_MAX_ATTENTION; ++i) {
        const espui_attention_view_t *attention = &model->attention[i];
        if (!attention->in_use) {
            continue;
        }
        ++total;
        if (first == NULL) {
            first = attention;
        }
        if (shown >= 3 || offset >= sizeof(list)) {
            continue;
        }
        uint32_t age_s = 0;
        if (model->uptime_ms > attention->created_at_ms) {
            age_s = (model->uptime_ms - attention->created_at_ms) / 1000u;
        }
        int written = snprintf(
            list + offset, sizeof(list) - offset, "[%s] %s  %.18s  T+%lus\n",
            espui_attention_kind_name(attention->kind),
            attention->session_id[0] != '\0' ? attention->session_id : "-",
            attention->summary[0] != '\0' ? attention->summary : "(no summary)",
            (unsigned long)age_s);
        if (written < 0) {
            break;
        }
        offset += (size_t)written;
        ++shown;
    }

    if (total == 0) {
        set_text(s_state.attention_list, "NO PENDING ATTENTION");
        set_text(s_state.attention_detail, "DETAIL / ACTIONS: STAGE 3");
        return;
    }
    if (total > shown && offset < sizeof(list)) {
        (void)snprintf(list + offset, sizeof(list) - offset, "+%u MORE\n",
                       (unsigned)(total - shown));
    }
    list[sizeof(list) - 1] = '\0';
    set_text(s_state.attention_list, list);

    char detail[256];
    if (first != NULL) {
        snprintf(detail, sizeof(detail), "PERMISSION: %s\nSESSION: %s\n%.36s\n"
                                         "DETAIL / ACTIONS: STAGE 3",
                 first->permission_id, first->session_id, first->summary);
    } else {
        snprintf(detail, sizeof(detail), "NO PENDING ATTENTION");
    }
    set_text(s_state.attention_detail, detail);
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
    (void)s_state.renderer->scene_commit(s_state.scene);
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

    espui_widget_spec_t info_panel_spec = {
        .x = 16,
        .y = 326,
        .width = 448,
        .height = 96,
        .fg_color = tokens->colors.surface,
        .bg_color = tokens->colors.surface,
        .transparent_bg = false,
        .font_scale_percent = 100,
        .align = ESPUI_TEXT_ALIGN_LEFT,
        .visible = true,
    };
    s_state.info_panel = make_rect(&s_state, &info_panel_spec);
    if (s_state.info_panel == NULL) {
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
    focus_spec.y = 330;
    focus_spec.height = 28;
    focus_spec.fg_color = tokens->colors.warning;
    focus_spec.align = ESPUI_TEXT_ALIGN_CENTER;
    s_state.focus_action =
        make_widget(&s_state, &focus_spec, "[ INTERRUPT: STAGE 3 ]");

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
    attention_spec.y = 106;
    attention_spec.height = 170;
    s_state.attention_list =
        make_widget(&s_state, &attention_spec, "NO PENDING ATTENTION");
    attention_spec.y = 282;
    attention_spec.height = 118;
    attention_spec.fg_color = tokens->colors.muted;
    s_state.attention_detail = make_widget(
        &s_state, &attention_spec, "DETAIL / ACTIONS: STAGE 3");
    if (s_state.header == NULL || s_state.scene_label == NULL ||
        s_state.core == NULL || s_state.status == NULL ||
        s_state.strip == NULL || s_state.footer == NULL ||
        s_state.core_panel == NULL || s_state.info_panel == NULL ||
        s_state.focus_title == NULL || s_state.focus_agent == NULL ||
        s_state.focus_status == NULL || s_state.focus_duration == NULL ||
        s_state.focus_recent == NULL || s_state.focus_action == NULL ||
        s_state.activity_title == NULL || s_state.activity_body == NULL ||
        s_state.attention_title == NULL || s_state.attention_list == NULL ||
        s_state.attention_detail == NULL) {
        destroy_scene();
        return ESP_ERR_NO_MEM;
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
    if (s_state.scene_label != NULL) {
        char buffer[ESPUI_LABEL_MAX];
        snprintf(buffer, sizeof(buffer), "SCENE: %s", espui_scene_name(scene));
        s_state.renderer->widget_set_text(s_state.scene_label, buffer);
    }
    apply_scene_visibility(scene);
    phosphor_lvgl_port_set_active(scene == ESPUI_SCENE_AMBIENT &&
                                  s_state.canvas_attached);
    update_state(model);
    return ESP_OK;
}

static void terminal_update(const espui_view_model_t *model)
{
    update_state(model);
}

static void terminal_on_tick(uint32_t delta_ms)
{
    (void)delta_ms;
}

static void terminal_on_input(const espui_input_event_t *event)
{
    (void)event;
}

static void terminal_on_action_result(const espui_action_result_t *result)
{
    if (result == NULL || s_state.scene == NULL || s_state.renderer == NULL) {
        return;
    }
    char buffer[ESPUI_LABEL_MAX];
    snprintf(buffer, sizeof(buffer), "%s // %.48s",
             result->status == ESPUI_ACTION_STATUS_OK ? "ACK" : "ERR",
             result->message[0] != '\0' ? result->message
                                        : espui_action_type_name(result->action.type));
    s_state.renderer->widget_set_text(s_state.footer, buffer);
    s_state.renderer->scene_commit(s_state.scene);
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
