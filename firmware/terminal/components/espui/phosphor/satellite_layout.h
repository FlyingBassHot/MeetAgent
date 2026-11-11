#pragma once

#include <stdbool.h>
#include <stdint.h>

#include "orbit_geometry.h"

#ifdef __cplusplus
extern "C" {
#endif

#define PHOSPHOR_SATELLITE_MAX 4

typedef enum {
    PHOSPHOR_SATELLITE_RUNNING = 0,
    PHOSPHOR_SATELLITE_WARNING,
} phosphor_satellite_status_t;

typedef struct {
    char session_id[40];
    phosphor_satellite_status_t status;
    float angle;
} phosphor_satellite_t;

typedef struct {
    phosphor_satellite_t items[PHOSPHOR_SATELLITE_MAX];
    uint8_t count;
} phosphor_satellite_layout_t;

void phosphor_satellite_layout_static(phosphor_satellite_layout_t *out);
bool phosphor_satellite_cell(const phosphor_orbit_t *orbit,
                             const phosphor_satellite_t *satellite,
                             float cx, float cy, uint8_t *col, uint8_t *row);

#ifdef __cplusplus
}
#endif
