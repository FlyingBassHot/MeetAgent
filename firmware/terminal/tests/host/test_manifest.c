#include <string.h>

#include "test_assert.h"
#include "espui_manifest.h"

static espui_manifest_t make_manifest(void)
{
    espui_manifest_t manifest = {
        .theme_id = "unit_test_theme",
        .name = "Unit Test Theme",
        .version = "1.0.0",
        .api_version = ESPUI_THEME_API_VERSION,
        .renderer_id = "builtin.label",
        .display =
            {
                .orientation = ESPUI_ORIENTATION_PORTRAIT,
                .min_width = 320,
                .min_height = 480,
            },
        .scene_mask = ESPUI_SCENE_MASK_ALL,
        .configurable_mask = ESPUI_CONFIG_BRIGHTNESS | ESPUI_CONFIG_MOTION_ENABLED,
        .motion_profile_id = "motion_none",
    };
    return manifest;
}

static espui_display_info_t make_display(void)
{
    espui_display_info_t display = {
        .panel_name = "test-panel",
        .width = 320,
        .height = 480,
        .orientation = ESPUI_ORIENTATION_PORTRAIT,
        .color_depth = 16,
        .supports_touch = true,
    };
    return display;
}

static void test_manifest_valid(void)
{
    espui_manifest_t manifest = make_manifest();
    espui_display_info_t display = make_display();

    TEST_ASSERT_EQ_INT(ESPUI_MANIFEST_OK,
                       espui_manifest_validate_basic(&manifest));
    TEST_ASSERT_EQ_INT(ESPUI_MANIFEST_OK,
                       espui_manifest_validate(&manifest, &display));
}

static void test_manifest_shape_rejections(void)
{
    espui_display_info_t display = make_display();
    espui_manifest_t manifest = make_manifest();

    TEST_ASSERT_EQ_INT(ESPUI_MANIFEST_NULL_ARG,
                       espui_manifest_validate_basic(NULL));
    TEST_ASSERT_EQ_INT(ESPUI_MANIFEST_NULL_ARG,
                       espui_manifest_validate(NULL, &display));

    manifest = make_manifest();
    manifest.api_version = ESPUI_THEME_API_VERSION + 1;
    TEST_ASSERT_EQ_INT(ESPUI_MANIFEST_BAD_API,
                       espui_manifest_validate_basic(&manifest));

    manifest = make_manifest();
    manifest.theme_id = "";
    TEST_ASSERT_EQ_INT(ESPUI_MANIFEST_BAD_ID,
                       espui_manifest_validate_basic(&manifest));

    manifest = make_manifest();
    manifest.name = NULL;
    TEST_ASSERT_EQ_INT(ESPUI_MANIFEST_BAD_ID,
                       espui_manifest_validate_basic(&manifest));

    manifest = make_manifest();
    manifest.version = " ";
    TEST_ASSERT_EQ_INT(ESPUI_MANIFEST_BAD_ID,
                       espui_manifest_validate_basic(&manifest));

    manifest = make_manifest();
    manifest.renderer_id = "";
    TEST_ASSERT_EQ_INT(ESPUI_MANIFEST_BAD_RENDERER,
                       espui_manifest_validate_basic(&manifest));

    manifest = make_manifest();
    manifest.scene_mask = 0;
    TEST_ASSERT_EQ_INT(ESPUI_MANIFEST_NO_SCENES,
                       espui_manifest_validate_basic(&manifest));

    manifest = make_manifest();
    manifest.scene_mask = 1u << (ESPUI_SCENE_COUNT + 2);
    TEST_ASSERT_EQ_INT(ESPUI_MANIFEST_NO_SCENES,
                       espui_manifest_validate_basic(&manifest));

    manifest = make_manifest();
    manifest.display.min_width = 0;
    TEST_ASSERT_EQ_INT(ESPUI_MANIFEST_BAD_DISPLAY,
                       espui_manifest_validate_basic(&manifest));

    manifest = make_manifest();
    manifest.display.orientation = (espui_orientation_t)99;
    TEST_ASSERT_EQ_INT(ESPUI_MANIFEST_BAD_DISPLAY,
                       espui_manifest_validate_basic(&manifest));
}

static void test_manifest_display_rejections(void)
{
    espui_manifest_t manifest = make_manifest();
    espui_display_info_t display = make_display();

    display.width = 240;
    display.height = 320;
    TEST_ASSERT_EQ_INT(ESPUI_MANIFEST_BAD_DISPLAY,
                       espui_manifest_validate(&manifest, &display));

    display = make_display();
    display.orientation = ESPUI_ORIENTATION_LANDSCAPE;
    TEST_ASSERT_EQ_INT(ESPUI_MANIFEST_BAD_DISPLAY,
                       espui_manifest_validate(&manifest, &display));

    display = make_display();
    display.width = 0;
    TEST_ASSERT_EQ_INT(ESPUI_MANIFEST_BAD_DISPLAY,
                       espui_manifest_validate(&manifest, &display));

    TEST_ASSERT_EQ_INT(ESPUI_MANIFEST_BAD_DISPLAY,
                       espui_manifest_validate(&manifest, NULL));

    manifest.display.orientation = ESPUI_ORIENTATION_ANY;
    display = make_display();
    display.orientation = ESPUI_ORIENTATION_LANDSCAPE;
    display.width = 480;
    display.height = 320;
    TEST_ASSERT_EQ_INT(ESPUI_MANIFEST_OK,
                       espui_manifest_validate(&manifest, &display));

    manifest.display.min_width = 300;
    manifest.display.min_height = 300;
    TEST_ASSERT_EQ_INT(ESPUI_MANIFEST_OK,
                       espui_manifest_validate(&manifest, &display));

    /* Landscape display can satisfy an orientation-neutral portrait theme. */
    manifest.display.min_width = 320;
    manifest.display.min_height = 480;
    TEST_ASSERT_EQ_INT(ESPUI_MANIFEST_OK,
                       espui_manifest_validate(&manifest, &display));
}

static void test_manifest_scene_helpers(void)
{
    espui_manifest_t manifest = make_manifest();

    TEST_ASSERT_EQ_INT(ESPUI_SCENE_MASK_ALL & ESPUI_SCENE_MASK_ALL,
                       manifest.scene_mask);
    TEST_ASSERT_TRUE(
        espui_manifest_supports_scene(&manifest, ESPUI_SCENE_AMBIENT));
    TEST_ASSERT_TRUE(
        espui_manifest_supports_scene(&manifest, ESPUI_SCENE_SETTINGS));
    TEST_ASSERT_FALSE(espui_manifest_supports_scene(&manifest,
                                                    ESPUI_SCENE_INVALID));

    manifest.scene_mask = ESPUI_SCENE_MASK(ESPUI_SCENE_AMBIENT);
    TEST_ASSERT_TRUE(
        espui_manifest_supports_scene(&manifest, ESPUI_SCENE_AMBIENT));
    TEST_ASSERT_FALSE(
        espui_manifest_supports_scene(&manifest, ESPUI_SCENE_FOCUS));
    TEST_ASSERT_FALSE(espui_manifest_supports_scene(NULL, ESPUI_SCENE_AMBIENT));

    TEST_ASSERT_NOT_NULL(espui_manifest_status_name(ESPUI_MANIFEST_OK));
    TEST_ASSERT_NOT_NULL(espui_manifest_status_name(ESPUI_MANIFEST_BAD_API));
    TEST_ASSERT_NOT_NULL(espui_manifest_status_name(ESPUI_MANIFEST_NO_SCENES));
}

void test_manifest_run(void)
{
    test_manifest_valid();
    test_manifest_shape_rejections();
    test_manifest_display_rejections();
    test_manifest_scene_helpers();
}
