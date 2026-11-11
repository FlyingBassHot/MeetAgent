#pragma once

#include <stdbool.h>
#include <stdint.h>

#include "phosphor_types.h"

#ifdef __cplusplus
extern "C" {
#endif

void glyph_frame_clear(glyph_frame_t *frame);
bool glyph_frame_set(glyph_frame_t *frame, uint8_t col, uint8_t row,
                     uint8_t glyph, glyph_color_id_t color,
                     uint8_t intensity, uint8_t flags);
bool glyph_frame_set_scaled(glyph_frame_t *frame, uint8_t col, uint8_t row,
                            uint8_t glyph, glyph_color_id_t color,
                            uint8_t intensity, uint8_t flags,
                            uint8_t scale);
const glyph_cell_t *glyph_frame_cell(const glyph_frame_t *frame, uint8_t col,
                                     uint8_t row);
uint8_t glyph_frame_layer_priority(uint8_t flags);

#ifdef __cplusplus
}
#endif
