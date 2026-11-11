#include <string.h>

#include "input_gesture.h"
#include "test_assert.h"

static size_t feed(espui_gesture_state_t *state, bool pressed, uint16_t x,
                   uint16_t y, uint32_t t, espui_input_event_t *out,
                   size_t max)
{
    return espui_gesture_feed(state, pressed, x, y, t, out, max);
}

static void start(espui_gesture_state_t *state, uint16_t x, uint16_t y,
                  uint32_t t, espui_input_event_t *out)
{
    size_t n = feed(state, true, x, y, t, out, ESPUI_GESTURE_MAX_EVENTS);
    TEST_ASSERT_EQ_INT(1, n);
    TEST_ASSERT_EQ_INT(ESPUI_INPUT_TOUCH, out[0].type);
    TEST_ASSERT_TRUE(out[0].pressed);
}

static void test_thresholds(void)
{
    espui_gesture_state_t state;
    espui_input_event_t out[ESPUI_GESTURE_MAX_EVENTS];

    /* Tap duration 299/300/301. */
    for (uint32_t duration = 299; duration <= 301; ++duration) {
        espui_gesture_init(&state);
        start(&state, 100, 100, 1000, out);
        size_t n = feed(&state, false, 100, 100, 1000 + duration, out,
                        ESPUI_GESTURE_MAX_EVENTS);
        TEST_ASSERT_EQ_INT(duration <= 300 ? 2 : 1, n);
        TEST_ASSERT_EQ_INT(ESPUI_INPUT_TOUCH, out[0].type);
        TEST_ASSERT_FALSE(out[0].pressed);
        if (duration <= 300) {
            TEST_ASSERT_EQ_INT(ESPUI_INPUT_TAP, out[1].type);
            TEST_ASSERT_EQ_INT(100, out[1].x);
            TEST_ASSERT_EQ_INT(1000 + (int)duration, out[1].timestamp_ms);
        }
    }

    /* Tap distance 7/8/9. */
    for (uint16_t distance = 7; distance <= 9; ++distance) {
        espui_gesture_init(&state);
        start(&state, 100, 100, 2000, out);
        size_t n = feed(&state, false, 100 + distance, 100,
                        2000 + 200, out, ESPUI_GESTURE_MAX_EVENTS);
        TEST_ASSERT_EQ_INT(distance <= 8 ? 2 : 1, n);
        if (distance <= 8) {
            TEST_ASSERT_EQ_INT(ESPUI_INPUT_TAP, out[1].type);
        }
    }

    /* Swipe distance 47/48/49 at 250 ms; 48/49 are swipes, 47 is a drag. */
    for (uint16_t distance = 47; distance <= 49; ++distance) {
        espui_gesture_init(&state);
        start(&state, 100, 100, 3000, out);
        size_t n = feed(&state, false, 100 + distance, 100,
                        3000 + 250, out, ESPUI_GESTURE_MAX_EVENTS);
        TEST_ASSERT_EQ_INT(distance >= 48 ? 2 : 1, n);
        if (distance >= 48) {
            TEST_ASSERT_EQ_INT(ESPUI_INPUT_SWIPE_RIGHT, out[1].type);
            TEST_ASSERT_EQ_INT(100 + (int)distance, out[1].x);
        }
    }

    /* Swipe duration 250 passes, 251 does not; no tap for a drag. */
    espui_gesture_init(&state);
    start(&state, 100, 100, 4000, out);
    size_t n = feed(&state, false, 149, 100, 4250, out,
                    ESPUI_GESTURE_MAX_EVENTS);
    TEST_ASSERT_EQ_INT(2, n);
    TEST_ASSERT_EQ_INT(ESPUI_INPUT_SWIPE_RIGHT, out[1].type);

    espui_gesture_init(&state);
    start(&state, 100, 100, 5000, out);
    n = feed(&state, false, 149, 100, 5251, out, ESPUI_GESTURE_MAX_EVENTS);
    TEST_ASSERT_EQ_INT(1, n);
}

static void test_long_once_and_debounce(void)
{
    espui_gesture_state_t state;
    espui_input_event_t out[ESPUI_GESTURE_MAX_EVENTS];
    espui_gesture_init(&state);
    start(&state, 50, 60, 1000, out);

    size_t n = feed(&state, true, 50, 60, 1599, out,
                    ESPUI_GESTURE_MAX_EVENTS);
    TEST_ASSERT_EQ_INT(0, n);
    n = feed(&state, true, 50, 60, 1600, out, ESPUI_GESTURE_MAX_EVENTS);
    TEST_ASSERT_EQ_INT(1, n);
    TEST_ASSERT_EQ_INT(ESPUI_INPUT_LONG_PRESS, out[0].type);
    TEST_ASSERT_EQ_INT(50, out[0].x);
    TEST_ASSERT_EQ_INT(60, out[0].y);
    TEST_ASSERT_EQ_INT(1600, out[0].timestamp_ms);

    n = feed(&state, true, 55, 65, 1750, out, ESPUI_GESTURE_MAX_EVENTS);
    TEST_ASSERT_EQ_INT(0, n);
    n = feed(&state, false, 55, 65, 1800, out, ESPUI_GESTURE_MAX_EVENTS);
    TEST_ASSERT_EQ_INT(1, n);
    TEST_ASSERT_EQ_INT(ESPUI_INPUT_TOUCH, out[0].type);
    TEST_ASSERT_FALSE(out[0].pressed);

    /* Press within 30 ms after release is debounced; 31 ms is accepted. */
    n = feed(&state, true, 10, 10, 1830, out, ESPUI_GESTURE_MAX_EVENTS);
    TEST_ASSERT_EQ_INT(0, n);
    n = feed(&state, true, 10, 10, 1831, out, ESPUI_GESTURE_MAX_EVENTS);
    TEST_ASSERT_EQ_INT(1, n);
    TEST_ASSERT_EQ_INT(ESPUI_INPUT_TOUCH, out[0].type);
    TEST_ASSERT_TRUE(out[0].pressed);

    /* If the long sample is missed, release still emits LONG exactly once. */
    espui_gesture_state_t long_state;
    espui_gesture_init(&long_state);
    start(&long_state, 20, 20, 8000, out);
    n = feed(&long_state, false, 20, 20, 8650, out,
             ESPUI_GESTURE_MAX_EVENTS);
    TEST_ASSERT_EQ_INT(2, n);
    TEST_ASSERT_EQ_INT(ESPUI_INPUT_TOUCH, out[0].type);
    TEST_ASSERT_FALSE(out[0].pressed);
    TEST_ASSERT_EQ_INT(ESPUI_INPUT_LONG_PRESS, out[1].type);
}

static void test_swipe_directions(void)
{
    espui_gesture_state_t state;
    espui_input_event_t out[ESPUI_GESTURE_MAX_EVENTS];
    struct {
        uint16_t dx;
        uint16_t dy;
        espui_input_type_t type;
    } cases[] = {
        {80, 0, ESPUI_INPUT_SWIPE_RIGHT},
        {0, 80, ESPUI_INPUT_SWIPE_DOWN},
    };
    for (size_t i = 0; i < sizeof(cases) / sizeof(cases[0]); ++i) {
        espui_gesture_init(&state);
        start(&state, 100, 100, 2000, out);
        size_t n = feed(&state, false, (uint16_t)(100 + cases[i].dx),
                        (uint16_t)(100 + cases[i].dy), 2200, out,
                        ESPUI_GESTURE_MAX_EVENTS);
        TEST_ASSERT_EQ_INT(2, n);
        TEST_ASSERT_EQ_INT(cases[i].type, out[1].type);
    }

    /* Left/up are the negative directions; use signed arithmetic in test. */
    espui_gesture_init(&state);
    start(&state, 180, 180, 3000, out);
    size_t n = feed(&state, false, 100, 180, 3200, out,
                    ESPUI_GESTURE_MAX_EVENTS);
    TEST_ASSERT_EQ_INT(ESPUI_INPUT_SWIPE_LEFT, out[1].type);
    TEST_ASSERT_EQ_INT(100, out[1].x);

    espui_gesture_init(&state);
    start(&state, 180, 180, 4000, out);
    n = feed(&state, false, 180, 100, 4200, out, ESPUI_GESTURE_MAX_EVENTS);
    TEST_ASSERT_EQ_INT(ESPUI_INPUT_SWIPE_UP, out[1].type);
    TEST_ASSERT_EQ_INT(100, out[1].y);

    /* Same-axis tie favors horizontal; short drag is not tap. */
    espui_gesture_init(&state);
    start(&state, 100, 100, 5000, out);
    feed(&state, true, 120, 100, 5050, out, ESPUI_GESTURE_MAX_EVENTS);
    n = feed(&state, false, 120, 100, 5200, out, ESPUI_GESTURE_MAX_EVENTS);
    TEST_ASSERT_EQ_INT(1, n);
}

static void test_mixed_inputs(void)
{
    espui_gesture_state_t state;
    espui_input_event_t out[ESPUI_GESTURE_MAX_EVENTS];
    espui_gesture_init(&state);
    uint32_t rng = 0x5eed1234u;
    uint32_t now = 1000;
    int raw_press = 0;
    int raw_release = 0;
    int gestures = 0;
    for (int i = 0; i < 1000; ++i) {
        rng = rng * 1664525u + 1013904223u;
        now += 20;
        bool pressed = (rng & 7u) != 0;
        uint16_t x = (uint16_t)(100 + ((rng >> 8) % 80));
        uint16_t y = (uint16_t)(100 + ((rng >> 16) % 80));
        size_t n = feed(&state, pressed, x, y, now, out,
                        ESPUI_GESTURE_MAX_EVENTS);
        TEST_ASSERT_TRUE(n <= ESPUI_GESTURE_MAX_EVENTS);
        for (size_t e = 0; e < n; ++e) {
            TEST_ASSERT_TRUE(out[e].type != ESPUI_INPUT_NONE);
            if (out[e].type == ESPUI_INPUT_TOUCH) {
                if (out[e].pressed) ++raw_press;
                else ++raw_release;
            } else {
                ++gestures;
            }
        }
    }
    TEST_ASSERT_TRUE(raw_press > 0);
    TEST_ASSERT_TRUE(raw_release > 0);
    TEST_ASSERT_TRUE(gestures > 0);
}

void test_input_gesture_run(void)
{
    test_thresholds();
    test_long_once_and_debounce();
    test_swipe_directions();
    test_mixed_inputs();
}
