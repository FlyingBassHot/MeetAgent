#include "glyph_frame.h"

#include <string.h>

uint8_t glyph_frame_layer_priority(uint8_t flags)
{
    if (flags & GLYPH_FLAG_NUCLEUS) {
        return 5;
    }
    if (flags & GLYPH_FLAG_SATELLITE) {
        return 4;
    }
    if (flags & GLYPH_FLAG_FRONT) {
        return 3;
    }
    if (flags & GLYPH_FLAG_BACK) {
        return 2;
    }
    if (flags & GLYPH_FLAG_FIELD) {
        return 1;
    }
    return 0;
}

void glyph_frame_clear(glyph_frame_t *frame)
{
    if (frame != NULL) {
        memset(frame, 0, sizeof(*frame));
    }
}

bool glyph_frame_set_scaled(glyph_frame_t *frame, uint8_t col, uint8_t row,
                            uint8_t glyph, glyph_color_id_t color,
                            uint8_t intensity, uint8_t flags, uint8_t scale)
{
    if (scale != 1 && scale != 2) {
        return false;
    }
    if (frame == NULL || col >= PHOSPHOR_GRID_W || row >= PHOSPHOR_GRID_H ||
        glyph == 0 || intensity == 0) {
        return false;
    }
    glyph_cell_t *cell = &frame->cells[row][col];
    uint8_t new_priority = glyph_frame_layer_priority(flags);
    uint8_t old_priority = glyph_frame_layer_priority(cell->flags);
    if (cell->glyph != 0 &&
        (new_priority < old_priority ||
         (new_priority == old_priority && intensity <= cell->intensity))) {
        return false;
    }
    cell->glyph = glyph;
    cell->color = (uint8_t)color;
    cell->intensity = intensity;
    cell->flags = flags;
    cell->scale = scale;
    return true;
}

bool glyph_frame_set(glyph_frame_t *frame, uint8_t col, uint8_t row,
                     uint8_t glyph, glyph_color_id_t color,
                     uint8_t intensity, uint8_t flags)
{
    return glyph_frame_set_scaled(frame, col, row, glyph, color, intensity,
                                  flags, 1);
}

const glyph_cell_t *glyph_frame_cell(const glyph_frame_t *frame, uint8_t col,
                                     uint8_t row)
{
    if (frame == NULL || col >= PHOSPHOR_GRID_W || row >= PHOSPHOR_GRID_H) {
        return NULL;
    }
    return &frame->cells[row][col];
}
