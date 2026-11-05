#include "ascii_renderer.h"

#include <stdio.h>
#include <string.h>

#include "ascii_canvas.h"
#include "espui_types.h"

static const char *STATE_NAMES[ESPUI_VISUAL_STATE_COUNT] = {
    "idle", "working", "completed", "approval", "error", "offline",
};

static const char *STATE_LABELS[ESPUI_VISUAL_STATE_COUNT] = {
    "IDLE", "WORKING", "COMPLETED", "APPROVAL", "ERROR", "OFFLINE",
};

/* Base motion before profile/density/user scaling. */
static const espui_ascii_motion_t BASE_MOTION[ESPUI_VISUAL_STATE_COUNT] = {
    [ESPUI_VISUAL_IDLE] = {25, 25, 18},
    [ESPUI_VISUAL_WORKING] = {100, 80, 70},
    [ESPUI_VISUAL_COMPLETED] = {60, 65, 55},
    [ESPUI_VISUAL_APPROVAL] = {35, 50, 25},
    [ESPUI_VISUAL_ERROR] = {20, 70, 40},
    [ESPUI_VISUAL_OFFLINE] = {0, 15, 0},
};

static bool state_is_valid(espui_visual_state_t state)
{
    return (unsigned)state < (unsigned)ESPUI_VISUAL_STATE_COUNT;
}

static uint32_t isqrt_u32(uint32_t value)
{
    uint32_t result = 0;
    uint32_t bit = 1u << 30;
    while (bit > value) {
        bit >>= 2;
    }
    while (bit != 0) {
        if (value >= result + bit) {
            value -= result + bit;
            result = (result >> 1) + bit;
        } else {
            result >>= 1;
        }
        bit >>= 2;
    }
    return result;
}

static uint32_t mix32(uint32_t value)
{
    value ^= value >> 16;
    value *= 0x7FEB352Du;
    value ^= value >> 15;
    value *= 0x846CA68Bu;
    value ^= value >> 16;
    return value;
}

static uint32_t field_hash(uint32_t x, uint32_t y, uint32_t phase)
{
    return mix32(x * 0x9E3779B1u ^ y * 0x85EBCA77u ^ phase * 0xC2B2AE3Du);
}

static uint16_t count_agent_errors(const espui_view_model_t *model)
{
    uint16_t count = 0;
    for (size_t i = 0; i < ESPUI_MAX_AGENTS; ++i) {
        if (model->agents[i].in_use &&
            model->agents[i].status == ESPUI_AGENT_ERROR) {
            ++count;
        }
    }
    return count;
}

static bool any_agent_busy(const espui_view_model_t *model)
{
    for (size_t i = 0; i < ESPUI_MAX_AGENTS; ++i) {
        if (model->agents[i].in_use &&
            model->agents[i].status == ESPUI_AGENT_BUSY) {
            return true;
        }
    }
    return false;
}

const char *espui_visual_state_name(espui_visual_state_t state)
{
    return state_is_valid(state) ? STATE_NAMES[state] : "offline";
}

const char *espui_visual_state_label(espui_visual_state_t state)
{
    return state_is_valid(state) ? STATE_LABELS[state] : "OFFLINE";
}

espui_ascii_color_t espui_ascii_state_color(espui_visual_state_t state)
{
    switch (state) {
    case ESPUI_VISUAL_IDLE:
    case ESPUI_VISUAL_WORKING:
        return ESPUI_ASCII_COLOR_PRIMARY;
    case ESPUI_VISUAL_COMPLETED:
        return ESPUI_ASCII_COLOR_HIGHLIGHT;
    case ESPUI_VISUAL_APPROVAL:
        return ESPUI_ASCII_COLOR_WARNING;
    case ESPUI_VISUAL_ERROR:
        return ESPUI_ASCII_COLOR_ERROR;
    case ESPUI_VISUAL_OFFLINE:
    default:
        return ESPUI_ASCII_COLOR_MUTED;
    }
}

espui_visual_state_t espui_ascii_resolve_visual_state(
    const espui_view_model_t *model, espui_scene_id_t scene)
{
    (void)scene; /* Reserved for scene-specific layout emphasis. */

    if (model == NULL) {
        return ESPUI_VISUAL_OFFLINE;
    }
    if (model->node.status == ESPUI_NODE_OFFLINE ||
        model->node.status == ESPUI_NODE_CONNECTING ||
        !model->node.connected) {
        return ESPUI_VISUAL_OFFLINE;
    }

    /* Safety-relevant approval outranks error/completion. */
    if (model->approval_count > 0) {
        return ESPUI_VISUAL_APPROVAL;
    }
    if (model->error_count > 0 || count_agent_errors(model) > 0) {
        return ESPUI_VISUAL_ERROR;
    }
    if (model->completion_count > 0) {
        return ESPUI_VISUAL_COMPLETED;
    }
    if (model->node.status == ESPUI_NODE_SYNCING ||
        espui_view_model_active_session_count(model) > 0 ||
        any_agent_busy(model)) {
        return ESPUI_VISUAL_WORKING;
    }
    return ESPUI_VISUAL_IDLE;
}

espui_ascii_status_t espui_ascii_status_from_view_model(
    const espui_view_model_t *model, espui_scene_id_t scene)
{
    espui_ascii_status_t status;
    memset(&status, 0, sizeof(status));
    status.state = ESPUI_VISUAL_OFFLINE;
    status.node_status = ESPUI_NODE_OFFLINE;
    if (model == NULL) {
        return status;
    }

    status.node_status = model->node.status;
    status.connected = model->node.connected;
    status.approval_count = model->approval_count;
    status.completion_count = model->completion_count;
    status.active_sessions = espui_view_model_active_session_count(model);
    status.agent_count = espui_view_model_agent_count(model);

    uint16_t agent_errors = count_agent_errors(model);
    status.error_count =
        model->error_count > agent_errors ? model->error_count : agent_errors;
    status.state = espui_ascii_resolve_visual_state(model, scene);
    return status;
}

char espui_ascii_glyph_for_intensity(const espui_ascii_renderer_t *renderer,
                                     uint8_t percent)
{
    if (renderer == NULL || renderer->glyph_count == 0) {
        return ' ';
    }
    if (percent > 100) {
        percent = 100;
    }
    uint8_t index =
        (uint8_t)((uint32_t)percent * (renderer->glyph_count - 1u) / 100u);
    return renderer->glyphs[index];
}

static void recompute_motion(espui_ascii_renderer_t *renderer)
{
    const espui_motion_profile_t *profile =
        renderer->profile != NULL ? renderer->profile
                                  : espui_motion_profile_default();

    for (int i = 0; i < ESPUI_VISUAL_STATE_COUNT; ++i) {
        espui_ascii_motion_t base = BASE_MOTION[i];
        uint32_t profile_speed = (i == ESPUI_VISUAL_WORKING)
                                     ? profile->work_speed_percent
                                     : profile->speed_percent;
        uint32_t speed = (uint32_t)base.speed_percent * profile_speed / 100u;
        speed = speed * renderer->user_speed_percent / 100u;
        if (speed > 300u) {
            speed = 300u;
        }

        uint32_t density =
            (uint32_t)base.density_percent * renderer->density_percent / 100u;
        if (density > 100u) {
            density = 100u;
        }

        uint32_t amplitude = base.amplitude_percent;
        if (profile->reduced_motion) {
            amplitude = 0;
        } else if (i == ESPUI_VISUAL_IDLE) {
            amplitude = amplitude * profile->idle_amplitude_percent / 30u;
            if (amplitude > 100u) {
                amplitude = 100u;
            }
        }

        renderer->motion[i].speed_percent = (uint16_t)speed;
        renderer->motion[i].density_percent = (uint8_t)density;
        renderer->motion[i].amplitude_percent = (uint8_t)amplitude;
    }
}

esp_err_t espui_ascii_renderer_init(espui_ascii_renderer_t *renderer,
                                    const espui_ascii_config_t *config)
{
    if (renderer == NULL) {
        return ESP_ERR_INVALID_ARG;
    }

    espui_ascii_config_t defaults = {0};
    const espui_ascii_config_t *cfg = config != NULL ? config : &defaults;

    uint8_t cols = cfg->cols != 0 ? cfg->cols : ESPUI_ASCII_DEFAULT_COLS;
    uint8_t rows = cfg->rows != 0 ? cfg->rows : ESPUI_ASCII_DEFAULT_ROWS;
    if (cols < 20 || rows < 8 || cols > ESPUI_ASCII_MAX_COLS ||
        rows > ESPUI_ASCII_MAX_ROWS) {
        return ESP_ERR_INVALID_ARG;
    }

    const char *ramp =
        cfg->glyph_ramp != NULL ? cfg->glyph_ramp : ESPUI_ASCII_DEFAULT_RAMP;
    size_t ramp_len = strlen(ramp);
    if (ramp_len < 2 || ramp_len >= ESPUI_ASCII_GLYPH_MAX) {
        return ESP_ERR_INVALID_ARG;
    }

    const espui_motion_profile_t *profile =
        cfg->motion_profile != NULL ? cfg->motion_profile
                                    : espui_motion_profile_default();
    uint8_t density = cfg->density_percent != 0 ? cfg->density_percent : 100;
    uint16_t user_speed =
        cfg->animation_speed_percent != 0 ? cfg->animation_speed_percent : 100;
    uint8_t fps = cfg->target_fps != 0
                      ? cfg->target_fps
                      : (profile->target_fps != 0 ? profile->target_fps
                                                  : ESPUI_ASCII_DEFAULT_FPS);
    if (fps > 30) {
        fps = 30;
    }
    if (fps == 0) {
        fps = 1;
    }

    uint32_t pulse = cfg->completed_pulse_ms;
    if (pulse == 0) {
        pulse = profile->transition_ms != 0
                    ? (uint32_t)profile->transition_ms + 800u
                    : ESPUI_ASCII_DEFAULT_PULSE_MS;
    }
    if (pulse < 200u) {
        pulse = 200u;
    }
    if (pulse > 5000u) {
        pulse = 5000u;
    }

    memset(renderer, 0, sizeof(*renderer));
    renderer->initialized = true;
    renderer->cols = cols;
    renderer->rows = rows;
    renderer->glyph_count = (uint8_t)ramp_len;
    memcpy(renderer->glyphs, ramp, ramp_len);
    renderer->glyphs[ramp_len] = '\0';
    renderer->target_fps = fps;
    renderer->completed_pulse_ms = (uint16_t)pulse;
    renderer->density_percent = density;
    renderer->user_speed_percent = user_speed;
    renderer->profile = profile;
    renderer->scheduler_slot = -1;
    renderer->status.state = ESPUI_VISUAL_IDLE;
    renderer->status.node_status = ESPUI_NODE_OFFLINE;
    renderer->frame.cols = cols;
    renderer->frame.rows = rows;
    recompute_motion(renderer);
    espui_ascii_canvas_clear(&renderer->frame);
    return ESP_OK;
}

void espui_ascii_renderer_deinit(espui_ascii_renderer_t *renderer)
{
    if (renderer == NULL) {
        return;
    }
    if (renderer->scheduler != NULL && renderer->scheduler_slot >= 0) {
        (void)espui_animation_scheduler_remove(renderer->scheduler,
                                               renderer->scheduler_slot);
    }
    memset(renderer, 0, sizeof(*renderer));
}

esp_err_t espui_ascii_renderer_set_motion_profile(
    espui_ascii_renderer_t *renderer, const espui_motion_profile_t *profile)
{
    if (renderer == NULL || !renderer->initialized || profile == NULL) {
        return ESP_ERR_INVALID_ARG;
    }
    renderer->profile = profile;
    recompute_motion(renderer);
    return ESP_OK;
}

esp_err_t espui_ascii_renderer_set_density(espui_ascii_renderer_t *renderer,
                                           uint8_t density_percent)
{
    if (renderer == NULL || !renderer->initialized) {
        return ESP_ERR_INVALID_ARG;
    }
    renderer->density_percent = density_percent;
    recompute_motion(renderer);
    return ESP_OK;
}

esp_err_t espui_ascii_renderer_set_user_speed(
    espui_ascii_renderer_t *renderer, uint16_t speed_percent)
{
    if (renderer == NULL || !renderer->initialized) {
        return ESP_ERR_INVALID_ARG;
    }
    renderer->user_speed_percent = speed_percent;
    recompute_motion(renderer);
    return ESP_OK;
}

esp_err_t espui_ascii_renderer_set_status(espui_ascii_renderer_t *renderer,
                                          const espui_ascii_status_t *status)
{
    if (renderer == NULL || !renderer->initialized || status == NULL ||
        !state_is_valid(status->state)) {
        return ESP_ERR_INVALID_ARG;
    }
    if (status->state != renderer->status.state) {
        renderer->state_ms = 0;
    }
    renderer->status = *status;
    return ESP_OK;
}

esp_err_t espui_ascii_renderer_apply_view_model(
    espui_ascii_renderer_t *renderer, const espui_view_model_t *model,
    espui_scene_id_t scene)
{
    if (renderer == NULL || !renderer->initialized) {
        return ESP_ERR_INVALID_ARG;
    }
    espui_ascii_status_t status =
        espui_ascii_status_from_view_model(model, scene);
    esp_err_t err = espui_ascii_renderer_set_status(renderer, &status);
    if (err != ESP_OK) {
        return err;
    }
    return espui_ascii_renderer_render(renderer);
}

void espui_ascii_renderer_advance(espui_ascii_renderer_t *renderer,
                                  uint32_t delta_ms)
{
    if (renderer == NULL || !renderer->initialized || delta_ms == 0) {
        return;
    }

    renderer->clock_ms += delta_ms;
    renderer->state_ms += delta_ms;

    uint32_t speed = renderer->motion[renderer->status.state].speed_percent;
    uint64_t increment = (uint64_t)delta_ms * speed * 65536u / 100000u;
    renderer->phase = (uint16_t)(renderer->phase + (uint16_t)increment);
}

static char core_ring_glyph(const espui_ascii_renderer_t *renderer)
{
    switch (renderer->status.state) {
    case ESPUI_VISUAL_IDLE:
        return espui_ascii_glyph_for_intensity(renderer, 22);
    case ESPUI_VISUAL_WORKING:
        return espui_ascii_glyph_for_intensity(renderer, 70);
    case ESPUI_VISUAL_COMPLETED:
        return espui_ascii_glyph_for_intensity(renderer, 100);
    case ESPUI_VISUAL_APPROVAL:
        return espui_ascii_glyph_for_intensity(renderer, 40);
    case ESPUI_VISUAL_ERROR:
        return espui_ascii_glyph_for_intensity(renderer, 82);
    case ESPUI_VISUAL_OFFLINE:
    default:
        return espui_ascii_glyph_for_intensity(renderer, 14);
    }
}

static void draw_background_field(espui_ascii_renderer_t *renderer,
                                  espui_ascii_frame_t *frame, int cx, int cy,
                                  int orbit_radius, uint8_t density,
                                  espui_ascii_color_t color)
{
    if (density == 0) {
        return;
    }
    uint32_t threshold = density / 3u;
    int x_end = (int)frame->cols - 2;
    int y_end = (int)frame->rows - 2;

    for (int y = 3; y < y_end; ++y) {
        for (int x = 2; x < x_end; ++x) {
            int dx = x - cx;
            int dy = y - cy;
            uint32_t dist =
                isqrt_u32((uint32_t)(dx * dx) / 4u + (uint32_t)(dy * dy));
            if (dist <= (uint32_t)orbit_radius + 1u) {
                continue;
            }
            uint32_t hash =
                field_hash((uint32_t)x, (uint32_t)y, renderer->phase);
            if (hash % 100u >= threshold) {
                continue;
            }
            uint8_t index =
                (uint8_t)(1u + ((hash >> 8) % (renderer->glyph_count - 1u)));
            uint8_t intensity = (uint8_t)(35u + ((hash >> 16) % 60u));
            espui_ascii_canvas_put(frame, x, y, renderer->glyphs[index], color,
                                   intensity);
        }
    }
}

static void draw_core_field(espui_ascii_renderer_t *renderer,
                            espui_ascii_frame_t *frame, int cx, int cy,
                            int radius, uint8_t amplitude,
                            espui_ascii_color_t color)
{
    for (int y = cy - radius; y <= cy + radius; ++y) {
        for (int x = cx - radius * 2; x <= cx + radius * 2; ++x) {
            int dx = x - cx;
            int dy = y - cy;
            uint32_t dist =
                isqrt_u32((uint32_t)(dx * dx) / 4u + (uint32_t)(dy * dy));
            if (dist > (uint32_t)radius) {
                continue;
            }

            uint32_t base = (uint32_t)(radius - (int)dist + 1) * 100u /
                            (uint32_t)(radius + 1);
            int angle = (int)(renderer->phase >> 8) + (int)dist * 7 + x * 3 +
                        y * 5;
            int32_t wave = espui_ascii_canvas_sin((uint8_t)angle);
            uint8_t ripple =
                (uint8_t)(((wave + 127) * (int32_t)amplitude) / 254);
            uint32_t factor = 25u + (uint32_t)amplitude * 3u / 4u;
            uint32_t value = base * factor / 100u + ripple;
            if (value > 100u) {
                value = 100u;
            }
            if (value < 8u) {
                continue;
            }

            uint8_t index =
                (uint8_t)(value * (renderer->glyph_count - 1u) / 100u);
            if (index == 0) {
                index = 1;
            }
            uint8_t intensity =
                value > 127u ? 255u : (uint8_t)(value * 2u);
            espui_ascii_canvas_put(frame, x, y, renderer->glyphs[index], color,
                                   intensity);
        }
    }
}

static void draw_spokes(espui_ascii_renderer_t *renderer,
                        espui_ascii_frame_t *frame, int cx, int cy, int radius,
                        uint8_t amplitude, espui_ascii_color_t color)
{
    int count = renderer->status.state == ESPUI_VISUAL_WORKING ? 4 : 2;
    uint8_t intensity = (uint8_t)(70u + amplitude);
    if (intensity < 80u) {
        intensity = 80u;
    }

    for (int i = 0; i < count; ++i) {
        uint8_t angle =
            (uint8_t)((renderer->phase >> 8) + (uint8_t)(i * 64 / count));
        int x1 = cx + (int)((espui_ascii_canvas_cos(angle) * radius * 2) / 127);
        int y1 = cy + (int)((espui_ascii_canvas_sin(angle) * radius) / 127);
        espui_ascii_canvas_line(frame, cx, cy, x1, y1, color, intensity);
    }
}

static void draw_particles(espui_ascii_renderer_t *renderer,
                           espui_ascii_frame_t *frame, int cx, int cy,
                           int orbit_radius, uint8_t amplitude,
                           uint8_t density, espui_ascii_color_t color)
{
    if (amplitude == 0) {
        return;
    }
    int count = 1 + density / 25 + (renderer->status.state ==
                                            ESPUI_VISUAL_WORKING
                                        ? 2
                                        : 0);
    if (count > 8) {
        count = 8;
    }

    char glyph = espui_ascii_glyph_for_intensity(renderer, 88);
    for (int i = 0; i < count; ++i) {
        uint8_t angle =
            (uint8_t)((renderer->phase >> 7) + (uint8_t)(i * 64 / count));
        int radius = orbit_radius +
                     (int)(((renderer->phase >> 5) + (uint32_t)i) % 2u);
        int x = cx + (int)((espui_ascii_canvas_cos(angle) * radius * 2) / 127);
        int y = cy + (int)((espui_ascii_canvas_sin(angle) * radius) / 127);
        espui_ascii_canvas_put(frame, x, y, glyph, color, 200);
    }
}

static void format_footer(const espui_ascii_renderer_t *renderer, char *buffer,
                          size_t size)
{
    const espui_ascii_status_t *status = &renderer->status;
    switch (status->state) {
    case ESPUI_VISUAL_WORKING:
        snprintf(buffer, size, "SYSTEM ACTIVE // %02u SESSION(S)",
                 (unsigned)status->active_sessions);
        break;
    case ESPUI_VISUAL_COMPLETED:
        snprintf(buffer, size, "SESSION COMPLETED %02u",
                 (unsigned)status->completion_count);
        break;
    case ESPUI_VISUAL_APPROVAL:
        snprintf(buffer, size, "APPROVAL REQUIRED %02u // WAITING",
                 (unsigned)status->approval_count);
        break;
    case ESPUI_VISUAL_ERROR:
        snprintf(buffer, size, "SYSTEM ERROR %02u // CHECK ACTIVITY",
                 (unsigned)status->error_count);
        break;
    case ESPUI_VISUAL_OFFLINE:
        snprintf(buffer, size, "%s",
                 status->node_status == ESPUI_NODE_CONNECTING
                     ? "NODE CONNECTING // LINK DOWN"
                     : "NODE OFFLINE // NO LINK");
        break;
    case ESPUI_VISUAL_IDLE:
    default:
        snprintf(buffer, size, "SYSTEM IDLE // ALL QUIET");
        break;
    }
}

esp_err_t espui_ascii_renderer_render(espui_ascii_renderer_t *renderer)
{
    if (renderer == NULL || !renderer->initialized) {
        return ESP_ERR_INVALID_STATE;
    }

    espui_ascii_frame_t *frame = &renderer->frame;
    espui_ascii_canvas_clear(frame);

    espui_visual_state_t state = renderer->status.state;
    espui_ascii_motion_t motion = renderer->motion[state];
    espui_ascii_color_t state_color = espui_ascii_state_color(state);
    uint8_t amplitude = motion.amplitude_percent;
    uint8_t density = motion.density_percent;

    if (state == ESPUI_VISUAL_COMPLETED) {
        if (renderer->completed_pulse_ms > 0 &&
            renderer->state_ms < renderer->completed_pulse_ms) {
            amplitude = (uint8_t)((uint32_t)amplitude *
                                  (renderer->completed_pulse_ms -
                                   renderer->state_ms) /
                                  renderer->completed_pulse_ms);
        } else {
            amplitude = 0;
        }
    }
    if (state == ESPUI_VISUAL_OFFLINE) {
        amplitude = 0;
    }

    int cols = frame->cols;
    int rows = frame->rows;
    int cx = cols / 2;
    int cy = (rows - 1) / 2;

    int radius = (rows - 7) / 2;
    int horizontal_limit = (cols - 10) / 4;
    if (horizontal_limit < radius) {
        radius = horizontal_limit;
    }
    if (radius < 3) {
        radius = 3;
    }
    if (radius > 12) {
        radius = 12;
    }
    int orbit_radius = radius + 2;

    uint8_t gap = (state == ESPUI_VISUAL_OFFLINE ||
                   state == ESPUI_VISUAL_ERROR)
                      ? 2
                      : 0;
    espui_ascii_canvas_box(frame, 1, 2, cols - 2, rows - 4, gap,
                           ESPUI_ASCII_COLOR_MUTED, 100);

    if (state != ESPUI_VISUAL_OFFLINE) {
        int span = rows - 6;
        if (span < 1) {
            span = 1;
        }
        int scan_y = 3 + (int)((renderer->phase >> 9) % (uint32_t)span);
        uint8_t scan_intensity =
            (uint8_t)(40u + (uint32_t)density / 2u);
        espui_ascii_canvas_scanline(frame, scan_y, '-',
                                    ESPUI_ASCII_COLOR_SECONDARY,
                                    scan_intensity, 2);
    }

    espui_ascii_color_t field_color =
        (state == ESPUI_VISUAL_WORKING || state == ESPUI_VISUAL_COMPLETED)
            ? ESPUI_ASCII_COLOR_SECONDARY
            : ESPUI_ASCII_COLOR_MUTED;
    draw_background_field(renderer, frame, cx, cy, orbit_radius, density,
                          field_color);

    draw_core_field(renderer, frame, cx, cy, radius, amplitude, state_color);

    char ring_glyph = core_ring_glyph(renderer);
    uint8_t ring_intensity =
        state == ESPUI_VISUAL_OFFLINE ? 110u
                                      : (uint8_t)(150u + amplitude);
    espui_ascii_canvas_ring(frame, cx, cy, radius, ring_glyph, 0, 0,
                            renderer->phase, state_color, ring_intensity);

    char orbit_glyph = espui_ascii_glyph_for_intensity(renderer, 30);
    uint8_t orbit_intensity = (uint8_t)(70u + density / 2u);
    espui_ascii_canvas_ring(frame, cx, cy, orbit_radius, orbit_glyph, 8, 4,
                            renderer->phase, ESPUI_ASCII_COLOR_SECONDARY,
                            orbit_intensity);

    if (state == ESPUI_VISUAL_WORKING || state == ESPUI_VISUAL_APPROVAL ||
        state == ESPUI_VISUAL_ERROR || state == ESPUI_VISUAL_COMPLETED) {
        draw_spokes(renderer, frame, cx, cy, radius, amplitude, state_color);
    }

    draw_particles(renderer, frame, cx, cy, orbit_radius, amplitude, density,
                   state_color);

    char header[ESPUI_ASCII_MAX_COLS + 1];
    snprintf(header, sizeof(header), "MEETAGENT // %s",
             espui_visual_state_label(state));
    espui_ascii_canvas_text(frame, 2, 0, header, state_color, 255);

    char footer[ESPUI_ASCII_MAX_COLS + 1];
    format_footer(renderer, footer, sizeof(footer));
    espui_ascii_canvas_text(frame, 2, rows - 1, footer,
                            state == ESPUI_VISUAL_OFFLINE
                                ? ESPUI_ASCII_COLOR_MUTED
                                : state_color,
                            255);

    ++renderer->frame_count;
    return ESP_OK;
}

void espui_ascii_renderer_tick(uint32_t delta_ms, void *user_ctx)
{
    espui_ascii_renderer_t *renderer = (espui_ascii_renderer_t *)user_ctx;
    if (renderer == NULL || !renderer->initialized) {
        return;
    }
    espui_ascii_renderer_advance(renderer, delta_ms);
    (void)espui_ascii_renderer_render(renderer);
}

int32_t espui_ascii_renderer_bind_scheduler(
    espui_ascii_renderer_t *renderer, espui_animation_scheduler_t *scheduler)
{
    if (renderer == NULL || !renderer->initialized || scheduler == NULL) {
        return -1;
    }
    if (renderer->scheduler != NULL) {
        (void)espui_ascii_renderer_unbind_scheduler(renderer,
                                                    renderer->scheduler);
    }

    uint32_t period = 1000u / renderer->target_fps;
    if (period < 16u) {
        period = 16u;
    }
    int32_t slot = espui_animation_scheduler_add(
        scheduler, "ascii", espui_ascii_renderer_tick, renderer, period);
    if (slot < 0) {
        return -1;
    }
    renderer->scheduler = scheduler;
    renderer->scheduler_slot = slot;
    return slot;
}

bool espui_ascii_renderer_unbind_scheduler(
    espui_ascii_renderer_t *renderer, espui_animation_scheduler_t *scheduler)
{
    if (renderer == NULL || scheduler == NULL ||
        renderer->scheduler != scheduler || renderer->scheduler_slot < 0) {
        return false;
    }
    bool removed = espui_animation_scheduler_remove(scheduler,
                                                    renderer->scheduler_slot);
    renderer->scheduler = NULL;
    renderer->scheduler_slot = -1;
    return removed;
}

const espui_ascii_frame_t *espui_ascii_renderer_frame(
    const espui_ascii_renderer_t *renderer)
{
    return renderer == NULL || !renderer->initialized ? NULL : &renderer->frame;
}

const espui_ascii_status_t *espui_ascii_renderer_status(
    const espui_ascii_renderer_t *renderer)
{
    return renderer == NULL || !renderer->initialized ? NULL
                                                      : &renderer->status;
}

const espui_ascii_motion_t *espui_ascii_renderer_motion(
    const espui_ascii_renderer_t *renderer, espui_visual_state_t state)
{
    if (renderer == NULL || !renderer->initialized ||
        !state_is_valid(state)) {
        return NULL;
    }
    return &renderer->motion[state];
}
