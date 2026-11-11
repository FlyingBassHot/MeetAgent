/*
 * PHOSPHOR CORE pure types (P1 static core).
 *
 * No LVGL / ESP-IDF / cJSON dependencies. Coordinates are in the 288x288
 * Core Canvas; the 36x24 glyph frame is rasterized to RGB565.
 */
#pragma once

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define PHOSPHOR_CANVAS_W 432
#define PHOSPHOR_CANVAS_H 432
#define PHOSPHOR_CANVAS_X 24
#define PHOSPHOR_CANVAS_Y 24
#define PHOSPHOR_GRID_W 24
#define PHOSPHOR_GRID_H 16
#define PHOSPHOR_CELL_W 18
#define PHOSPHOR_CELL_H 27

#define PHOSPHOR_NUCLEUS_COL 10
#define PHOSPHOR_NUCLEUS_ROW 7
#define PHOSPHOR_NUCLEUS_SCALE 2

typedef enum {
    GLYPH_COLOR_MUTED = 0,
    GLYPH_COLOR_SECONDARY,
    GLYPH_COLOR_PRIMARY,
    GLYPH_COLOR_HIGHLIGHT,
    GLYPH_COLOR_WARNING,
    GLYPH_COLOR_ERROR,
    GLYPH_COLOR_COUNT,
} glyph_color_id_t;

#define GLYPH_FLAG_FRONT (1u << 0)
#define GLYPH_FLAG_BACK (1u << 1)
#define GLYPH_FLAG_FIELD (1u << 2)
#define GLYPH_FLAG_SATELLITE (1u << 3)
#define GLYPH_FLAG_NUCLEUS (1u << 4)
#define GLYPH_FLAG_INNER_BAND (1u << 5)
#define GLYPH_FLAG_OUTER_BAND (1u << 6)

typedef struct {
    uint8_t glyph;
    uint8_t color;
    uint8_t intensity;
    uint8_t flags;
    uint8_t scale;
} glyph_cell_t;

typedef struct {
    glyph_cell_t cells[PHOSPHOR_GRID_H][PHOSPHOR_GRID_W];
} glyph_frame_t;

typedef struct {
    float x;
    float y;
    float depth;
} phosphor_point_t;

#ifdef __cplusplus
}
#endif
