/*
 * Node snapshot/event -> ESPUI ViewModel adapter.
 *
 * Keeps protocol and UI state decoupled. The adapter is pure logic and is
 * covered by host tests; network transport is E4 work.
 */
#pragma once

#include <stdint.h>

#include "esp_err.h"
#include "espui_view_model.h"
#include "meet_protocol.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    uint64_t last_sequence;
    uint32_t applied_snapshots;
    uint32_t ignored_events;
} meet_state_adapter_t;

void meet_state_adapter_init(meet_state_adapter_t *adapter);

/*
 * Replaces business state from a snapshot while preserving device settings.
 * Returns ESP_ERR_INVALID_ARG for an unsupported schema or malformed counts.
 */
esp_err_t meet_state_adapter_apply_snapshot(
    meet_state_adapter_t *adapter, const meet_protocol_snapshot_t *snapshot,
    uint32_t now_ms, espui_view_model_t *model);

/*
 * E4: apply one sequenced event. E2 stub returns ESP_ERR_NOT_SUPPORTED and
 * leaves last_sequence untouched.
 */
esp_err_t meet_state_adapter_apply_event(meet_state_adapter_t *adapter,
                                         const meet_protocol_event_t *event,
                                         uint32_t now_ms,
                                         espui_view_model_t *model);

#ifdef __cplusplus
}
#endif
