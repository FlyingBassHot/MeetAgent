/*
 * Memory budget bookkeeping.
 *
 * This is not an allocator. It tracks UI-owned allocations against a fixed
 * budget so a theme or renderer cannot silently exhaust PSRAM. All calls are
 * expected on the UI task.
 */
#pragma once

#include <stdbool.h>
#include <stdint.h>

#include "espui_types.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    uint32_t budget_bytes;
    uint32_t used_bytes;
    uint32_t peak_bytes;
    uint32_t failures;
} espui_memory_budget_t;

void espui_memory_budget_init(espui_memory_budget_t *budget,
                              uint32_t budget_bytes);
bool espui_memory_budget_reserve(espui_memory_budget_t *budget,
                                 uint32_t bytes);
void espui_memory_budget_release(espui_memory_budget_t *budget,
                                 uint32_t bytes);
uint32_t espui_memory_budget_available(const espui_memory_budget_t *budget);
void espui_memory_budget_reset_peak(espui_memory_budget_t *budget);

#ifdef __cplusplus
}
#endif
