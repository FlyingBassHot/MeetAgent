#pragma once

#include <stdint.h>

#include "glyph_frame.h"
#include "phosphor_motion.h"
#include "phosphor_state.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    uint16_t approval_gap_cells;
    uint16_t error_break_cells;
    uint8_t completion_wave_layers;
    uint16_t offline_removed_cells;
} phosphor_core_metrics_t;

void phosphor_core_build_frame(glyph_frame_t *frame,
                               const phosphor_visual_state_t *visual,
                               phosphor_core_metrics_t *metrics);
void phosphor_core_build_frame_motion(
    glyph_frame_t *frame, const phosphor_visual_state_t *visual,
    const phosphor_motion_state_t *motion,
    phosphor_core_metrics_t *metrics);
void phosphor_core_render_rgb565(uint16_t *pixels, int width, int height,
                                 const phosphor_visual_state_t *visual,
                                 phosphor_core_metrics_t *metrics);
void phosphor_core_render_motion_rgb565(
    uint16_t *pixels, int width, int height,
    const phosphor_visual_state_t *visual,
    const phosphor_motion_state_t *motion,
    phosphor_core_metrics_t *metrics);

#ifdef __cplusplus
}
#endif
