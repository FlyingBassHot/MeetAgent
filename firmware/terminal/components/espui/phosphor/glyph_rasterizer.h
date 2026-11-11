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

#ifdef __cplusplus
}
#endif
