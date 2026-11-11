#include "phosphor_scene.h"

#include <math.h>
#include <stddef.h>

#include "glyph_atlas.h"
#include "glyph_ramp.h"
#include "glyph_rasterizer.h"
#include "orbit_geometry.h"
#include "phosphor_palette.h"
#include "radial_field.h"
#include "satellite_layout.h"

#define PHOSPHOR_CX 216.0f
#define PHOSPHOR_CY 216.0f
#define PHOSPHOR_TWO_PI 6.28318530718f

static char inner_glyph(uint32_t i)
{
    static const char pattern[] = ".,:;";
    return pattern[i % 4u];
}

static char middle_glyph(uint32_t i, float dx, float dy)
{
    if (fabsf(dx) >= fabsf(dy)) {
        return (i & 1u) ? '=' : '-';
    }
    return dx * dy >= 0.0f ? '\\' : '/';
}

static char outer_glyph(uint32_t i)
{
    static const char pattern[] = "+#%@";
    return pattern[i % 4u];
}

static void draw_core_density(glyph_frame_t *frame,
                              const phosphor_motion_state_t *motion)
{
    /* Smooth, deterministic density gradient inside the inner orbit. The
     * nucleus footprint is cleared later by the 2x rasterizer pass. */
    for (uint8_t row = 0; row < PHOSPHOR_GRID_H; ++row) {
        for (uint8_t col = 0; col < PHOSPHOR_GRID_W; ++col) {
            float x = (float)col * PHOSPHOR_CELL_W + PHOSPHOR_CELL_W / 2.0f;
            float y = (float)row * PHOSPHOR_CELL_H + PHOSPHOR_CELL_H / 2.0f;
            float dx = x - PHOSPHOR_CX;
            float dy = y - PHOSPHOR_CY;
            float distance = sqrtf(dx * dx + dy * dy);
            if (distance < 30.0f || distance > 78.0f) {
                continue;
            }
            float falloff = phosphor_smooth_falloff(distance, 78.0f);
            float density = motion != NULL ? motion->density : 0.30f;
            uint8_t intensity = (uint8_t)((70.0f + 120.0f * density) *
                                          falloff);
            char glyph = glyph_ramp_char(intensity);
            uint8_t glyph_index = glyph_atlas_index(glyph);
            if (glyph_index == 0) {
                continue;
            }
            glyph_frame_set(frame, col, row, glyph_index,
                            GLYPH_COLOR_SECONDARY, intensity,
                            GLYPH_FLAG_FIELD);
        }
    }
}

#define PHOSPHOR_MIDDLE_GAP 5.85f
#define PHOSPHOR_MIDDLE_GAP_HALF_WIDTH 0.23f
#define PHOSPHOR_OUTER_GAP_HALF_WIDTH 0.22f

static bool in_gap(float theta, float gap_angle, float half_width)
{
    float diff = theta - gap_angle;
    while (diff > 3.14159265f) {
        diff -= PHOSPHOR_TWO_PI;
    }
    while (diff < -3.14159265f) {
        diff += PHOSPHOR_TWO_PI;
    }
    return fabsf(diff) < half_width;
}

static void draw_orbit_cell(glyph_frame_t *frame,
                             const phosphor_motion_state_t *motion,
                             int orbit_index, uint32_t i, float theta,
                             float x, float y)
{
    uint8_t col = 0;
    uint8_t row = 0;
    if (!phosphor_point_to_cell(x, y, &col, &row)) {
        return;
    }
    phosphor_point_t point;
    float dx = 0.0f;
    float dy = 0.0f;
    phosphor_orbit_tangent(&phosphor_orbit_defaults()[orbit_index], theta,
                           &dx, &dy);
    phosphor_orbit_point(&phosphor_orbit_defaults()[orbit_index], theta,
                         PHOSPHOR_CX, PHOSPHOR_CY, &point);

    char glyph;
    glyph_color_id_t color;
    uint8_t intensity;
    if (orbit_index == 0) {
        glyph = inner_glyph(i);
        color = GLYPH_COLOR_SECONDARY;
        intensity = 200;
    } else if (orbit_index == 1) {
        glyph = middle_glyph(i, dx, dy);
        color = GLYPH_COLOR_SECONDARY;
        intensity = 235;
    } else {
        glyph = outer_glyph(i);
        color = GLYPH_COLOR_PRIMARY;
        intensity = 250;
    }
    uint8_t flags = GLYPH_FLAG_FRONT;
    if (point.depth < 0.0f) {
        flags = GLYPH_FLAG_BACK;
        if (orbit_index == 0) {
            color = GLYPH_COLOR_MUTED;
            intensity = 185;
        } else if (orbit_index == 1) {
            intensity = 175;
        } else {
            intensity = 185;
        }
    }
    if (motion != NULL) {
        float brightness = motion->brightness;
        if (brightness < 0.0f) {
            brightness = 0.0f;
        } else if (brightness > 1.0f) {
            brightness = 1.0f;
        }
        uint8_t adjust = (uint8_t)((1.0f - brightness) * 15.0f);
        intensity = intensity > adjust ? intensity - adjust : 170;
        if (intensity < 170) {
            intensity = 170;
        }
    }
    glyph_frame_set(frame, col, row, glyph_atlas_index(glyph), color,
                    intensity, flags);
}

static void draw_orbit(glyph_frame_t *frame,
                       const phosphor_motion_state_t *motion,
                       const phosphor_orbit_t *orbit, int orbit_index,
                       float warning_angle)
{
    float phase_offset = motion != NULL ? motion->orbit_phase[orbit_index]
                                        : 0.0f;
    for (uint32_t i = 0; i < orbit->samples; ++i) {
        float t0 = orbit->phase + phase_offset +
                   PHOSPHOR_TWO_PI * (float)i / (float)orbit->samples;
        float t1 = orbit->phase + phase_offset +
                   PHOSPHOR_TWO_PI * (float)(i + 1) /
                       (float)orbit->samples;
        if (orbit_index == 1 &&
            (in_gap(t0, PHOSPHOR_MIDDLE_GAP,
                    PHOSPHOR_MIDDLE_GAP_HALF_WIDTH) ||
             in_gap(t1, PHOSPHOR_MIDDLE_GAP,
                    PHOSPHOR_MIDDLE_GAP_HALF_WIDTH))) {
            continue; /* Separate middle orbit from the outer gap. */
        }
        if (orbit_index == 2 &&
            (in_gap(t0, warning_angle, PHOSPHOR_OUTER_GAP_HALF_WIDTH) ||
             in_gap(t1, warning_angle, PHOSPHOR_OUTER_GAP_HALF_WIDTH))) {
            continue; /* Keep the warning satellite gap open. */
        }

        phosphor_point_t p0;
        phosphor_point_t p1;
        phosphor_orbit_point(orbit, t0, PHOSPHOR_CX, PHOSPHOR_CY, &p0);
        phosphor_orbit_point(orbit, t1, PHOSPHOR_CX, PHOSPHOR_CY, &p1);
        float dx = p1.x - p0.x;
        float dy = p1.y - p0.y;
        float cells_x = fabsf(dx) / PHOSPHOR_CELL_W;
        float cells_y = fabsf(dy) / PHOSPHOR_CELL_H;
        int steps = (int)(fmaxf(cells_x, cells_y) * 2.0f) + 1;
        if (steps > 12) {
            steps = 12;
        }
        for (int step = 0; step < steps; ++step) {
            float t = (float)step / (float)steps;
            draw_orbit_cell(frame, motion, orbit_index, i, t0,
                            p0.x + dx * t, p0.y + dy * t);
        }
    }
}

static void draw_activity_particles(glyph_frame_t *frame,
                                    const phosphor_motion_state_t *motion)
{
    if (motion == NULL || motion->activity_energy < 0.05f) {
        return;
    }
    uint8_t count = (uint8_t)(2.0f + motion->activity_energy * 5.0f);
    const phosphor_orbit_t *middle = &phosphor_orbit_defaults()[1];
    for (uint8_t i = 0; i < count; ++i) {
        float theta = motion->orbit_phase[1] + 0.60f +
                      PHOSPHOR_TWO_PI * (float)i / (float)count;
        phosphor_point_t point;
        phosphor_orbit_point(middle, theta, PHOSPHOR_CX, PHOSPHOR_CY,
                             &point);
        uint8_t col = 0;
        uint8_t row = 0;
        if (!phosphor_point_to_cell(point.x, point.y, &col, &row)) {
            continue;
        }
        char glyph = (i & 1u) ? '%' : '+';
        glyph_frame_set(frame, col, row, glyph_atlas_index(glyph),
                        (i & 1u) ? GLYPH_COLOR_HIGHLIGHT
                                 : GLYPH_COLOR_PRIMARY,
                        240, GLYPH_FLAG_FRONT);
    }
}

void phosphor_scene_build_frame_motion(glyph_frame_t *frame,
                                       const phosphor_motion_state_t *motion)
{
    if (frame == NULL) {
        return;
    }
    phosphor_motion_state_t fallback;
    if (motion == NULL) {
        phosphor_motion_init(&fallback, PHOSPHOR_MODE_IDLE);
        motion = &fallback;
    }
    glyph_frame_clear(frame);
    phosphor_satellite_layout_t satellites;
    phosphor_satellite_layout_static(&satellites);
    float warning_angle = satellites.count > 1 ? satellites.items[1].angle
                                               : 3.75f;

    draw_core_density(frame, motion);
    const phosphor_orbit_t *orbits = phosphor_orbit_defaults();
    for (int i = 0; i < 3; ++i) {
        draw_orbit(frame, motion, &orbits[i], i, warning_angle);
    }
    draw_activity_particles(frame, motion);

    for (uint8_t i = 0; i < satellites.count; ++i) {
        uint8_t col = 0;
        uint8_t row = 0;
        if (!phosphor_satellite_cell(&orbits[2], &satellites.items[i],
                                     PHOSPHOR_CX, PHOSPHOR_CY, &col, &row)) {
            continue;
        }
        const bool warning =
            satellites.items[i].status == PHOSPHOR_SATELLITE_WARNING;
        glyph_frame_set(frame, col, row,
                        glyph_atlas_index(warning ? '?' : '*'),
                        warning ? GLYPH_COLOR_WARNING : GLYPH_COLOR_HIGHLIGHT,
                        255, GLYPH_FLAG_SATELLITE);
    }

    uint8_t nucleus_intensity =
        (uint8_t)(235.0f + 20.0f * motion->breath);
    glyph_frame_set_scaled(frame, PHOSPHOR_NUCLEUS_COL, PHOSPHOR_NUCLEUS_ROW,
                           glyph_atlas_index('>'), GLYPH_COLOR_HIGHLIGHT,
                           nucleus_intensity, GLYPH_FLAG_NUCLEUS,
                           PHOSPHOR_NUCLEUS_SCALE);
    glyph_frame_set_scaled(frame, PHOSPHOR_NUCLEUS_COL + 2,
                           PHOSPHOR_NUCLEUS_ROW, glyph_atlas_index('_'),
                           GLYPH_COLOR_HIGHLIGHT, nucleus_intensity,
                           GLYPH_FLAG_NUCLEUS, PHOSPHOR_NUCLEUS_SCALE);
}

void phosphor_scene_build_frame(glyph_frame_t *frame)
{
    phosphor_motion_state_t idle;
    phosphor_motion_init(&idle, PHOSPHOR_MODE_IDLE);
    phosphor_scene_build_frame_motion(frame, &idle);
}

void phosphor_scene_render_motion_rgb565(
    uint16_t *pixels, int width, int height,
    const phosphor_motion_state_t *motion)
{
    glyph_frame_t frame;
    phosphor_scene_build_frame_motion(&frame, motion);
    glyph_rasterizer_render(&frame, pixels, width, height,
                            phosphor_palette_default());
}

void phosphor_scene_render_rgb565(uint16_t *pixels, int width, int height)
{
    phosphor_motion_state_t idle;
    phosphor_motion_init(&idle, PHOSPHOR_MODE_IDLE);
    phosphor_scene_render_motion_rgb565(pixels, width, height, &idle);
}
