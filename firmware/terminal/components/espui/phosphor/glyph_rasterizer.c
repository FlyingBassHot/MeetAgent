#include "glyph_rasterizer.h"

#include <stddef.h>

#include "glyph_atlas.h"

static void fill_row_pairs(uint16_t *row, int x, int count, uint16_t color)
{
    uint32_t pair = (uint32_t)color | ((uint32_t)color << 16);
    int i = x;
    if ((((uintptr_t)&row[i]) & 3u) == 0u) {
        for (; i + 1 < x + count; i += 2) {
            *(uint32_t *)&row[i] = pair;
        }
    }
    for (; i < x + count; ++i) {
        row[i] = color;
    }
}

void glyph_rasterizer_fill_rect(uint16_t *pixels, int width, int height, int x,
                                int y, int w, int h, uint16_t color)
{
    if (x < 0) {
        w += x;
        x = 0;
    }
    if (y < 0) {
        h += y;
        y = 0;
    }
    if (x + w > width) w = width - x;
    if (y + h > height) h = height - y;
    if (w <= 0 || h <= 0) return;
    for (int row = 0; row < h; ++row) {
        fill_row_pairs(&pixels[(y + row) * width], x, w, color);
    }
}

void glyph_rasterizer_fill_background(uint16_t *pixels, int width, int height,
                                      uint16_t background)
{
    if (pixels == NULL || width <= 0 || height <= 0) return;
    for (int y = 0; y < height; ++y) {
        fill_row_pairs(&pixels[y * width], 0, width, background);
    }
}

void glyph_rasterizer_draw_cell(const glyph_cell_t *cell, uint8_t col,
                                uint8_t row, uint16_t *pixels, int width,
                                int height,
                                const phosphor_palette_t *palette)
{
    if (cell == NULL || pixels == NULL || width <= 0 || height <= 0 ||
        cell->glyph == 0 || cell->intensity == 0) {
        return;
    }
    if (palette == NULL) {
        palette = phosphor_palette_default();
    }
    uint8_t glyph_index = cell->glyph;
    uint16_t color = phosphor_scale565(
        phosphor_color565(palette, (glyph_color_id_t)cell->color),
        cell->intensity);
    int origin_x = (int)col * PHOSPHOR_CELL_W;
    int origin_y = (int)row * PHOSPHOR_CELL_H;
    bool nucleus = (cell->flags & GLYPH_FLAG_NUCLEUS) != 0;
    uint8_t scale = cell->scale == 0 ? 1 : cell->scale;

    if (nucleus) {
        int draw_w = GLYPH_ATLAS_W * scale;
        int draw_h = GLYPH_ATLAS_H * scale;
        glyph_rasterizer_fill_rect(pixels, width, height, origin_x, origin_y,
                                   draw_w, draw_h, palette->background);
    }

    for (int gy = 0; gy < GLYPH_ATLAS_H; ++gy) {
        int py = origin_y + gy * (nucleus ? scale : 1);
        if (nucleus) {
            for (int gx = 0; gx < GLYPH_ATLAS_W; ++gx) {
                if (!glyph_atlas_pixel(glyph_index, (uint8_t)gx, (uint8_t)gy)) {
                    continue;
                }
                int px = origin_x + gx * scale;
                if (px >= 0 && px + scale <= width && py >= 0 &&
                    py + scale <= height) {
                    for (int sy = 0; sy < scale; ++sy) {
                        for (int sx = 0; sx < scale; ++sx) {
                            pixels[(py + sy) * width + px + sx] = color;
                        }
                    }
                } else {
                    for (int sy = 0; sy < scale; ++sy) {
                        for (int sx = 0; sx < scale; ++sx) {
                            int qx = px + sx;
                            int qy = py + sy;
                            if (qx >= 0 && qx < width && qy >= 0 &&
                                qy < height) {
                                pixels[qy * width + qx] = color;
                            }
                        }
                    }
                }
            }
        } else {
            if (py < 0 || py >= height) continue;
            for (int gx = 0; gx < GLYPH_ATLAS_W; ++gx) {
                int px = origin_x + gx;
                if (px >= 0 && px < width &&
                    glyph_atlas_pixel(glyph_index, (uint8_t)gx, (uint8_t)gy)) {
                    pixels[py * width + px] = color;
                }
            }
        }
    }
}

void glyph_rasterizer_render(const glyph_frame_t *frame, uint16_t *pixels,
                             int width, int height,
                             const phosphor_palette_t *palette)
{
    if (palette == NULL) {
        palette = phosphor_palette_default();
    }
    if (pixels == NULL || width <= 0 || height <= 0) {
        return;
    }
    glyph_rasterizer_fill_background(pixels, width, height,
                                     palette->background);
    if (frame == NULL) {
        return;
    }

    /* Normal cells first. The Nucleus is deliberately a second pass so its
     * 2x scaled glyph is never covered by an orbit cell. */
    for (uint8_t row = 0; row < PHOSPHOR_GRID_H; ++row) {
        for (uint8_t col = 0; col < PHOSPHOR_GRID_W; ++col) {
            const glyph_cell_t *cell = &frame->cells[row][col];
            if ((cell->flags & GLYPH_FLAG_NUCLEUS) != 0) continue;
            glyph_rasterizer_draw_cell(cell, col, row, pixels, width, height,
                                       palette);
        }
    }
    for (uint8_t row = 0; row < PHOSPHOR_GRID_H; ++row) {
        for (uint8_t col = 0; col < PHOSPHOR_GRID_W; ++col) {
            const glyph_cell_t *cell = &frame->cells[row][col];
            if ((cell->flags & GLYPH_FLAG_NUCLEUS) == 0) continue;
            glyph_rasterizer_draw_cell(cell, col, row, pixels, width, height,
                                       palette);
        }
    }
}
