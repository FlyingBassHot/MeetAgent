#pragma once

#include <stdint.h>

#include "phosphor_state.h"

#ifdef __cplusplus
extern "C" {
#endif

#define PHOSPHOR_DEMO_CYCLE_MS 22000u

typedef enum {
    PHOSPHOR_DEMO_IDLE = 0,
    PHOSPHOR_DEMO_WORKING,
    PHOSPHOR_DEMO_APPROVAL,
    PHOSPHOR_DEMO_ERROR,
    PHOSPHOR_DEMO_COMPLETED,
    PHOSPHOR_DEMO_OFFLINE,
} phosphor_demo_phase_t;

phosphor_demo_phase_t phosphor_demo_phase_at(uint32_t elapsed_ms);
const char *phosphor_demo_phase_name(phosphor_demo_phase_t phase);
void phosphor_demo_build_input(phosphor_demo_phase_t phase, uint32_t cycle,
                               phosphor_input_state_t *input);

#ifdef __cplusplus
}
#endif
