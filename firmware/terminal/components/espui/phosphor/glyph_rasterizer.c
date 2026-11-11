#include "glyph_rasterizer.h"

#include <stddef.h>

#include "glyph_atlas.h"

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
    for (int y = 0; y < height; ++y) {
        for (int x = 0; x < width; ++x) {
            pixels[y * width + x] = palette->background;
        }
    }
    if (frame == NULL) {
        return;
    }

    /* Normal cells first. The Nucleus is deliberately a second pass so its
     * 2x scaled glyph is never covered by an orbit cell. */
    for (uint8_t row = 0; row < PHOSPHOR_GRID_H; ++row) {
        for (uint8_t col = 0; col < PHOSPHOR_GRID_W; ++col) {
            const glyph_cell_t *cell = &frame->cells[row][col];
            if (cell->glyph == 0 || cell->intensity == 0 ||
                (cell->flags & GLYPH_FLAG_NUCLEUS) != 0) {
                continue;
            }
            uint8_t glyph_index = cell->glyph;
            uint16_t color = phosphor_scale565(
                phosphor_color565(palette, (glyph_color_id_t)cell->color),
                cell->intensity);
            int origin_x = (int)col * PHOSPHOR_CELL_W;
            int origin_y = (int)row * PHOSPHOR_CELL_H;
            for (int gy = 0; gy < GLYPH_ATLAS_H; ++gy) {
                int py = origin_y + gy;
                if (py < 0 || py >= height) {
                    continue;
                }
                for (int gx = 0; gx < GLYPH_ATLAS_W; ++gx) {
                    int px = origin_x + gx;
                    if (px < 0 || px >= width) {
                        continue;
                    }
                    if (glyph_atlas_pixel(glyph_index, (uint8_t)gx,
                                          (uint8_t)gy)) {
                        pixels[py * width + px] = color;
                    }
                }
            }
        }
    }

    for (uint8_t row = 0; row < PHOSPHOR_GRID_H; ++row) {
        for (uint8_t col = 0; col < PHOSPHOR_GRID_W; ++col) {
            const glyph_cell_t *cell = &frame->cells[row][col];
            if ((cell->flags & GLYPH_FLAG_NUCLEUS) == 0 ||
                cell->glyph == 0 || cell->intensity == 0) {
                continue;
            }
            uint8_t glyph_index = cell->glyph;
            uint8_t scale = cell->scale == 0 ? 1 : cell->scale;
            uint16_t color = phosphor_scale565(
                phosphor_color565(palette, (glyph_color_id_t)cell->color),
                cell->intensity);
            int origin_x = (int)col * PHOSPHOR_CELL_W;
            int origin_y = (int)row * PHOSPHOR_CELL_H;
            int draw_w = GLYPH_ATLAS_W * scale;
            int draw_h = GLYPH_ATLAS_H * scale;
            /* Clear the scaled footprint so no background/orbit glyph shows
             * through gaps in the bold nucleus bitmap. */
            for (int py = origin_y; py < origin_y + draw_h; ++py) {
                if (py < 0 || py >= height) {
                    continue;
                }
                for (int px = origin_x; px < origin_x + draw_w; ++px) {
                    if (px >= 0 && px < width) {
                        pixels[py * width + px] = palette->background;
                    }
                }
            }
            for (int gy = 0; gy < GLYPH_ATLAS_H; ++gy) {
                for (int gx = 0; gx < GLYPH_ATLAS_W; ++gx) {
                    if (!glyph_atlas_pixel(glyph_index, (uint8_t)gx,
                                           (uint8_t)gy)) {
                        continue;
                    }
                    for (int sy = 0; sy < scale; ++sy) {
                        for (int sx = 0; sx < scale; ++sx) {
                            int px = origin_x + gx * scale + sx;
                            int py = origin_y + gy * scale + sy;
                            if (px >= 0 && px < width && py >= 0 &&
                                py < height) {
                                pixels[py * width + px] = color;
                            }
                        }
                    }
                }
            }
        }
    }
}
