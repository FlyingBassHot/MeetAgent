/*
 * Host-test shim for ESPUI context accessors.
 *
 * Theme descriptors are linked into the host tests to verify manifests and
 * registry wiring, but their rendering ops are never invoked on the host.
 * The real implementations live in components/espui/core/espui.c.
 */
#include "espui.h"

static const espui_view_model_t *s_test_model;
static const espui_renderer_ops_t *s_test_renderer;
static uint16_t s_test_focus_index;

void host_espui_stub_set_model(const espui_view_model_t *model)
{
    s_test_model = model;
}

void host_espui_stub_set_renderer(const espui_renderer_ops_t *ops)
{
    s_test_renderer = ops;
}

void host_espui_stub_set_focus_index(uint16_t index)
{
    s_test_focus_index = index;
}

const espui_view_model_t *espui_view_model(const espui_context_t *ctx)
{
    (void)ctx;
    return s_test_model;
}

const espui_renderer_ops_t *espui_renderer(const espui_context_t *ctx)
{
    (void)ctx;
    return s_test_renderer;
}

const espui_tokens_t *espui_tokens(const espui_context_t *ctx)
{
    (void)ctx;
    return espui_tokens_safe_default();
}

const espui_motion_profile_t *espui_motion_profile(const espui_context_t *ctx)
{
    (void)ctx;
    return espui_motion_profile_default();
}

const espui_display_info_t *espui_display(const espui_context_t *ctx)
{
    (void)ctx;
    return NULL;
}

espui_scene_id_t espui_current_scene(const espui_context_t *ctx)
{
    (void)ctx;
    return ESPUI_SCENE_AMBIENT;
}

esp_err_t espui_request_scene(const espui_context_t *ctx,
                              espui_scene_id_t scene)
{
    (void)ctx;
    (void)scene;
    return ESP_ERR_NOT_SUPPORTED;
}

esp_err_t espui_dispatch_action(const espui_context_t *ctx,
                                const espui_action_t *action)
{
    (void)ctx;
    (void)action;
    return ESP_ERR_NOT_SUPPORTED;
}

bool espui_motion_enabled(const espui_context_t *ctx)
{
    (void)ctx;
    return true;
}

uint16_t espui_focus_index(const espui_context_t *ctx)
{
    (void)ctx;
    return s_test_focus_index;
}

uint32_t espui_uptime_ms(const espui_context_t *ctx)
{
    (void)ctx;
    return 0;
}
