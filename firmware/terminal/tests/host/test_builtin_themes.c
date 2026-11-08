#include <string.h>

#include "test_assert.h"
#include "espui_motion.h"
#include "espui_theme.h"

static espui_display_info_t display(void)
{
    espui_display_info_t info = {
        .panel_name = "test-panel-480",
        .width = 480,
        .height = 480,
        .orientation = ESPUI_ORIENTATION_ANY,
        .color_depth = 16,
        .supports_touch = true,
    };
    return info;
}

static void assert_theme_usable(const espui_theme_descriptor_t *theme)
{
    TEST_ASSERT_NOT_NULL(theme);
    TEST_ASSERT_NOT_NULL(theme->manifest);
    TEST_ASSERT_NOT_NULL(theme->ops);
    TEST_ASSERT_NOT_NULL(theme->ops->init);
    TEST_ASSERT_NOT_NULL(theme->ops->enter_scene);
    TEST_ASSERT_NOT_NULL(theme->ops->update);
    TEST_ASSERT_NOT_NULL(theme->ops->on_tick);
    TEST_ASSERT_NOT_NULL(theme->ops->on_input);
    TEST_ASSERT_NOT_NULL(theme->ops->on_action_result);
    TEST_ASSERT_NOT_NULL(theme->ops->destroy);
    TEST_ASSERT_NOT_NULL(
        espui_motion_profile_find(theme->manifest->motion_profile_id));

    espui_display_info_t info = display();
    TEST_ASSERT_EQ_INT(ESPUI_MANIFEST_OK,
                       espui_manifest_validate(theme->manifest, &info));
    TEST_ASSERT_EQ_INT((int)ESPUI_SCENE_MASK_ALL,
                       (int)theme->manifest->scene_mask);

    /* Old 320x480 layout must be rejected after the LCD-4 migration. */
    espui_display_info_t small = display();
    small.width = 320;
    small.height = 480;
    small.orientation = ESPUI_ORIENTATION_PORTRAIT;
    TEST_ASSERT_EQ_INT(ESPUI_MANIFEST_BAD_DISPLAY,
                       espui_manifest_validate(theme->manifest, &small));
}

static void test_builtin_registration(void)
{
    espui_theme_registry_reset();
    TEST_ASSERT_EQ_INT(0, espui_theme_registry_count());

    espui_register_builtin_themes();
    TEST_ASSERT_EQ_INT(2, espui_theme_registry_count());
    TEST_ASSERT_TRUE(espui_theme_registry_find("safe_fallback") ==
                     espui_theme_safe());
    TEST_ASSERT_TRUE(espui_theme_registry_find("terminal_aesthetic") ==
                     espui_theme_terminal_aesthetic());

    /* Idempotent. */
    espui_register_builtin_themes();
    TEST_ASSERT_EQ_INT(2, espui_theme_registry_count());
}

static void test_safe_theme(void)
{
    const espui_theme_descriptor_t *theme = espui_theme_safe();
    assert_theme_usable(theme);
    TEST_ASSERT_STR_EQ("safe_fallback", theme->manifest->theme_id);
    TEST_ASSERT_STR_EQ("builtin.label", theme->manifest->renderer_id);
    TEST_ASSERT_NOT_NULL(theme->tokens);
    TEST_ASSERT_STR_EQ("safe_fallback", theme->tokens->theme_id);
}

static void test_terminal_mock_theme(void)
{
    const espui_theme_descriptor_t *theme = espui_theme_terminal_aesthetic();
    assert_theme_usable(theme);
    TEST_ASSERT_STR_EQ("terminal_aesthetic", theme->manifest->theme_id);
    TEST_ASSERT_STR_EQ("builtin.label", theme->manifest->renderer_id);
    TEST_ASSERT_NOT_NULL(theme->tokens);
    TEST_ASSERT_STR_EQ("terminal_aesthetic", theme->tokens->theme_id);
    TEST_ASSERT_EQ_INT(0xA7F36B, theme->tokens->colors.primary);
    TEST_ASSERT_TRUE(espui_manifest_supports_scene(theme->manifest,
                                                   ESPUI_SCENE_ATTENTION));
}

void test_builtin_themes_run(void)
{
    test_builtin_registration();
    test_safe_theme();
    test_terminal_mock_theme();
}
