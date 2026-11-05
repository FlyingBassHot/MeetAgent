#include "ascii_canvas.h"

#include <string.h>

static const int8_t SINE_Q7[64] = {
    0,   12,  25,  37,  49,  60,  71,  81,  90,  98,  106, 112, 117, 122,
    125, 126, 127, 126, 125, 122, 117, 112, 106, 98,  90,  81,  71,  60,
    49,  37,  25,  12,  0,   -12, -25, -37, -49, -60, -71, -81, -90, -98,
    -106, -112, -117, -122, -125, -126, -127, -126, -125, -122, -117, -112,
    -106, -98, -90, -81, -71, -60, -49, -37, -25, -12,
};

static int abs_int(int value)
{
    return value < 0 ? -value : value;
}

int32_t espui_ascii_canvas_sin(uint8_t angle)
{
    return SINE_Q7[angle & 63u];
}

int32_t espui_ascii_canvas_cos(uint8_t angle)
{
    return SINE_Q7[(uint8_t)(angle + 16u) & 63u];
}

void espui_ascii_canvas_clear(espui_ascii_frame_t *frame)
{
    if (frame == NULL) {
        return;
    }
    for (uint8_t y = 0; y < frame->rows; ++y) {
        memset(frame->cells[y], ' ', frame->cols);
        frame->cells[y][frame->cols] = '\0';
        memset(frame->intensity[y], 0, frame->cols);
        memset(frame->color[y], 0, frame->cols);
    }
}

void espui_ascii_canvas_put(espui_ascii_frame_t *frame, int x, int y,
                            char glyph, espui_ascii_color_t color,
                            uint8_t intensity)
{
    if (frame == NULL || x < 0 || y < 0 || x >= frame->cols || y >= frame->rows ||
        glyph == '\0') {
        return;
    }
    if (intensity < frame->intensity[y][x]) {
        return;
    }
    frame->cells[y][x] = glyph;
    frame->intensity[y][x] = intensity;
    frame->color[y][x] = (uint8_t)color;
}

void espui_ascii_canvas_text(espui_ascii_frame_t *frame, int x, int y,
                             const char *text, espui_ascii_color_t color,
                             uint8_t intensity)
{
    if (frame == NULL || text == NULL || y < 0 || y >= frame->rows) {
        return;
    }
    for (const char *p = text; *p != '\0'; ++p, ++x) {
        if (x < 0 || x >= frame->cols) {
            continue;
        }
        if (*p == ' ') {
            frame->cells[y][x] = ' ';
            frame->intensity[y][x] = 0;
            frame->color[y][x] = (uint8_t)ESPUI_ASCII_COLOR_NONE;
            continue;
        }
        frame->cells[y][x] = *p;
        frame->intensity[y][x] = intensity;
        frame->color[y][x] = (uint8_t)color;
    }
}

void espui_ascii_canvas_line(espui_ascii_frame_t *frame, int x0, int y0,
                             int x1, int y1, espui_ascii_color_t color,
                             uint8_t intensity)
{
    if (frame == NULL) {
        return;
    }

    int dx = abs_int(x1 - x0);
    int dy = abs_int(y1 - y0);
    char glyph;
    if (dx >= dy * 2) {
        glyph = '-';
    } else if (dy >= dx * 2) {
        glyph = '|';
    } else if ((x1 - x0) * (y1 - y0) >= 0) {
        glyph = '\\';
    } else {
        glyph = '/';
    }

    int sx = x0 < x1 ? 1 : -1;
    int sy = y0 < y1 ? 1 : -1;
    int err = dx - dy;

    for (;;) {
        espui_ascii_canvas_put(frame, x0, y0, glyph, color, intensity);
        if (x0 == x1 && y0 == y1) {
            break;
        }
        int e2 = err * 2;
        if (e2 > -dy) {
            err -= dy;
            x0 += sx;
        }
        if (e2 < dx) {
            err += dx;
            y0 += sy;
        }
    }
}

void espui_ascii_canvas_ring(espui_ascii_frame_t *frame, int cx, int cy,
                             int radius, char glyph, uint8_t dash_period,
                             uint8_t dash_on, uint32_t phase,
                             espui_ascii_color_t color, uint8_t intensity)
{
    if (frame == NULL || radius <= 0) {
        return;
    }

    for (uint8_t i = 0; i < 64; ++i) {
        if (dash_period > 0) {
            uint8_t shift = (uint8_t)((phase >> 10) & 0xFFu);
            if ((uint8_t)((i + shift) % dash_period) >= dash_on) {
                continue;
            }
        }
        int x = cx + (int)((espui_ascii_canvas_cos(i) * radius * 2) / 127);
        int y = cy + (int)((espui_ascii_canvas_sin(i) * radius) / 127);
        espui_ascii_canvas_put(frame, x, y, glyph, color, intensity);
    }
}

void espui_ascii_canvas_box(espui_ascii_frame_t *frame, int x, int y, int width,
                            int height, uint8_t gap, espui_ascii_color_t color,
                            uint8_t intensity)
{
    if (frame == NULL || width < 2 || height < 2) {
        return;
    }

    int right = x + width - 1;
    int bottom = y + height - 1;
    int gap_start = x + width / 2 - gap / 2;
    int gap_end = gap_start + gap;

    for (int px = x; px <= right; ++px) {
        bool in_gap = gap > 0 && px >= gap_start && px < gap_end;
        if (!in_gap) {
            char glyph = (px == x || px == right) ? '+' : '-';
            espui_ascii_canvas_put(frame, px, y, glyph, color, intensity);
            espui_ascii_canvas_put(frame, px, bottom, glyph, color, intensity);
        }
    }

    for (int py = y + 1; py < bottom; ++py) {
        espui_ascii_canvas_put(frame, x, py, '|', color, intensity);
        espui_ascii_canvas_put(frame, right, py, '|', color, intensity);
    }
}

void espui_ascii_canvas_scanline(espui_ascii_frame_t *frame, int y, char glyph,
                                 espui_ascii_color_t color, uint8_t intensity,
                                 uint8_t step)
{
    if (frame == NULL || y < 0 || y >= frame->rows || glyph == '\0') {
        return;
    }
    if (step == 0) {
        step = 1;
    }
    for (int x = 1; x < (int)frame->cols - 1; x += step) {
        espui_ascii_canvas_put(frame, x, y, glyph, color, intensity);
    }
}
