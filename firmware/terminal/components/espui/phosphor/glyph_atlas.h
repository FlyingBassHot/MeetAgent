#pragma once

#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define GLYPH_ATLAS_W 18
#define GLYPH_ATLAS_H 27
#define GLYPH_ATLAS_ROW_BYTES 3

uint8_t glyph_atlas_count(void);
uint8_t glyph_atlas_index(char c);
bool glyph_atlas_contains(char c);
char glyph_atlas_char_at(uint8_t index);
const uint8_t *glyph_atlas_bitmap(uint8_t index);
bool glyph_atlas_pixel(uint8_t index, uint8_t x, uint8_t y);

#ifdef __cplusplus
}
#endif
