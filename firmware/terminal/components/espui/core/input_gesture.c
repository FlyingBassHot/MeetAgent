#include "input_gesture.h"

#include <stdlib.h>

static uint16_t abs_diff_u16(uint16_t a, uint16_t b)
{
    return a >= b ? (uint16_t)(a - b) : (uint16_t)(b - a);
}

static void emit(espui_input_event_t *out, size_t max, size_t *count,
                 espui_input_type_t type, uint16_t x, uint16_t y,
                 bool pressed, uint32_t timestamp_ms)
{
    if (*count >= max || out == NULL) return;
    espui_input_event_t *event = &out[(*count)++];
    event->type = type;
    event->value = 0;
    event->x = x;
    event->y = y;
    event->pressed = pressed;
    event->timestamp_ms = timestamp_ms;
}

void espui_gesture_init(espui_gesture_state_t *state)
{
    if (state == NULL) return;
    state->initialized = true;
    state->pressed = false;
    state->long_sent = false;
    state->start_x = 0;
    state->start_y = 0;
    state->last_x = 0;
    state->last_y = 0;
    state->press_ms = 0;
    state->last_release_ms = 0;
}

size_t espui_gesture_feed(espui_gesture_state_t *state, bool pressed, uint16_t x,
                          uint16_t y, uint32_t timestamp_ms,
                          espui_input_event_t *out, size_t max_events)
{
    size_t count = 0;
    if (state == NULL) return 0;
    if (!state->initialized) espui_gesture_init(state);

    if (pressed) {
        if (!state->pressed) {
            if (timestamp_ms - state->last_release_ms <=
                ESPUI_GESTURE_DEBOUNCE_MS) {
                return 0;
            }
            state->pressed = true;
            state->long_sent = false;
            state->start_x = x;
            state->start_y = y;
            state->last_x = x;
            state->last_y = y;
            state->press_ms = timestamp_ms;
            emit(out, max_events, &count, ESPUI_INPUT_TOUCH, x, y, true,
                 timestamp_ms);
            return count;
        }

        state->last_x = x;
        state->last_y = y;
        if (!state->long_sent &&
            timestamp_ms - state->press_ms >= ESPUI_GESTURE_LONG_MS) {
            state->long_sent = true;
            emit(out, max_events, &count, ESPUI_INPUT_LONG_PRESS, x, y, true,
                 timestamp_ms);
        }
        return count;
    }

    if (!state->pressed) {
        return 0;
    }

    state->pressed = false;
    state->last_release_ms = timestamp_ms;
    state->last_x = x;
    state->last_y = y;
    uint16_t end_x = x;
    uint16_t end_y = y;
    uint32_t duration = timestamp_ms - state->press_ms;
    uint16_t dx = abs_diff_u16(end_x, state->start_x);
    uint16_t dy = abs_diff_u16(end_y, state->start_y);
    uint16_t distance = dx > dy ? dx : dy;

    emit(out, max_events, &count, ESPUI_INPUT_TOUCH, end_x, end_y, false,
         timestamp_ms);
    if (!state->long_sent && duration >= ESPUI_GESTURE_LONG_MS) {
        state->long_sent = true;
        emit(out, max_events, &count, ESPUI_INPUT_LONG_PRESS, end_x, end_y,
             false, timestamp_ms);
        return count;
    }
    if (state->long_sent) {
        return count;
    }

    if (distance >= ESPUI_GESTURE_SWIPE_MIN_PX &&
        duration <= ESPUI_GESTURE_SWIPE_MAX_MS) {
        espui_input_type_t type;
        if (dx >= dy) {
            type = end_x >= state->start_x ? ESPUI_INPUT_SWIPE_RIGHT
                                           : ESPUI_INPUT_SWIPE_LEFT;
        } else {
            type = end_y >= state->start_y ? ESPUI_INPUT_SWIPE_DOWN
                                           : ESPUI_INPUT_SWIPE_UP;
        }
        emit(out, max_events, &count, type, end_x, end_y, false, timestamp_ms);
        return count;
    }

    if (distance <= ESPUI_GESTURE_TAP_MAX_PX &&
        duration <= ESPUI_GESTURE_TAP_MAX_MS) {
        emit(out, max_events, &count, ESPUI_INPUT_TAP, end_x, end_y, false,
             timestamp_ms);
    }
    return count;
}
