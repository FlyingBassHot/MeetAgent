#include "radial_field.h"

#include <math.h>
#include <stddef.h>

#include "glyph_atlas.h"
#include "orbit_geometry.h"

static const char s_ramp[] = " .,:-=+*#%@";

float phosphor_smooth_falloff(float distance, float radius)
{
    if (radius <= 0.0f || distance >= radius) {
        return 0.0f;
    }
    if (distance <= 0.0f) {
        return 1.0f;
    }
    float t = 1.0f - distance / radius;
    return t * t * (3.0f - 2.0f * t);
}

char phosphor_intensity_to_char(float intensity)
{
    if (intensity <= 0.0f) {
        return s_ramp[0];
    }
    if (intensity >= 1.0f) {
        return s_ramp[sizeof(s_ramp) - 2];
    }
    size_t index = (size_t)(intensity * (sizeof(s_ramp) - 2));
    return s_ramp[index];
}

static void radial_field_apply(glyph_frame_t *frame, float cx, float cy,
                               float inner_radius, float outer_radius,
                               uint8_t max_intensity,
                               glyph_color_id_t color, bool sparse)
{
    if (frame == NULL || outer_radius <= inner_radius ||
        max_intensity == 0) {
        return;
    }
    for (uint8_t row = 0; row < PHOSPHOR_GRID_H; ++row) {
        for (uint8_t col = 0; col < PHOSPHOR_GRID_W; ++col) {
            /* Deterministic field: no per-frame random noise. */
            if (sparse &&
                ((((unsigned)col * 7u + (unsigned)row * 11u) & 1u) != 0u)) {
                continue;
            }
            float x = (float)col * PHOSPHOR_CELL_W + PHOSPHOR_CELL_W / 2.0f;
            float y = (float)row * PHOSPHOR_CELL_H + PHOSPHOR_CELL_H / 2.0f;
            float dx = x - cx;
            float dy = y - cy;
            float distance = sqrtf(dx * dx + dy * dy);
            if (distance < inner_radius || distance > outer_radius) {
                continue;
            }
            float t = (distance - inner_radius) /
                      (outer_radius - inner_radius);
            float ring = 1.0f - fabsf(2.0f * t - 1.0f);
            float intensity = (float)max_intensity / 255.0f * ring;
            char glyph = phosphor_intensity_to_char(intensity);
            uint8_t glyph_index = glyph_atlas_index(glyph);
            if (glyph_index == 0) {
                continue;
            }
            glyph_frame_set(frame, col, row, glyph_index, color,
                            (uint8_t)(intensity * 255.0f), GLYPH_FLAG_FIELD);
        }
    }
}

void phosphor_radial_field_apply(glyph_frame_t *frame, float cx, float cy,
                                 float inner_radius, float outer_radius,
                                 uint8_t max_intensity,
                                 glyph_color_id_t color)
{
    radial_field_apply(frame, cx, cy, inner_radius, outer_radius,
                       max_intensity, color, true);
}

void phosphor_radial_field_apply_dense(glyph_frame_t *frame, float cx,
                                       float cy, float inner_radius,
                                       float outer_radius,
                                       uint8_t max_intensity,
                                       glyph_color_id_t color)
{
    radial_field_apply(frame, cx, cy, inner_radius, outer_radius,
                       max_intensity, color, false);
}


void phosphor_radial_field_apply_limited(glyph_frame_t *frame, float cx,
                                         float cy, float inner_radius,
                                         float outer_radius,
                                         uint8_t max_points,
                                         uint8_t max_intensity,
                                         glyph_color_id_t color)
{
    if (frame == NULL || max_points == 0 || outer_radius <= inner_radius ||
        max_intensity == 0) {
        return;
    }
    for (uint8_t i = 0; i < max_points; ++i) {
        float angle = 6.28318530718f * (float)i / (float)max_points;
        float band = (float)((i % 4u) + 1u) / 5.0f;
        float radius = inner_radius + (outer_radius - inner_radius) * band;
        float x = cx + cosf(angle) * radius;
        float y = cy + sinf(angle) * radius;
        uint8_t col = 0;
        uint8_t row = 0;
        if (!phosphor_point_to_cell(x, y, &col, &row)) {
            continue;
        }
        char glyph = (i & 1u) ? '.' : ':';
        glyph_frame_set(frame, col, row, glyph_atlas_index(glyph), color,
                        max_intensity, GLYPH_FLAG_FIELD);
    }
}
