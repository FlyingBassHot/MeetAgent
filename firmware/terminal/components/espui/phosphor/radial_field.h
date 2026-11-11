#pragma once

#include <stdint.h>

#include "glyph_frame.h"

#ifdef __cplusplus
extern "C" {
#endif

float phosphor_smooth_falloff(float distance, float radius);
char phosphor_intensity_to_char(float intensity);

void phosphor_radial_field_apply(glyph_frame_t *frame, float cx, float cy,
                                 float inner_radius, float outer_radius,
                                 uint8_t max_intensity,
                                 glyph_color_id_t color);

/* Same annulus, but fills every eligible cell. */
void phosphor_radial_field_apply_dense(glyph_frame_t *frame, float cx,
                                       float cy, float inner_radius,
                                       float outer_radius,
                                       uint8_t max_intensity,
                                       glyph_color_id_t color);

/* Exactly max_points deterministic samples on a radial band (P1.1 ambient). */
void phosphor_radial_field_apply_limited(glyph_frame_t *frame, float cx,
                                         float cy, float inner_radius,
                                         float outer_radius,
                                         uint8_t max_points,
                                         uint8_t max_intensity,
                                         glyph_color_id_t color);

#ifdef __cplusplus
}
#endif
