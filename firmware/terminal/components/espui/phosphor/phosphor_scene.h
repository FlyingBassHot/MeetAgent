#pragma once

#include <stdint.h>

#include "glyph_frame.h"
#include "phosphor_motion.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Builds the deterministic static frame (idle phase=0). */
void phosphor_scene_build_frame(glyph_frame_t *frame);

/* Renders the static frame into a 432x432 RGB565 canvas (or a clipped view). */
void phosphor_scene_render_rgb565(uint16_t *pixels, int width, int height);

/* P3: builds/renders from a pure motion snapshot. */
void phosphor_scene_build_frame_motion(glyph_frame_t *frame,
                                       const phosphor_motion_state_t *motion);
void phosphor_scene_render_motion_rgb565(
    uint16_t *pixels, int width, int height,
    const phosphor_motion_state_t *motion);

#ifdef __cplusplus
}
#endif
