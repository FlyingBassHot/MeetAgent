#include <string.h>

#include "test_assert.h"
#include "espui_motion.h"

static void test_motion_profiles(void)
{
    const espui_motion_profile_t *none =
        espui_motion_profile_find("motion_none");
    TEST_ASSERT_NOT_NULL(none);
    TEST_ASSERT_STR_EQ("motion_none", none->id);
    TEST_ASSERT_EQ_INT(0, none->speed_percent);
    TEST_ASSERT_EQ_INT(0, none->idle_amplitude_percent);
    TEST_ASSERT_EQ_INT(0, none->target_fps);
    TEST_ASSERT_TRUE(none->reduced_motion);

    const espui_motion_profile_t *phosphor =
        espui_motion_profile_find("phosphor_default");
    TEST_ASSERT_NOT_NULL(phosphor);
    TEST_ASSERT_STR_EQ("phosphor_default", phosphor->id);
    TEST_ASSERT_EQ_INT(100, phosphor->speed_percent);
    TEST_ASSERT_EQ_INT(30, phosphor->idle_amplitude_percent);
    TEST_ASSERT_EQ_INT(100, phosphor->work_speed_percent);
    TEST_ASSERT_EQ_INT(220, phosphor->transition_ms);
    TEST_ASSERT_EQ_INT(15, phosphor->target_fps);
    TEST_ASSERT_FALSE(phosphor->reduced_motion);

    TEST_ASSERT_NULL(espui_motion_profile_find("missing"));
    TEST_ASSERT_NULL(espui_motion_profile_find(NULL));
    TEST_ASSERT_TRUE(espui_motion_profile_default() ==
                     espui_motion_profile_find("phosphor_default"));
}

void test_motion_profiles_run(void)
{
    test_motion_profiles();
}
