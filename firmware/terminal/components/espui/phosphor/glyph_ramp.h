#pragma once

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Monotonic density ramp shared by PC and ESP32. */
const char *glyph_ramp(void);
uint8_t glyph_ramp_index(uint8_t intensity);
char glyph_ramp_char(uint8_t intensity);
uint8_t glyph_ramp_atlas(uint8_t intensity);

#ifdef __cplusplus
}
#endif
