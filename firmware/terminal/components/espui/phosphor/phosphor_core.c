/*
 * PHOSPHOR CORE R1 — state-first static geometry.
 *
 * The visual state directly generates the orbit gaps, local breaks, platform
 * disintegration, completion waves, satellites and label. There is no P3
 * base-frame + overlay path here.
 */
#include "phosphor_core.h"

#include <math.h>
#include <stdio.h>
#include <string.h>

#include "glyph_atlas.h"
#include "glyph_ramp.h"
#include "glyph_rasterizer.h"
#include "orbit_geometry.h"
#include "phosphor_palette.h"
#include "radial_field.h"
#include "phosphor_types.h"

#define CORE_CX 216.0f
#define CORE_CY 216.0f
#define CORE_TWO_PI 6.28318530718f
#define CORE_APPROVAL_GAP 3.75f
#define CORE_APPROVAL_GAP_HALF 1.35f
#define CORE_ERROR_GAP_HALF 1.55f

typedef struct {
    char glyph;
    glyph_color_id_t color;
    uint8_t intensity;
} water_cell_t;

static const water_cell_t s_outer_pattern[] = {
    {'0', GLYPH_COLOR_SECONDARY, 230}, {'1', GLYPH_COLOR_SECONDARY, 230},
    {'-', GLYPH_COLOR_SECONDARY, 230}, {'=', GLYPH_COLOR_SECONDARY, 230},
    {'1', GLYPH_COLOR_SECONDARY, 230}, {'0', GLYPH_COLOR_SECONDARY, 230},
    {'/', GLYPH_COLOR_SECONDARY, 230}, {'\\', GLYPH_COLOR_SECONDARY, 230},
    {'#', GLYPH_COLOR_HIGHLIGHT, 255}, {'@', GLYPH_COLOR_HIGHLIGHT, 255},
    {'+', GLYPH_COLOR_HIGHLIGHT, 255}, {'o', GLYPH_COLOR_HIGHLIGHT, 255},
    {'1', GLYPH_COLOR_SECONDARY, 230}, {'0', GLYPH_COLOR_SECONDARY, 230},
    {'=', GLYPH_COLOR_SECONDARY, 230}, {'-', GLYPH_COLOR_SECONDARY, 230},
    {'%', GLYPH_COLOR_HIGHLIGHT, 255}, {'@', GLYPH_COLOR_HIGHLIGHT, 255},
    {'#', GLYPH_COLOR_HIGHLIGHT, 255}, {'+', GLYPH_COLOR_HIGHLIGHT, 255},
};

static bool in_gap(float theta, float center, float half)
{
    float diff = theta - center;
    while (diff > 3.14159265f) diff -= CORE_TWO_PI;
    while (diff < -3.14159265f) diff += CORE_TWO_PI;
    return fabsf(diff) < half;
}

static void put_text(glyph_frame_t *frame, uint8_t row, uint8_t col,
                     const char *text, glyph_color_id_t color,
                     uint8_t intensity)
{
    for (uint8_t i = 0; text != NULL && text[i] != '\0'; ++i) {
        if (col + i >= PHOSPHOR_GRID_W) break;
        uint8_t g = glyph_atlas_index(text[i]);
        if (g != 0) glyph_frame_set(frame, col + i, row, g, color, intensity,
                                    GLYPH_FLAG_FRONT);
    }
}

static void put_center_label(glyph_frame_t *frame, uint8_t row,
                             const char *text, glyph_color_id_t color,
                             uint8_t intensity)
{
    uint8_t len = (uint8_t)strlen(text);
    uint8_t col = len < PHOSPHOR_GRID_W ? (PHOSPHOR_GRID_W - len) / 2 : 0;
    put_text(frame, row, col, text, color, intensity);
}

static char inner_char(uint32_t i)
{
    static const char p[] = ";:;:;+;";
    return p[i % 7u];
}

static char middle_char(uint32_t i, float dx, float dy)
{
    if (fabsf(dx) >= fabsf(dy)) return (i & 1u) ? '=' : '-';
    return dx * dy >= 0.0f ? '\\' : '/';
}

static water_cell_t outer_char(uint32_t i)
{
    return s_outer_pattern[i % (sizeof(s_outer_pattern) /
                                sizeof(s_outer_pattern[0]))];
}

static void core_density(glyph_frame_t *frame)
{
    for (uint8_t r = 0; r < PHOSPHOR_GRID_H; ++r) {
        for (uint8_t c = 0; c < PHOSPHOR_GRID_W; ++c) {
            float x = c * PHOSPHOR_CELL_W + PHOSPHOR_CELL_W / 2.0f;
            float y = r * PHOSPHOR_CELL_H + PHOSPHOR_CELL_H / 2.0f;
            float dx = x - CORE_CX, dy = y - CORE_CY;
            float d = sqrtf(dx * dx + dy * dy);
            if (d < 48.0f || d > 64.0f) continue;
            if ((c * 7u + r * 11u) % 17u != 0u) continue;
            float falloff = phosphor_smooth_falloff(d - 48.0f, 16.0f);
            uint8_t intensity = (uint8_t)(110.0f * falloff);
            char g = glyph_ramp_char(intensity);
            if (g != ' ') {
                glyph_frame_set(frame, c, r, glyph_atlas_index(g),
                                GLYPH_COLOR_SECONDARY, intensity,
                                GLYPH_FLAG_FIELD);
            }
        }
    }
}

static void put_band(glyph_frame_t *frame, int orbit_index, uint8_t col,
                     uint8_t row, uint32_t i, bool low)
{
    float x = col * PHOSPHOR_CELL_W + PHOSPHOR_CELL_W / 2.0f;
    float y = row * PHOSPHOR_CELL_H + PHOSPHOR_CELL_H / 2.0f;
    float dx = orbit_index == 2 ? CORE_CX - x : x - CORE_CX;
    float dy = orbit_index == 2 ? CORE_CY - y : y - CORE_CY;
    uint8_t nc = col, nr = row;
    if (fabsf(dx) / PHOSPHOR_CELL_W >= fabsf(dy) / PHOSPHOR_CELL_H) {
        nc = (uint8_t)((int)col + (dx >= 0 ? 1 : -1));
    } else {
        nr = (uint8_t)((int)row + (dy >= 0 ? 1 : -1));
    }
    if (nc >= PHOSPHOR_GRID_W || nr >= PHOSPHOR_GRID_H) return;
    char g = low ? '0' : (orbit_index == 0 ? inner_char(i) : '1');
    glyph_color_id_t color = GLYPH_COLOR_SECONDARY;
    glyph_frame_set(frame, nc, nr, glyph_atlas_index(g), color,
                    low ? 105 : (orbit_index == 0 ? 140 : 180),
                    GLYPH_FLAG_FIELD |
                        (orbit_index == 0 ? GLYPH_FLAG_INNER_BAND
                                          : GLYPH_FLAG_OUTER_BAND));
}

static void draw_orbit(glyph_frame_t *frame, int orbit_index,
                       const phosphor_visual_state_t *visual,
                       const phosphor_motion_state_t *motion,
                       phosphor_core_metrics_t *metrics,
                       float error_angle, bool has_error)
{
    phosphor_orbit_t orbit = phosphor_orbit_defaults()[orbit_index];
    float phase_offset = motion != NULL ? motion->orbit_phase[orbit_index]
                                        : 0.0f;
    if (visual->completion_active && (orbit_index == 1 || orbit_index == 2)) {
        float p = (float)visual->completion_elapsed_ms /
                  PHOSPHOR_COMPLETION_MS;
        if (p > 1.0f) p = 1.0f;
        orbit.radius *= (0.96f - 0.06f * p); /* pulse contraction */
    }
    for (uint32_t i = 0; i < orbit.samples; ++i) {
        float t0 = orbit.phase + phase_offset +
                   CORE_TWO_PI * (float)i / (float)orbit.samples;
        float t1 = orbit.phase + phase_offset +
                   CORE_TWO_PI * (float)(i + 1) / (float)orbit.samples;
        bool gap = false;
        if (orbit_index == 2 &&
            (in_gap(t0, CORE_APPROVAL_GAP,
                    (visual->attention & PHOSPHOR_ATTENTION_APPROVAL)
                        ? CORE_APPROVAL_GAP_HALF : 0.11f) ||
             in_gap(t1, CORE_APPROVAL_GAP,
                    (visual->attention & PHOSPHOR_ATTENTION_APPROVAL)
                        ? CORE_APPROVAL_GAP_HALF : 0.11f))) {
            gap = true;
        }
        if (orbit_index == 2 && has_error &&
            (in_gap(t0, error_angle, CORE_ERROR_GAP_HALF) ||
             in_gap(t1, error_angle, CORE_ERROR_GAP_HALF))) {
            gap = true;
        }
        if (gap) {
            if (metrics != NULL) ++metrics->approval_gap_cells;
            continue;
        }
        phosphor_point_t p0, p1;
        phosphor_orbit_point(&orbit, t0, CORE_CX, CORE_CY, &p0);
        phosphor_orbit_point(&orbit, t1, CORE_CX, CORE_CY, &p1);
        float dx = p1.x - p0.x, dy = p1.y - p0.y;
        int steps = (int)(fmaxf(fabsf(dx) / PHOSPHOR_CELL_W,
                                fabsf(dy) / PHOSPHOR_CELL_H) * 2.0f) + 1;
        if (steps > 12) steps = 12;
        for (int s = 0; s < steps; ++s) {
            float t = (float)s / (float)steps;
            float x = p0.x + dx * t, y = p0.y + dy * t;
            uint8_t col, row;
            if (!phosphor_point_to_cell(x, y, &col, &row)) continue;
            if (visual->activity == PHOSPHOR_ACTIVITY_OFFLINE &&
                (((unsigned)col * 13u + (unsigned)row * 7u) & 3u) == 0u) {
                if (metrics != NULL) ++metrics->offline_removed_cells;
                continue;
            }
            char glyph;
            glyph_color_id_t color;
            uint8_t intensity;
            if (orbit_index == 0) {
                glyph = inner_char(i);
                color = GLYPH_COLOR_SECONDARY;
                intensity = 200;
            } else if (orbit_index == 1) {
                glyph = middle_char(i, dx, dy);
                color = GLYPH_COLOR_PRIMARY;
                intensity = 215;
            } else {
                water_cell_t o = outer_char(i);
                glyph = o.glyph;
                color = o.color;
                intensity = o.intensity;
            }
            if (motion != NULL) {
                float b = motion->brightness;
                if (b < 0.0f) b = 0.0f;
                if (b > 1.0f) b = 1.0f;
                float breath = 0.90f + 0.10f * motion->breath;
                intensity = (uint8_t)((float)intensity *
                                      (0.82f + 0.18f * b) * breath);
            }
            uint8_t flags = GLYPH_FLAG_FRONT;
            if (visual->activity == PHOSPHOR_ACTIVITY_OFFLINE) {
                flags = GLYPH_FLAG_BACK;
                intensity = 90;
            }
            glyph_frame_set(frame, col, row, glyph_atlas_index(glyph),
                            color, intensity, flags);
            if (visual->activity != PHOSPHOR_ACTIVITY_OFFLINE &&
                (orbit_index == 0 || orbit_index == 2)) {
                put_band(frame, orbit_index, col, row, i, false);
            }
        }
    }
}

static void draw_working_water_boost(
    glyph_frame_t *frame, const phosphor_visual_state_t *visual,
    const phosphor_motion_state_t *motion, float error_angle, bool has_error)
{
    if (visual->activity != PHOSPHOR_ACTIVITY_WORKING ||
        visual->attention != 0) {
        return; /* Approval/Error have lower density, not Working water. */
    }
    bool approval = false;
    /* Extra deterministic water stream: a thin inner water band, not a
     * random halo. Working gets visibly more water cells than Idle. */
    const phosphor_orbit_t *outer = &phosphor_orbit_defaults()[2];
    for (int i = 0; i < 32; ++i) {
        float angle = CORE_TWO_PI * (float)i / 32.0f + 0.23f +
                      (motion != NULL ? motion->orbit_phase[2] : 0.0f);
        if ((approval && in_gap(angle, CORE_APPROVAL_GAP,
                                CORE_APPROVAL_GAP_HALF)) ||
            (has_error && in_gap(angle, error_angle,
                                 CORE_ERROR_GAP_HALF))) {
            continue;
        }
        phosphor_orbit_t band = *outer;
        band.radius = (i & 1) ? 196.0f : 200.0f;
        phosphor_point_t p;
        phosphor_orbit_point(&band, angle, CORE_CX, CORE_CY, &p);
        uint8_t col, row;
        if (!phosphor_point_to_cell(p.x, p.y, &col, &row)) {
            continue;
        }
        char g = (i % 3 == 0) ? '0' : (i % 3 == 1 ? '1' : '-');
        glyph_frame_set(frame, col, row, glyph_atlas_index(g),
                        GLYPH_COLOR_SECONDARY, 210, GLYPH_FLAG_FRONT);
    }
    for (int i = 0; i < 12; ++i) {
        float angle = CORE_TWO_PI * (float)i / 12.0f + 0.41f +
                      (motion != NULL ? motion->orbit_phase[2] : 0.0f);
        if ((approval && in_gap(angle, CORE_APPROVAL_GAP,
                                CORE_APPROVAL_GAP_HALF)) ||
            (has_error && in_gap(angle, error_angle,
                                 CORE_ERROR_GAP_HALF))) {
            continue;
        }
        phosphor_orbit_t band = *outer;
        band.radius = 150.0f;
        phosphor_point_t p;
        phosphor_orbit_point(&band, angle, CORE_CX, CORE_CY, &p);
        uint8_t col, row;
        if (!phosphor_point_to_cell(p.x, p.y, &col, &row)) {
            continue;
        }
        char g = (i & 1) ? '0' : '-';
        glyph_frame_set(frame, col, row, glyph_atlas_index(g),
                        GLYPH_COLOR_SECONDARY, 190, GLYPH_FLAG_FRONT);
    }
}

static void draw_error_break(glyph_frame_t *frame, float angle,
                             bool glitch)
{
    const phosphor_orbit_t *outer = &phosphor_orbit_defaults()[2];
    const float draw_angle = angle + (glitch ? 0.12f : 0.0f);
    static const char chars[7] = {'!', 'x', '-', '!', 'x', '-', '!'};
    for (int i = -3; i <= 3; ++i) {
        phosphor_point_t p;
        phosphor_orbit_point(outer, draw_angle + (float)i * 0.060f,
                             CORE_CX, CORE_CY, &p);
        uint8_t col, row;
        if (phosphor_point_to_cell(p.x, p.y, &col, &row)) {
            glyph_frame_set(frame, col, row, glyph_atlas_index(chars[i + 3]),
                            GLYPH_COLOR_ERROR, 230, GLYPH_FLAG_FRONT);
        }
    }
    phosphor_orbit_t inner = *outer;
    inner.radius -= 18.0f;
    for (int i = -3; i <= 3; ++i) {
        phosphor_point_t p;
        phosphor_orbit_point(&inner, draw_angle + (float)i * 0.060f + 0.02f,
                             CORE_CX, CORE_CY, &p);
        uint8_t col, row;
        if (phosphor_point_to_cell(p.x, p.y, &col, &row)) {
            char g = (i & 1) ? 'x' : '-';
            glyph_frame_set(frame, col, row, glyph_atlas_index(g),
                            GLYPH_COLOR_ERROR, 150, GLYPH_FLAG_FRONT);
        }
    }
    if (glitch) {
        phosphor_point_t gp;
        phosphor_orbit_point(outer, draw_angle + 0.18f, CORE_CX, CORE_CY,
                             &gp);
        uint8_t gc, gr;
        if (phosphor_point_to_cell(gp.x, gp.y, &gc, &gr)) {
            glyph_frame_set(frame, gc, gr, glyph_atlas_index('!'),
                            GLYPH_COLOR_ERROR, 240, GLYPH_FLAG_FRONT);
        }
    }
    phosphor_orbit_t inner2 = *outer;
    inner2.radius -= 36.0f;
    for (int i = -3; i <= 3; ++i) {
        phosphor_point_t p;
        phosphor_orbit_point(&inner2, draw_angle + (float)i * 0.060f + 0.04f,
                             CORE_CX, CORE_CY, &p);
        uint8_t col, row;
        if (phosphor_point_to_cell(p.x, p.y, &col, &row)) {
            char g = (i & 1) ? '-' : 'x';
            glyph_frame_set(frame, col, row, glyph_atlas_index(g),
                            GLYPH_COLOR_ERROR, 110, GLYPH_FLAG_FRONT);
        }
    }
}

static void draw_satellites(glyph_frame_t *frame,
                            const phosphor_visual_state_t *visual)
{
    static const float slot_angle[4] = {0.65f, 5.10f, 3.40f, 4.05f};
    const phosphor_orbit_t *outer = &phosphor_orbit_defaults()[2];
    for (uint8_t i = 0; i < visual->satellite_count && i < 4; ++i) {
        const phosphor_satellite_state_t *s = &visual->satellites[i];
        float angle = slot_angle[s->slot % 4];
        if ((visual->attention & PHOSPHOR_ATTENTION_APPROVAL) &&
            s->status == PHOSPHOR_SESSION_WAITING) {
            angle = (s->slot == 2)
                        ? CORE_APPROVAL_GAP - CORE_APPROVAL_GAP_HALF + 0.02f
                        : CORE_APPROVAL_GAP + CORE_APPROVAL_GAP_HALF - 0.02f;
        }
        phosphor_point_t p;
        phosphor_orbit_point(outer, angle, CORE_CX, CORE_CY, &p);
        uint8_t col, row;
        if (!phosphor_point_to_cell(p.x, p.y, &col, &row)) continue;
        char g = 'o';
        glyph_color_id_t color = GLYPH_COLOR_MUTED;
        uint8_t intensity = 170;
        switch (s->status) {
        case PHOSPHOR_SESSION_RUNNING: g = '*'; color = GLYPH_COLOR_HIGHLIGHT; intensity = 255; break;
        case PHOSPHOR_SESSION_WAITING: g = '?'; color = GLYPH_COLOR_WARNING;
            intensity = (uint8_t)(180.0f + 70.0f * visual->approval_breath); break;
        case PHOSPHOR_SESSION_ERROR: g = '!'; color = GLYPH_COLOR_ERROR; intensity = 255; break;
        case PHOSPHOR_SESSION_COMPLETED: g = '.'; color = GLYPH_COLOR_HIGHLIGHT; intensity = 220; break;
        default: g = 'o'; color = GLYPH_COLOR_MUTED; intensity = 170; break;
        }
        glyph_frame_set(frame, col, row, glyph_atlas_index(g), color,
                        intensity, GLYPH_FLAG_SATELLITE);
    }
    if (visual->overflow_count > 0) {
        char ov[8];
        snprintf(ov, sizeof(ov), "+%u", (unsigned)visual->overflow_count);
        put_text(frame, 1, PHOSPHOR_GRID_W - 7, ov, GLYPH_COLOR_HIGHLIGHT, 200);
    }
}

static void draw_completed(glyph_frame_t *frame,
                           const phosphor_visual_state_t *visual,
                           phosphor_core_metrics_t *metrics)
{
    if (!visual->completion_active) return;
    float p = (float)visual->completion_elapsed_ms / PHOSPHOR_COMPLETION_MS;
    if (p > 1.0f) p = 1.0f;
    static const int counts[3] = {16, 24, 32};
    for (int layer = 0; layer < 3; ++layer) {
        float radius = 34.0f + layer * 30.0f + 100.0f * p;
        for (int i = 0; i < counts[layer]; ++i) {
            float a = CORE_TWO_PI * i / counts[layer] + layer * 0.15f;
            uint8_t col, row;
            if (!phosphor_point_to_cell(CORE_CX + cosf(a) * radius,
                                        CORE_CY + sinf(a) * radius,
                                        &col, &row)) continue;
            char g = (i & 1) ? '*' : '+';
            uint8_t intensity = (uint8_t)(230.0f * (1.0f - p) + 20.0f);
            glyph_frame_set(frame, col, row, glyph_atlas_index(g),
                            GLYPH_COLOR_HIGHLIGHT, intensity,
                            GLYPH_FLAG_FRONT);
        }
    }
    if (metrics != NULL) metrics->completion_wave_layers = 3;
    const phosphor_orbit_t *outer = &phosphor_orbit_defaults()[2];
    for (int i = 0; i < 8; ++i) {
        float a = CORE_TWO_PI * i / 8.0f + p * 0.8f;
        phosphor_point_t point;
        phosphor_orbit_point(outer, a, CORE_CX, CORE_CY, &point);
        uint8_t col, row;
        if (phosphor_point_to_cell(point.x, point.y, &col, &row)) {
            glyph_frame_set(frame, col, row, glyph_atlas_index('*'),
                            GLYPH_COLOR_HIGHLIGHT, 220, GLYPH_FLAG_FRONT);
        }
    }
}

static void draw_approval_edges(glyph_frame_t *frame,
                                const phosphor_visual_state_t *visual)
{
    if ((visual->attention & PHOSPHOR_ATTENTION_APPROVAL) == 0) {
        return;
    }
    const phosphor_orbit_t *outer = &phosphor_orbit_defaults()[2];
    const float edges[2] = {CORE_APPROVAL_GAP - CORE_APPROVAL_GAP_HALF + 0.035f,
                            CORE_APPROVAL_GAP + CORE_APPROVAL_GAP_HALF - 0.035f};
    for (int layer = 0; layer < 3; ++layer) {
        phosphor_orbit_t boundary = *outer;
        boundary.radius = outer->radius - 18.0f * (float)layer;
        for (int e = 0; e < 2; ++e) {
            for (int k = -2; k <= 2; ++k) {
                phosphor_point_t p;
                phosphor_orbit_point(&boundary,
                                     edges[e] + (float)k * 0.035f,
                                     CORE_CX, CORE_CY, &p);
                uint8_t col, row;
                if (phosphor_point_to_cell(p.x, p.y, &col, &row)) {
                    char g = (k == 0 && layer == 0) ? '?' : (k & 1 ? ':' : '|');
                    float breath = 0.78f + 0.22f * visual->approval_breath;
                    uint8_t base = k == 0 ? 190 : (layer == 0 ? 120 : 90);
                    glyph_frame_set(frame, col, row, glyph_atlas_index(g),
                                    k == 0 ? GLYPH_COLOR_WARNING
                                           : GLYPH_COLOR_MUTED,
                                    (uint8_t)((float)base * breath),
                                    GLYPH_FLAG_FRONT);
                }
            }
        }
    }
}

static void draw_label(glyph_frame_t *frame,
                       const phosphor_visual_state_t *visual)
{
    char text[16];
    if (visual->activity == PHOSPHOR_ACTIVITY_OFFLINE) {
        put_center_label(frame, 0, "OFFLINE", GLYPH_COLOR_MUTED, 210);
        put_center_label(frame, 2, "NO LINK", GLYPH_COLOR_MUTED, 200);
        put_center_label(frame, 4, "LAST SYNC --", GLYPH_COLOR_MUTED, 140);
        return;
    }
    if (visual->completion_active) {
        snprintf(text, sizeof(text), "COMPLETED");
        put_center_label(frame, 0, text, GLYPH_COLOR_HIGHLIGHT, 255);
    } else if (visual->attention & PHOSPHOR_ATTENTION_APPROVAL) {
        snprintf(text, sizeof(text), "APPROVAL %u",
                 (unsigned)visual->waiting_count);
        put_center_label(frame, 0, text, GLYPH_COLOR_WARNING, 255);
    } else if (visual->attention & PHOSPHOR_ATTENTION_ERROR) {
        snprintf(text, sizeof(text), "ERROR %u", (unsigned)visual->error_count);
        put_center_label(frame, 0, text, GLYPH_COLOR_ERROR, 255);
    } else if (visual->activity == PHOSPHOR_ACTIVITY_WORKING) {
        put_center_label(frame, 0, "WORKING", GLYPH_COLOR_PRIMARY, 255);
    } else {
        put_center_label(frame, 0, "IDLE", GLYPH_COLOR_SECONDARY, 255);
    }
}

void phosphor_core_build_frame_motion(
    glyph_frame_t *frame, const phosphor_visual_state_t *visual,
    const phosphor_motion_state_t *motion,
    phosphor_core_metrics_t *metrics)
{
    if (frame == NULL || visual == NULL) return;
    if (metrics != NULL) memset(metrics, 0, sizeof(*metrics));
    glyph_frame_clear(frame);
    core_density(frame);
    float error_angle = 0.0f;
    bool has_error = false;
    for (uint8_t i = 0; i < visual->satellite_count && i < 4; ++i) {
        if (visual->satellites[i].status == PHOSPHOR_SESSION_ERROR) {
            static const float slot_angle[4] = {0.65f, 5.10f, 3.40f, 4.05f};
            error_angle = slot_angle[visual->satellites[i].slot % 4];
            has_error = true;
        }
    }
    for (int i = 2; i >= 0; --i) {
        draw_orbit(frame, i, visual, motion, metrics, error_angle, has_error);
    }
    draw_working_water_boost(frame, visual, motion, error_angle, has_error);
    draw_approval_edges(frame, visual);
    if (has_error) {
        draw_error_break(frame, error_angle, visual->error_glitch_active);
        if (metrics != NULL) metrics->error_break_cells = 7;
    }
    draw_satellites(frame, visual);
    draw_completed(frame, visual, metrics);
    if (visual->activity == PHOSPHOR_ACTIVITY_OFFLINE) {
        for (uint8_t r = 0; r < PHOSPHOR_GRID_H; ++r) {
            for (uint8_t c = 0; c < PHOSPHOR_GRID_W; ++c) {
                glyph_cell_t *cell = &frame->cells[r][c];
                if (cell->glyph != 0) {
                    cell->intensity = (uint8_t)(cell->intensity * 35u / 100u);
                }
            }
        }
    }
    draw_label(frame, visual);
    uint8_t ni = visual->completion_active ? 255 : 245;
    glyph_frame_set_scaled(frame, 10, 7, glyph_atlas_index('>'),
                           GLYPH_COLOR_HIGHLIGHT, ni, GLYPH_FLAG_NUCLEUS, 2);
    glyph_frame_set_scaled(frame, 12, 7, glyph_atlas_index('_'),
                           GLYPH_COLOR_HIGHLIGHT, ni, GLYPH_FLAG_NUCLEUS, 2);
}

void phosphor_core_build_frame(glyph_frame_t *frame,
                               const phosphor_visual_state_t *visual,
                               phosphor_core_metrics_t *metrics)
{
    phosphor_core_build_frame_motion(frame, visual, NULL, metrics);
}

void phosphor_core_render_motion_rgb565(
    uint16_t *pixels, int width, int height,
    const phosphor_visual_state_t *visual,
    const phosphor_motion_state_t *motion,
    phosphor_core_metrics_t *metrics)
{
    glyph_frame_t frame;
    phosphor_core_build_frame_motion(&frame, visual, motion, metrics);
    glyph_rasterizer_render(&frame, pixels, width, height,
                            phosphor_palette_default());
}

void phosphor_core_render_rgb565(uint16_t *pixels, int width, int height,
                                 const phosphor_visual_state_t *visual,
                                 phosphor_core_metrics_t *metrics)
{
    phosphor_core_render_motion_rgb565(pixels, width, height, visual, NULL,
                                       metrics);
}
