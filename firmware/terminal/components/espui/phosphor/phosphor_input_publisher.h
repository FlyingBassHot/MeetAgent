/*
 * Thread-safe PHOSPHOR input publisher.
 *
 * Networking publishes a full phosphor_input_state_t after ViewModel updates;
 * the PHOSPHOR animation task reads a snapshot. The publisher owns the only
 * cross-task state and never touches LVGL or the ViewModel.
 */
#pragma once

#include <stdbool.h>

#include "phosphor_state.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Idempotent; safe to call from app_main before the net/anim tasks start. */
void phosphor_input_publisher_init(void);

/*
 * Publishes a new input snapshot. from_snapshot=true marks the data as coming
 * from a real Node snapshot and clears the completion latch. Ordinary events
 * pass false; a zero completed_event_id then keeps the last completion hash
 * visible until the animation task has consumed it (the resolver dedupes).
 */
bool phosphor_input_publisher_publish(const phosphor_input_state_t *input,
                                      bool from_snapshot);

/*
 * Copies the latest snapshot. Returns false until the first publish.
 * has_real_snapshot distinguishes real Node data from connection-only updates,
 * so the caller can keep using the demo fallback before the first welcome.
 */
bool phosphor_input_publisher_read(phosphor_input_state_t *out,
                                   bool *has_real_snapshot);

/* Drops the snapshot and the real-data flag (network task shutdown/reset). */
void phosphor_input_publisher_clear(void);

#ifdef __cplusplus
}
#endif
