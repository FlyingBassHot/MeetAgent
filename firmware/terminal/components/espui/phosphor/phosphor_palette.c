#include "phosphor_palette.h"

#include <stddef.h>

static const phosphor_palette_t s_palette = {
    .background = 0x0861, /* #080C0A */
    .surface = 0x10C2,    /* #101813 */
    .primary = 0xA78D,    /* #A7F36B */
    .secondary = 0x6633,  /* #62C79B */
    .muted = 0x5B8C,      /* #587364 */
    .warning = 0xF5AB,    /* #F2B75E */
    .error = 0xF36C,      /* #F06C67 */
    .highlight = 0xDFF8,  /* #D9FFC3 */
};

const phosphor_palette_t *phosphor_palette_default(void)
{
    return &s_palette;
}

uint16_t phosphor_rgb565_from_hex(uint32_t rgb)
{
    uint16_t r = (uint16_t)((rgb >> 16) & 0xFFu);
    uint16_t g = (uint16_t)((rgb >> 8) & 0xFFu);
    uint16_t b = (uint16_t)(rgb & 0xFFu);
    return (uint16_t)(((r >> 3) << 11) | ((g >> 2) << 5) | (b >> 3));
}

uint16_t phosphor_color565(const phosphor_palette_t *palette,
                           glyph_color_id_t color)
{
    if (palette == NULL) {
        palette = phosphor_palette_default();
    }
    switch (color) {
    case GLYPH_COLOR_SECONDARY:
        return palette->secondary;
    case GLYPH_COLOR_PRIMARY:
        return palette->primary;
    case GLYPH_COLOR_HIGHLIGHT:
        return palette->highlight;
    case GLYPH_COLOR_WARNING:
        return palette->warning;
    case GLYPH_COLOR_ERROR:
        return palette->error;
    case GLYPH_COLOR_MUTED:
    default:
        return palette->muted;
    }
}

uint16_t phosphor_scale565(uint16_t color, uint8_t intensity)
{
    if (intensity >= 255) {
        return color;
    }
    if (intensity == 0) {
        return 0;
    }
    uint32_t r = (color >> 11) & 0x1Fu;
    uint32_t g = (color >> 5) & 0x3Fu;
    uint32_t b = color & 0x1Fu;
    r = (r * intensity) / 255u;
    g = (g * intensity) / 255u;
    b = (b * intensity) / 255u;
    return (uint16_t)((r << 11) | (g << 5) | b);
}
