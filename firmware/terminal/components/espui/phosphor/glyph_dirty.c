#include "glyph_dirty.h"

#include <stddef.h>
#include <string.h>

#include "glyph_atlas.h"
#include "glyph_rasterizer.h"

static bool cell_rendered(const glyph_cell_t *cell)
{
    return cell->glyph != 0 && cell->intensity != 0;
}

static bool cell_same(const glyph_cell_t *a, const glyph_cell_t *b)
{
    return a->glyph == b->glyph && a->color == b->color &&
           a->intensity == b->intensity && a->flags == b->flags &&
           a->scale == b->scale;
}

static void mark_footprint(glyph_dirty_state_t *state,
                           const glyph_cell_t *cell, uint8_t col, uint8_t row)
{
    if (!cell_rendered(cell)) return;
    int x = (int)col * PHOSPHOR_CELL_W;
    int y = (int)row * PHOSPHOR_CELL_H;
    int w = PHOSPHOR_CELL_W;
    int h = PHOSPHOR_CELL_H;
    if ((cell->flags & GLYPH_FLAG_NUCLEUS) != 0) {
        int scale = cell->scale == 0 ? 1 : cell->scale;
        w = GLYPH_ATLAS_W * scale;
        h = GLYPH_ATLAS_H * scale;
    }
    int c0 = x / PHOSPHOR_CELL_W;
    int c1 = (x + w - 1) / PHOSPHOR_CELL_W;
    int r0 = y / PHOSPHOR_CELL_H;
    int r1 = (y + h - 1) / PHOSPHOR_CELL_H;
    if (c0 < 0) c0 = 0;
    if (r0 < 0) r0 = 0;
    if (c1 >= PHOSPHOR_GRID_W) c1 = PHOSPHOR_GRID_W - 1;
    if (r1 >= PHOSPHOR_GRID_H) r1 = PHOSPHOR_GRID_H - 1;
    for (int r = r0; r <= r1; ++r) {
        for (int c = c0; c <= c1; ++c) {
            state->dirty[r][c] = 1;
        }
    }
}

static bool nucleus_dirty(const glyph_dirty_state_t *state,
                          const glyph_cell_t *cell, uint8_t col, uint8_t row)
{
    int scale = cell->scale == 0 ? 1 : cell->scale;
    int x = (int)col * PHOSPHOR_CELL_W;
    int y = (int)row * PHOSPHOR_CELL_H;
    int w = GLYPH_ATLAS_W * scale;
    int h = GLYPH_ATLAS_H * scale;
    int c0 = x / PHOSPHOR_CELL_W;
    int c1 = (x + w - 1) / PHOSPHOR_CELL_W;
    int r0 = y / PHOSPHOR_CELL_H;
    int r1 = (y + h - 1) / PHOSPHOR_CELL_H;
    if (c0 < 0) c0 = 0;
    if (r0 < 0) r0 = 0;
    if (c1 >= PHOSPHOR_GRID_W) c1 = PHOSPHOR_GRID_W - 1;
    if (r1 >= PHOSPHOR_GRID_H) r1 = PHOSPHOR_GRID_H - 1;
    for (int r = r0; r <= r1; ++r) {
        for (int c = c0; c <= c1; ++c) {
            if (state->dirty[r][c]) return true;
        }
    }
    return false;
}

void glyph_dirty_reset(glyph_dirty_state_t *state)
{
    if (state == NULL) return;
    memset(state, 0, sizeof(*state));
}

bool glyph_dirty_render(glyph_dirty_state_t *state,
                        const glyph_frame_t *frame, uint16_t *pixels,
                        int width, int height,
                        const phosphor_palette_t *palette,
                        glyph_dirty_bbox_t *bbox)
{
    if (bbox != NULL) memset(bbox, 0, sizeof(*bbox));
    if (state == NULL || frame == NULL || pixels == NULL || width <= 0 ||
        height <= 0) {
        return false;
    }
    if (palette == NULL) palette = phosphor_palette_default();

    if (!state->valid || state->width != width || state->height != height ||
        state->palette != palette) {
        glyph_rasterizer_render(frame, pixels, width, height, palette);
        state->previous = *frame;
        state->valid = true;
        state->width = width;
        state->height = height;
        state->palette = palette;
        state->last_changed_cells = PHOSPHOR_GRID_W * PHOSPHOR_GRID_H;
        state->last_dirty_cells = PHOSPHOR_GRID_W * PHOSPHOR_GRID_H;
        state->last_redraw_cells = PHOSPHOR_GRID_W * PHOSPHOR_GRID_H;
        if (bbox != NULL) {
            bbox->x = 0;
            bbox->y = 0;
            bbox->w = width;
            bbox->h = height;
            bbox->full = true;
        }
        return true;
    }

    memset(state->dirty, 0, sizeof(state->dirty));
    state->last_changed_cells = 0;
    state->last_dirty_cells = 0;
    state->last_redraw_cells = 0;
    for (uint8_t row = 0; row < PHOSPHOR_GRID_H; ++row) {
        for (uint8_t col = 0; col < PHOSPHOR_GRID_W; ++col) {
            const glyph_cell_t *old_cell = &state->previous.cells[row][col];
            const glyph_cell_t *new_cell = &frame->cells[row][col];
            if (cell_same(old_cell, new_cell)) continue;
            ++state->last_changed_cells;
            mark_footprint(state, old_cell, col, row);
            mark_footprint(state, new_cell, col, row);
        }
    }

    int min_c = PHOSPHOR_GRID_W, min_r = PHOSPHOR_GRID_H;
    int max_c = -1, max_r = -1;
    for (uint8_t row = 0; row < PHOSPHOR_GRID_H; ++row) {
        for (uint8_t col = 0; col < PHOSPHOR_GRID_W; ++col) {
            if (!state->dirty[row][col]) continue;
            ++state->last_dirty_cells;
            if (col < min_c) min_c = col;
            if (col > max_c) max_c = col;
            if (row < min_r) min_r = row;
            if (row > max_r) max_r = row;
        }
    }
    if (max_c < 0) {
        state->previous = *frame;
        return false;
    }

    /* Clear the dirty cell rectangles, then replay only the affected cells
     * in the same two-pass order as the full rasterizer. */
    for (uint8_t row = 0; row < PHOSPHOR_GRID_H; ++row) {
        for (uint8_t col = 0; col < PHOSPHOR_GRID_W; ++col) {
            if (!state->dirty[row][col]) continue;
            glyph_rasterizer_fill_rect(pixels, width, height,
                                       (int)col * PHOSPHOR_CELL_W,
                                       (int)row * PHOSPHOR_CELL_H,
                                       PHOSPHOR_CELL_W, PHOSPHOR_CELL_H,
                                       palette->background);
        }
    }
    for (uint8_t row = 0; row < PHOSPHOR_GRID_H; ++row) {
        for (uint8_t col = 0; col < PHOSPHOR_GRID_W; ++col) {
            const glyph_cell_t *cell = &frame->cells[row][col];
            if (!state->dirty[row][col] || !cell_rendered(cell) ||
                (cell->flags & GLYPH_FLAG_NUCLEUS) != 0) {
                continue;
            }
            glyph_rasterizer_draw_cell(cell, col, row, pixels, width, height,
                                       palette);
            ++state->last_redraw_cells;
        }
    }
    for (uint8_t row = 0; row < PHOSPHOR_GRID_H; ++row) {
        for (uint8_t col = 0; col < PHOSPHOR_GRID_W; ++col) {
            const glyph_cell_t *cell = &frame->cells[row][col];
            if (!cell_rendered(cell) ||
                (cell->flags & GLYPH_FLAG_NUCLEUS) == 0 ||
                !nucleus_dirty(state, cell, col, row)) {
                continue;
            }
            glyph_rasterizer_draw_cell(cell, col, row, pixels, width, height,
                                       palette);
            ++state->last_redraw_cells;
        }
    }

    state->previous = *frame;
    if (bbox != NULL) {
        bbox->x = min_c * PHOSPHOR_CELL_W;
        bbox->y = min_r * PHOSPHOR_CELL_H;
        bbox->w = (max_c - min_c + 1) * PHOSPHOR_CELL_W;
        bbox->h = (max_r - min_r + 1) * PHOSPHOR_CELL_H;
        if (bbox->x + bbox->w > width) bbox->w = width - bbox->x;
        if (bbox->y + bbox->h > height) bbox->h = height - bbox->y;
        bbox->full = false;
    }
    return true;
}
