#pragma once

#include <stdint.h>

#include "glyph_frame.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Builds the deterministic P1 static frame (no motion/state switching). */
void phosphor_scene_build_frame(glyph_frame_t *frame);

/* Renders the static frame into a 288x288 RGB565 canvas (or a clipped view). */
void phosphor_scene_render_rgb565(uint16_t *pixels, int width, int height);

#ifdef __cplusplus
}
#endif
