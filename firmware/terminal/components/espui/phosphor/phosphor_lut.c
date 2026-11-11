#include "phosphor_lut.h"

#include <math.h>

#define PHOSPHOR_TWO_PI 6.28318530718f

static float s_sin[PHOSPHOR_LUT_SIZE];
static int s_initialized;

void phosphor_lut_init(void)
{
    if (s_initialized) {
        return;
    }
    for (int i = 0; i < PHOSPHOR_LUT_SIZE; ++i) {
        s_sin[i] = sinf(PHOSPHOR_TWO_PI * (float)i / (float)PHOSPHOR_LUT_SIZE);
    }
    s_initialized = 1;
}

static int lut_index(float radians)
{
    float turns = radians / PHOSPHOR_TWO_PI;
    turns -= floorf(turns);
    int index = (int)(turns * (float)PHOSPHOR_LUT_SIZE + 0.5f);
    return index & (PHOSPHOR_LUT_SIZE - 1);
}

float phosphor_lut_sin(float radians)
{
    phosphor_lut_init();
    return s_sin[lut_index(radians)];
}

float phosphor_lut_cos(float radians)
{
    return phosphor_lut_sin(radians + 1.57079632679f);
}
