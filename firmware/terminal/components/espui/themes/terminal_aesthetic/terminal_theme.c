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
    espui_widget_spec_t status_spec;
    espui_scene_id_t current_scene;
} terminal_theme_state_t;

static terminal_theme_state_t s_state;

static void destroy_scene(void)
{
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

static void update_state(const espui_view_model_t *model)
{
    if (model == NULL || s_state.scene == NULL || s_state.renderer == NULL) {
        return;
    }

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
    s_state.renderer->widget_set_spec(s_state.status, &s_state.status_spec);
    s_state.renderer->widget_set_text(s_state.status, status);

    char strip[ESPUI_LABEL_MAX];
    snprintf(strip, sizeof(strip), "AGENTS %02u   SESSIONS %02u   ALERTS %02u",
             (unsigned)espui_view_model_agent_count(model),
             (unsigned)active_sessions,
             (unsigned)(model->approval_count + model->error_count));
    s_state.renderer->widget_set_text(s_state.strip, strip);
    s_state.renderer->scene_commit(s_state.scene);
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
        .y = 18,
        .width = 440,
        .height = 28,
        .fg_color = tokens->colors.secondary,
        .transparent_bg = true,
        .font_scale_percent = 100,
        .align = ESPUI_TEXT_ALIGN_LEFT,
        .visible = true,
    };
    espui_widget_spec_t scene_spec = {
        .x = 20,
        .y = 50,
        .width = 440,
        .height = 24,
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
    espui_widget_spec_t strip_spec = {
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

    s_state.header =
        make_widget(&s_state, &header_spec, "MEETAGENT // TERMINAL");
    s_state.scene_label =
        make_widget(&s_state, &scene_spec, "SCENE: AMBIENT");
    s_state.core = make_widget(&s_state, &core_spec, TERMINAL_CORE_ART);
    s_state.status =
        make_widget(&s_state, &s_state.status_spec, "SYSTEM IDLE");
    s_state.strip = make_widget(&s_state, &strip_spec,
                                "AGENTS 00   SESSIONS 00   ALERTS 00");
    s_state.footer =
        make_widget(&s_state, &footer_spec, "E2 MOCK // ASCII RENDERER E3");
    if (s_state.header == NULL || s_state.scene_label == NULL ||
        s_state.core == NULL || s_state.status == NULL ||
        s_state.strip == NULL || s_state.footer == NULL ||
        s_state.core_panel == NULL || s_state.info_panel == NULL) {
        destroy_scene();
        return ESP_ERR_NO_MEM;
    }

    if (s_state.renderer->scene_commit(s_state.scene) != ESP_OK) {
        destroy_scene();
        return ESP_ERR_INVALID_STATE;
    }

    s_state.current_scene = ESPUI_SCENE_AMBIENT;
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
