/*
 * Real ESPUI ViewModel -> PHOSPHOR input state adapter.
 *
 * Pure logic: no LVGL, no FreeRTOS, no JSON. The networking task calls this
 * after applying a snapshot/event; the PHOSPHOR animation task consumes the
 * published copy through phosphor_input_publisher.
 */
#pragma once

#include <stdbool.h>
#include <stdint.h>

#include "espui_view_model.h"
#include "phosphor_state.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    PHOSPHOR_CONNECTION_OFFLINE = 0,
    PHOSPHOR_CONNECTION_CONNECTING,
    PHOSPHOR_CONNECTION_ONLINE,
    PHOSPHOR_CONNECTION_ERROR,
} phosphor_connection_state_t;

typedef struct {
    bool valid; /* when false, the ViewModel node status is authoritative */
    phosphor_connection_state_t state;
} phosphor_connection_input_t;

/*
 * Maps the ViewModel to the fixed-size PHOSPHOR input.
 *
 * - ONLINE/SYNCING -> node_online=true; CONNECTING/OFFLINE -> false.
 * - A valid connection input is authoritative for transport loss:
 *   ONLINE -> true, ERROR/OFFLINE/CONNECTING -> false. CONNECTING maps to
 *   Offline because phosphor_input_state_t has no Stale/Syncing field; this
 *   avoids showing stale Working until the next welcome snapshot.
 * - session order is preserved (snapshot is already active-first).
 * - completion_event_hash is passed through; 0 means "no new completion".
 */
phosphor_input_state_t phosphor_view_model_to_input(
    const espui_view_model_t *model, uint64_t completion_event_hash,
    const phosphor_connection_input_t *connection);

/*
 * Stable non-zero hash for a completion event. Returns 0 when both the event
 * id and sequence are absent, so callers can pass 0 for non-completion events.
 */
uint64_t phosphor_completion_hash(const char *event_id, uint64_t sequence);

#ifdef __cplusplus
}
#endif
