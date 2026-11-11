#include "phosphor_scene.h"

#include <math.h>
#include <stddef.h>

#include "glyph_atlas.h"
#include "glyph_rasterizer.h"
#include "orbit_geometry.h"
#include "phosphor_palette.h"
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

static void draw_core_shell(glyph_frame_t *frame)
{
    /* Sparse punctuation shell; no solid #/= rings and a clear center. */
    for (uint8_t row = 0; row < PHOSPHOR_GRID_H; ++row) {
        for (uint8_t col = 0; col < PHOSPHOR_GRID_W; ++col) {
            if ((col + row) & 1u) {
                continue;
            }
            float x = (float)col * PHOSPHOR_CELL_W + PHOSPHOR_CELL_W / 2.0f;
            float y = (float)row * PHOSPHOR_CELL_H + PHOSPHOR_CELL_H / 2.0f;
            float dx = x - PHOSPHOR_CX;
            float dy = y - PHOSPHOR_CY;
            float distance = sqrtf(dx * dx + dy * dy);
            float inner_ring = fabsf(distance - 46.0f);
            float outer_ring = fabsf(distance - 68.0f);
            if (inner_ring > 7.0f && outer_ring > 7.0f) {
                continue;
            }
            char glyph = inner_ring <= 7.0f ? ':' : '.';
            glyph_frame_set(frame, col, row, glyph_atlas_index(glyph),
                            GLYPH_COLOR_SECONDARY,
                            inner_ring <= 7.0f ? 105 : 75,
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

static void draw_orbit_cell(glyph_frame_t *frame, int orbit_index, uint32_t i,
                             float theta, float x, float y)
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
        intensity = 190;
    } else if (orbit_index == 1) {
        glyph = middle_glyph(i, dx, dy);
        color = GLYPH_COLOR_PRIMARY;
        intensity = 235;
    } else {
        glyph = outer_glyph(i);
        color = (i % 5u == 0u) ? GLYPH_COLOR_HIGHLIGHT
                               : GLYPH_COLOR_SECONDARY;
        intensity = 245;
    }
    uint8_t flags = GLYPH_FLAG_FRONT;
    if (point.depth < 0.0f) {
        color = GLYPH_COLOR_MUTED;
        intensity = 125;
        flags = GLYPH_FLAG_BACK;
    }
    glyph_frame_set(frame, col, row, glyph_atlas_index(glyph), color,
                    intensity, flags);
}

static void draw_orbit(glyph_frame_t *frame, const phosphor_orbit_t *orbit,
                       int orbit_index, float warning_angle)
{
    for (uint32_t i = 0; i < orbit->samples; ++i) {
        float t0 = orbit->phase +
                   PHOSPHOR_TWO_PI * (float)i / (float)orbit->samples;
        float t1 = orbit->phase + PHOSPHOR_TWO_PI * (float)(i + 1) /
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
            draw_orbit_cell(frame, orbit_index, i, t0,
                            p0.x + dx * t, p0.y + dy * t);
        }
    }
}

void phosphor_scene_build_frame(glyph_frame_t *frame)
{
    if (frame == NULL) {
        return;
    }
    glyph_frame_clear(frame);

    phosphor_satellite_layout_t satellites;
    phosphor_satellite_layout_static(&satellites);
    float warning_angle = satellites.count > 1 ? satellites.items[1].angle
                                               : 3.75f;

    draw_core_shell(frame);
    /* P1.3: no ambient radial points/halo; clean orbit separation. */

    const phosphor_orbit_t *orbits = phosphor_orbit_defaults();
    for (int i = 0; i < 3; ++i) {
        draw_orbit(frame, &orbits[i], i, warning_angle);
    }

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

    /* Nucleus is last: no orbit or satellite can cover >_ . */
    glyph_frame_set_scaled(frame, PHOSPHOR_NUCLEUS_COL, PHOSPHOR_NUCLEUS_ROW,
                           glyph_atlas_index('>'), GLYPH_COLOR_HIGHLIGHT, 255,
                           GLYPH_FLAG_NUCLEUS, PHOSPHOR_NUCLEUS_SCALE);
    glyph_frame_set_scaled(frame, PHOSPHOR_NUCLEUS_COL + 2,
                           PHOSPHOR_NUCLEUS_ROW, glyph_atlas_index('_'),
                           GLYPH_COLOR_HIGHLIGHT, 255, GLYPH_FLAG_NUCLEUS,
                           PHOSPHOR_NUCLEUS_SCALE);
}

void phosphor_scene_render_rgb565(uint16_t *pixels, int width, int height)
{
    glyph_frame_t frame;
    phosphor_scene_build_frame(&frame);
    glyph_rasterizer_render(&frame, pixels, width, height,
                            phosphor_palette_default());
}
