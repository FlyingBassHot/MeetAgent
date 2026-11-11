#include "orbit_geometry.h"

#include <math.h>
#include <stddef.h>

#include "phosphor_lut.h"

#define PHOSPHOR_TWO_PI 6.28318530718f

static const phosphor_orbit_t s_orbits[3] = {
    /* Inner: nearly horizontal/broad ellipse. */
    {80.0f, 1.00f, -0.55f, 0.10f, 96},
    /* Middle: tilted to the upper right. */
    {140.0f, 0.25f, 0.85f, 1.90f, 64},
    /* Outer: tilted the other way; carries satellites. */
    {192.0f, 1.30f, -0.65f, 3.80f, 96},
};

const phosphor_orbit_t *phosphor_orbit_defaults(void)
{
    return s_orbits;
}

void phosphor_orbit_point(const phosphor_orbit_t *orbit, float theta,
                          float cx, float cy, phosphor_point_t *out)
{
    if (orbit == NULL || out == NULL) {
        return;
    }
    float ct = phosphor_lut_cos(theta);
    float st = phosphor_lut_sin(theta);
    float cr = phosphor_lut_cos(orbit->rotation);
    float sr = phosphor_lut_sin(orbit->rotation);
    float ctilt = phosphor_lut_cos(orbit->tilt);
    float stilt = phosphor_lut_sin(orbit->tilt);
    float r = orbit->radius;
    out->x = cx + r * ct * cr - r * st * ctilt * sr;
    out->y = cy + r * ct * sr + r * st * ctilt * cr;
    out->depth = r * st * stilt;
}

void phosphor_orbit_tangent(const phosphor_orbit_t *orbit, float theta,
                            float *dx, float *dy)
{
    if (orbit == NULL || dx == NULL || dy == NULL) {
        return;
    }
    float ct = phosphor_lut_cos(theta);
    float st = phosphor_lut_sin(theta);
    float cr = phosphor_lut_cos(orbit->rotation);
    float sr = phosphor_lut_sin(orbit->rotation);
    float ctilt = phosphor_lut_cos(orbit->tilt);
    float r = orbit->radius;
    *dx = -r * st * cr - r * ct * ctilt * sr;
    *dy = -r * st * sr + r * ct * ctilt * cr;
}

char phosphor_orbit_glyph(float dx, float dy)
{
    float ax = fabsf(dx);
    float ay = fabsf(dy);
    if (ax >= 2.0f * ay) {
        return '-';
    }
    if (ay >= 2.0f * ax) {
        return '|';
    }
    return dx * dy >= 0.0f ? '\\' : '/';
}

bool phosphor_point_to_cell(float x, float y, uint8_t *col, uint8_t *row)
{
    if (col == NULL || row == NULL || x < 0.0f || y < 0.0f ||
        x >= (float)PHOSPHOR_CANVAS_W || y >= (float)PHOSPHOR_CANVAS_H) {
        return false;
    }
    uint8_t c = (uint8_t)((int)x / PHOSPHOR_CELL_W);
    uint8_t r = (uint8_t)((int)y / PHOSPHOR_CELL_H);
    if (c >= PHOSPHOR_GRID_W || r >= PHOSPHOR_GRID_H) {
        return false;
    }
    *col = c;
    *row = r;
    return true;
}
