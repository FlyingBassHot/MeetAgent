/*
 * Terminal Aesthetic — ASCII canvas primitives.
 *
 * Pure logic: fixed-size frame buffers only. No LVGL, FreeRTOS, malloc or
 * floating point. The LVGL character-grid widget (E3b) consumes the frame.
 */
#pragma once

#include <stdint.h>

#include "ascii_renderer.h"

#ifdef __cplusplus
extern "C" {
#endif

void espui_ascii_canvas_clear(espui_ascii_frame_t *frame);

/* Writes only when intensity is >= the existing cell intensity. */
void espui_ascii_canvas_put(espui_ascii_frame_t *frame, int x, int y,
                            char glyph, espui_ascii_color_t color,
                            uint8_t intensity);

/* Text overwrites the cell and clears spaces; clipped to the frame. */
void espui_ascii_canvas_text(espui_ascii_frame_t *frame, int x, int y,
                             const char *text, espui_ascii_color_t color,
                             uint8_t intensity);

/* Bresenham line with an auto glyph (- | / \). */
void espui_ascii_canvas_line(espui_ascii_frame_t *frame, int x0, int y0,
                             int x1, int y1, espui_ascii_color_t color,
                             uint8_t intensity);

/* Elliptical character ring; aspect ratio is corrected for tall cells. */
void espui_ascii_canvas_ring(espui_ascii_frame_t *frame, int cx, int cy,
                             int radius, char glyph, uint8_t dash_period,
                             uint8_t dash_on, uint32_t phase,
                             espui_ascii_color_t color, uint8_t intensity);

/* Box with + corners, - / | edges. gap > 0 breaks the top/bottom center. */
void espui_ascii_canvas_box(espui_ascii_frame_t *frame, int x, int y, int width,
                            int height, uint8_t gap, espui_ascii_color_t color,
                            uint8_t intensity);

void espui_ascii_canvas_scanline(espui_ascii_frame_t *frame, int y, char glyph,
                                 espui_ascii_color_t color, uint8_t intensity,
                                 uint8_t step);

/* Integer sine/cosine in Q7 (-127..127); angle is 0..63. */
int32_t espui_ascii_canvas_sin(uint8_t angle);
int32_t espui_ascii_canvas_cos(uint8_t angle);

#ifdef __cplusplus
}
#endif
