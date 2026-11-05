/*
 * Built-in E2 safe theme.
 *
 * Deliberately simple: one screen, fixed labels, no animation. It must keep
 * working when another theme is broken, so it only uses the renderer API.
 */
#include <stdio.h>
#include <string.h>

#include "espui.h"
#include "espui_theme.h"

static const espui_manifest_t s_manifest = {
    .theme_id = "safe_fallback",
    .name = "Safe Fallback",
    .version = "1.0.0",
    .api_version = ESPUI_THEME_API_VERSION,
    .renderer_id = "builtin.label",
    .display =
        {
            .orientation = ESPUI_ORIENTATION_PORTRAIT,
            .min_width = 320,
            .min_height = 480,
        },
    .scene_mask = ESPUI_SCENE_MASK_ALL,
    .configurable_mask = ESPUI_CONFIG_BRIGHTNESS | ESPUI_CONFIG_MOTION_ENABLED |
                         ESPUI_CONFIG_NIGHT_MODE | ESPUI_CONFIG_VISUAL_DENSITY,
    .motion_profile_id = "motion_none",
};

typedef struct {
    const espui_context_t *ctx;
    const espui_renderer_ops_t *renderer;
    const espui_tokens_t *tokens;
    espui_ui_scene_t *scene;
    espui_ui_widget_t *background;
    espui_ui_widget_t *title;
    espui_ui_widget_t *scene_label;
    espui_ui_widget_t *status;
    espui_ui_widget_t *detail;
    espui_ui_widget_t *footer;
    espui_widget_spec_t status_spec;
    espui_scene_id_t current_scene;
    char status_text[ESPUI_LABEL_MAX];
    char detail_text[ESPUI_LABEL_MAX];
} safe_theme_state_t;

static safe_theme_state_t s_state;

static void destroy_scene(void)
{
    if (s_state.scene != NULL && s_state.renderer != NULL &&
        s_state.renderer->scene_destroy != NULL) {
        s_state.renderer->scene_destroy(s_state.scene);
    }
    memset(&s_state, 0, sizeof(s_state));
}

static espui_ui_widget_t *make_widget(safe_theme_state_t *state,
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

static void update_status(const espui_view_model_t *model)
{
    uint16_t approvals = model->approval_count;
    uint16_t active_sessions = espui_view_model_active_session_count(model);
    const char *status = "IDLE";
    uint32_t color = s_state.tokens->colors.primary;

    if (model->node.status == ESPUI_NODE_OFFLINE) {
        status = "OFFLINE";
        color = s_state.tokens->colors.error;
    } else if (model->node.status == ESPUI_NODE_CONNECTING) {
        status = "CONNECTING";
    } else if (model->node.status == ESPUI_NODE_SYNCING) {
        status = "SYNCING";
    } else if (approvals > 0) {
        status = "ACTION REQUIRED";
        color = s_state.tokens->colors.warning;
    } else if (model->error_count > 0) {
        status = "ATTENTION";
        color = s_state.tokens->colors.error;
    } else if (active_sessions > 0) {
        status = "ACTIVE";
    }

    snprintf(s_state.status_text, sizeof(s_state.status_text), "%s", status);
    snprintf(s_state.detail_text, sizeof(s_state.detail_text),
             "AGENTS %u / SESSIONS %u / ALERTS %u",
             (unsigned)espui_view_model_agent_count(model),
             (unsigned)active_sessions,
             (unsigned)(approvals + model->error_count));

    s_state.status_spec.fg_color = color;
    s_state.renderer->widget_set_spec(s_state.status, &s_state.status_spec);
    s_state.renderer->widget_set_text(s_state.status, s_state.status_text);
    s_state.renderer->widget_set_text(s_state.detail, s_state.detail_text);
    if (s_state.scene != NULL) {
        s_state.renderer->scene_commit(s_state.scene);
    }
}

static esp_err_t safe_init(const espui_context_t *ctx)
{
    if (ctx == NULL) {
        return ESP_ERR_INVALID_ARG;
    }

    destroy_scene();
    s_state.ctx = ctx;
    s_state.renderer = espui_renderer(ctx);
    if (s_state.renderer == NULL || s_state.renderer->scene_create == NULL ||
        s_state.renderer->scene_destroy == NULL ||
        s_state.renderer->scene_commit == NULL ||
        s_state.renderer->widget_create == NULL ||
        s_state.renderer->widget_set_text == NULL ||
        s_state.renderer->widget_set_spec == NULL ||
        s_state.renderer->widget_set_visible == NULL) {
        return ESP_ERR_NOT_SUPPORTED;
    }

    const espui_tokens_t *tokens = espui_tokens(ctx);
    if (tokens == NULL) {
        return ESP_ERR_INVALID_STATE;
    }
    s_state.tokens = tokens;

    if (s_state.renderer->scene_create(&s_state.scene) != ESP_OK ||
        s_state.scene == NULL) {
        destroy_scene();
        return ESP_ERR_NO_MEM;
    }

    espui_widget_spec_t background = {
        .x = 0,
        .y = 0,
        .width = 320,
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

    espui_widget_spec_t title_spec = {
        .x = 16,
        .y = 20,
        .width = 288,
        .height = 28,
        .fg_color = tokens->colors.primary,
        .transparent_bg = true,
        .font_scale_percent = 100,
        .align = ESPUI_TEXT_ALIGN_LEFT,
        .visible = true,
    };
    espui_widget_spec_t scene_spec = {
        .x = 16,
        .y = 54,
        .width = 288,
        .height = 24,
        .fg_color = tokens->colors.secondary,
        .transparent_bg = true,
        .font_scale_percent = 100,
        .align = ESPUI_TEXT_ALIGN_LEFT,
        .visible = true,
    };
    s_state.status_spec = (espui_widget_spec_t){
        .x = 16,
        .y = 120,
        .width = 288,
        .height = 64,
        .fg_color = tokens->colors.primary,
        .transparent_bg = true,
        .font_scale_percent = 140,
        .align = ESPUI_TEXT_ALIGN_LEFT,
        .visible = true,
    };
    espui_widget_spec_t detail_spec = {
        .x = 16,
        .y = 210,
        .width = 288,
        .height = 120,
        .fg_color = tokens->colors.muted,
        .transparent_bg = true,
        .font_scale_percent = 100,
        .align = ESPUI_TEXT_ALIGN_LEFT,
        .visible = true,
    };
    espui_widget_spec_t footer_spec = {
        .x = 16,
        .y = 430,
        .width = 288,
        .height = 24,
        .fg_color = tokens->colors.muted,
        .transparent_bg = true,
        .font_scale_percent = 100,
        .align = ESPUI_TEXT_ALIGN_LEFT,
        .visible = true,
    };

    s_state.title = make_widget(&s_state, &title_spec, "MEETAGENT // SAFE MODE");
    s_state.scene_label = make_widget(&s_state, &scene_spec, "AMBIENT");
    s_state.status = make_widget(&s_state, &s_state.status_spec, "STARTING");
    s_state.detail = make_widget(&s_state, &detail_spec, "Waiting for state");
    s_state.footer = make_widget(&s_state, &footer_spec, "FALLBACK THEME");
    if (s_state.title == NULL || s_state.scene_label == NULL ||
        s_state.status == NULL || s_state.detail == NULL ||
        s_state.footer == NULL) {
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

static esp_err_t safe_enter_scene(espui_scene_id_t scene,
                                  const espui_view_model_t *model)
{
    if (s_state.scene == NULL || s_state.renderer == NULL) {
        return ESP_ERR_INVALID_STATE;
    }
    s_state.current_scene = scene;
    if (s_state.scene_label != NULL) {
        char buffer[ESPUI_LABEL_MAX];
        snprintf(buffer, sizeof(buffer), "%s", espui_scene_name(scene));
        s_state.renderer->widget_set_text(s_state.scene_label, buffer);
    }
    if (model != NULL) {
        update_status(model);
    }
    return ESP_OK;
}

static void safe_update(const espui_view_model_t *model)
{
    if (model != NULL && s_state.scene != NULL && s_state.renderer != NULL) {
        update_status(model);
    }
}

static void safe_on_tick(uint32_t delta_ms)
{
    (void)delta_ms;
}

static void safe_on_input(const espui_input_event_t *event)
{
    (void)event;
}

static void safe_on_action_result(const espui_action_result_t *result)
{
    if (result == NULL || s_state.scene == NULL || s_state.renderer == NULL) {
        return;
    }
    char buffer[ESPUI_LABEL_MAX];
    snprintf(buffer, sizeof(buffer), "%s: %.48s",
             result->status == ESPUI_ACTION_STATUS_OK ? "OK" : "FAILED",
             result->message[0] != '\0' ? result->message
                                        : espui_action_type_name(result->action.type));
    s_state.renderer->widget_set_text(s_state.detail, buffer);
    s_state.renderer->scene_commit(s_state.scene);
}

static void safe_destroy(void)
{
    destroy_scene();
}

static const espui_theme_ops_t s_ops = {
    .init = safe_init,
    .enter_scene = safe_enter_scene,
    .update = safe_update,
    .on_tick = safe_on_tick,
    .on_input = safe_on_input,
    .on_action_result = safe_on_action_result,
    .destroy = safe_destroy,
};

static const espui_theme_descriptor_t s_theme = {
    .manifest = &s_manifest,
    .ops = &s_ops,
    .tokens = &espui_tokens_safe_default_values,
};

const espui_theme_descriptor_t *espui_theme_safe(void)
{
    return &s_theme;
}
