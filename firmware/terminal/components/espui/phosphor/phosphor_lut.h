#pragma once

#ifdef __cplusplus
extern "C" {
#endif

#define PHOSPHOR_LUT_SIZE 1024

void phosphor_lut_init(void);
float phosphor_lut_sin(float radians);
float phosphor_lut_cos(float radians);

#ifdef __cplusplus
}
#endif
