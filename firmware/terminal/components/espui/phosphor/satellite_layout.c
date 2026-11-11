#include "satellite_layout.h"

#include <string.h>

void phosphor_satellite_layout_static(phosphor_satellite_layout_t *out)
{
    if (out == NULL) {
        return;
    }
    memset(out, 0, sizeof(*out));
    out->count = 2;
    strcpy(out->items[0].session_id, "P1-RUNNING");
    out->items[0].status = PHOSPHOR_SATELLITE_RUNNING;
    out->items[0].angle = 0.65f;
    strcpy(out->items[1].session_id, "P1-WARNING");
    out->items[1].status = PHOSPHOR_SATELLITE_WARNING;
    out->items[1].angle = 3.75f;
}

bool phosphor_satellite_cell(const phosphor_orbit_t *orbit,
                             const phosphor_satellite_t *satellite,
                             float cx, float cy, uint8_t *col, uint8_t *row)
{
    if (orbit == NULL || satellite == NULL || col == NULL || row == NULL) {
        return false;
    }
    phosphor_point_t point;
    phosphor_orbit_point(orbit, satellite->angle, cx, cy, &point);
    return phosphor_point_to_cell(point.x, point.y, col, row);
}
