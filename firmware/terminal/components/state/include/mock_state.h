/*
 * Mock state source for E1/E2 bring-up.
 *
 * It posts complete ViewModels exactly like the future state adapter task.
 * Device settings are preserved by the UI task, so this source never needs to
 * know the active theme or brightness.
 */
#pragma once

#include <stdint.h>

#include "esp_err.h"
#include "espui_view_model.h"

#ifdef __cplusplus
extern "C" {
#endif

void meet_mock_state_init(espui_view_model_t *model);
esp_err_t meet_mock_state_start(uint32_t interval_ms);
/*
 * Requests the mock task to stop at its next loop boundary and waits up to
 * 1 second. The task deletes itself; callers must not delete it externally.
 */
esp_err_t meet_mock_state_stop(void);

#ifdef __cplusplus
}
#endif
