#include "glyph_ramp.h"

#include "glyph_atlas.h"

static const char s_ramp[] = " .,:-=+*#%@";

const char *glyph_ramp(void)
{
    return s_ramp;
}

uint8_t glyph_ramp_index(uint8_t intensity)
{
    return (uint8_t)(((uint16_t)intensity * 10u) / 255u);
}

char glyph_ramp_char(uint8_t intensity)
{
    return s_ramp[glyph_ramp_index(intensity)];
}

uint8_t glyph_ramp_atlas(uint8_t intensity)
{
    return glyph_atlas_index(glyph_ramp_char(intensity));
}
