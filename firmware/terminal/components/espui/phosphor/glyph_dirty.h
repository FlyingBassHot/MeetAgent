#pragma once

#include <stdbool.h>
#include <stdint.h>

#include "glyph_frame.h"
#include "phosphor_palette.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    int x;
    int y;
    int w;
    int h;
    bool full;
} glyph_dirty_bbox_t;

typedef struct {
    glyph_frame_t previous;
    bool valid;
    int width;
    int height;
    const phosphor_palette_t *palette;
    uint8_t dirty[PHOSPHOR_GRID_H][PHOSPHOR_GRID_W];
    int last_changed_cells;
    int last_dirty_cells;
    int last_redraw_cells;
} glyph_dirty_state_t;

void glyph_dirty_reset(glyph_dirty_state_t *state);

/* Renders only changed cells. Returns true when pixels changed.
 * The first render, a size/palette change, or a close-to-full bbox uses the
 * full rasterizer and reports bbox.full=true. */
bool glyph_dirty_render(glyph_dirty_state_t *state,
                        const glyph_frame_t *frame, uint16_t *pixels,
                        int width, int height,
                        const phosphor_palette_t *palette,
                        glyph_dirty_bbox_t *bbox);

#ifdef __cplusplus
}
#endif
