#include <string.h>

#include "espui_view_model.h"
#include "test_assert.h"

static void test_activity_ring_order_and_capacity(void)
{
    espui_view_model_t model;
    espui_view_model_init(&model);
    TEST_ASSERT_EQ_INT(0, espui_view_model_activity_count(&model));
    TEST_ASSERT_NULL(espui_view_model_activity_at(&model, 0));

    for (int i = 0; i < ESPUI_MAX_ACTIVITY + 2; ++i) {
        char type[ESPUI_ACTIVITY_TYPE_MAX];
        char text[ESPUI_ACTIVITY_TEXT_MAX];
        snprintf(type, sizeof(type), "event.%d", i);
        snprintf(text, sizeof(text), "text-%d", i);
        espui_view_model_push_activity(&model, type, "ses-1", text,
                                       (uint32_t)(1000 + i));
    }

    TEST_ASSERT_EQ_INT(ESPUI_MAX_ACTIVITY,
                       espui_view_model_activity_count(&model));
    const espui_activity_view_t *first =
        espui_view_model_activity_at(&model, 0);
    const espui_activity_view_t *last =
        espui_view_model_activity_at(&model, ESPUI_MAX_ACTIVITY - 1);
    TEST_ASSERT_NOT_NULL(first);
    TEST_ASSERT_NOT_NULL(last);
    TEST_ASSERT_STR_EQ("event.2", first->type);
    TEST_ASSERT_STR_EQ("event.9", last->type);
    TEST_ASSERT_STR_EQ("ses-1", last->session_id);
    TEST_ASSERT_STR_EQ("text-9", last->short_text);
    TEST_ASSERT_EQ_INT(1009, last->timestamp_ms);
    TEST_ASSERT_NULL(espui_view_model_activity_at(&model,
                                                  ESPUI_MAX_ACTIVITY));

    espui_view_model_reset(&model, true);
    TEST_ASSERT_EQ_INT(0, espui_view_model_activity_count(&model));
}

void test_activity_ring_run(void)
{
    test_activity_ring_order_and_capacity();
}
