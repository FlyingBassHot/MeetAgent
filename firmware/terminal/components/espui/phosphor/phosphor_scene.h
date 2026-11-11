#pragma once

#include <stdint.h>

#include "glyph_frame.h"
#include "phosphor_motion.h"
#include "phosphor_state.h"

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

/* P4: overlays resolver state on top of the P3.6 static/kinetic scene. */
void phosphor_scene_build_frame_visual(
    glyph_frame_t *frame, const phosphor_motion_state_t *motion,
    const phosphor_visual_state_t *visual);
void phosphor_scene_render_visual_rgb565(
    uint16_t *pixels, int width, int height,
    const phosphor_motion_state_t *motion,
    const phosphor_visual_state_t *visual);

#ifdef __cplusplus
}
#endif
