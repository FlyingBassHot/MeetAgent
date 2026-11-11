#pragma once

#include <stdbool.h>
#include <stdint.h>

#include "phosphor_types.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    float radius;
    float tilt;
    float rotation;
    float phase;
    uint32_t samples;
} phosphor_orbit_t;

const phosphor_orbit_t *phosphor_orbit_defaults(void);

void phosphor_orbit_point(const phosphor_orbit_t *orbit, float theta,
                          float cx, float cy, phosphor_point_t *out);
void phosphor_orbit_tangent(const phosphor_orbit_t *orbit, float theta,
                            float *dx, float *dy);
char phosphor_orbit_glyph(float dx, float dy);
bool phosphor_point_to_cell(float x, float y, uint8_t *col, uint8_t *row);

#ifdef __cplusplus
}
#endif
