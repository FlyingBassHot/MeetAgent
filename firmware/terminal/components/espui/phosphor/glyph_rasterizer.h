#pragma once

#include <stdint.h>

#include "glyph_frame.h"
#include "phosphor_palette.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Fills the full RGB565 canvas with background, then renders glyph cells. */
void glyph_rasterizer_render(const glyph_frame_t *frame, uint16_t *pixels,
                             int width, int height,
                             const phosphor_palette_t *palette);

/* Draws one cell's glyph (including the scaled Nucleus footprint). A normal
 * call does not clear background; a Nucleus call clears its scaled footprint
 * first, matching the full renderer's second pass. */
void glyph_rasterizer_draw_cell(const glyph_cell_t *cell, uint8_t col,
                                uint8_t row, uint16_t *pixels, int width,
                                int height,
                                const phosphor_palette_t *palette);

/* Fast background fill; pairs two RGB565 pixels per 32-bit write. */
void glyph_rasterizer_fill_background(uint16_t *pixels, int width, int height,
                                      uint16_t background);
void glyph_rasterizer_fill_rect(uint16_t *pixels, int width, int height, int x,
                                int y, int w, int h, uint16_t color);

#ifdef __cplusplus
}
#endif
