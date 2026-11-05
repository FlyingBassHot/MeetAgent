#include "memory_manager.h"

#include <string.h>

void espui_memory_budget_init(espui_memory_budget_t *budget,
                              uint32_t budget_bytes)
{
    if (budget == NULL) {
        return;
    }
    memset(budget, 0, sizeof(*budget));
    budget->budget_bytes = budget_bytes;
}

bool espui_memory_budget_reserve(espui_memory_budget_t *budget, uint32_t bytes)
{
    if (budget == NULL || bytes == 0) {
        return false;
    }
    if (budget->used_bytes >= budget->budget_bytes ||
        bytes > budget->budget_bytes - budget->used_bytes) {
        ++budget->failures;
        return false;
    }
    budget->used_bytes += bytes;
    if (budget->used_bytes > budget->peak_bytes) {
        budget->peak_bytes = budget->used_bytes;
    }
    return true;
}

void espui_memory_budget_release(espui_memory_budget_t *budget, uint32_t bytes)
{
    if (budget == NULL || bytes == 0) {
        return;
    }
    if (bytes >= budget->used_bytes) {
        budget->used_bytes = 0;
    } else {
        budget->used_bytes -= bytes;
    }
}

uint32_t espui_memory_budget_available(const espui_memory_budget_t *budget)
{
    if (budget == NULL || budget->used_bytes > budget->budget_bytes) {
        return 0;
    }
    return budget->budget_bytes - budget->used_bytes;
}

void espui_memory_budget_reset_peak(espui_memory_budget_t *budget)
{
    if (budget != NULL) {
        budget->peak_bytes = budget->used_bytes;
    }
}
