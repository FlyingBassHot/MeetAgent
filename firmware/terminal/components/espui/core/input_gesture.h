/*
 * Pure touch gesture recognizer.
 *
 * Feed one sample per input poll. Raw TOUCH press/release is preserved for
 * themes; discrete TAP / LONG_PRESS / SWIPE_* events are emitted once.
 */
#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "espui_types.h"

#ifdef __cplusplus
extern "C" {
#endif

#define ESPUI_GESTURE_TAP_MAX_MS 300u
#define ESPUI_GESTURE_LONG_MS 600u
#define ESPUI_GESTURE_SWIPE_MIN_PX 48u
#define ESPUI_GESTURE_SWIPE_MAX_MS 250u
#define ESPUI_GESTURE_TAP_MAX_PX 8u
#define ESPUI_GESTURE_DEBOUNCE_MS 30u
#define ESPUI_GESTURE_MAX_EVENTS 4

typedef struct {
    bool initialized;
    bool pressed;
    bool long_sent;
    uint16_t start_x;
    uint16_t start_y;
    uint16_t last_x;
    uint16_t last_y;
    uint32_t press_ms;
    uint32_t last_release_ms;
} espui_gesture_state_t;

void espui_gesture_init(espui_gesture_state_t *state);

/*
 * Feeds a timestamped touch sample. `pressed` may stay true across samples;
 * `x/y` are ignored for movement when not pressed (release position is the last
 * pressed position). Returns the number of events written to `out`.
 */
size_t espui_gesture_feed(espui_gesture_state_t *state, bool pressed, uint16_t x,
                          uint16_t y, uint32_t timestamp_ms,
                          espui_input_event_t *out, size_t max_events);

#ifdef __cplusplus
}
#endif
