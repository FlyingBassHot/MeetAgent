#include "espui_manifest.h"

#include "espui_util.h"

static bool orientation_is_valid(espui_orientation_t orientation)
{
    return orientation == ESPUI_ORIENTATION_PORTRAIT ||
           orientation == ESPUI_ORIENTATION_LANDSCAPE ||
           orientation == ESPUI_ORIENTATION_ANY;
}

espui_manifest_status_t espui_manifest_validate_basic(
    const espui_manifest_t *manifest)
{
    if (manifest == NULL) {
        return ESPUI_MANIFEST_NULL_ARG;
    }
    if (espui_str_empty(manifest->theme_id) ||
        espui_str_empty(manifest->name) ||
        espui_str_empty(manifest->version)) {
        return ESPUI_MANIFEST_BAD_ID;
    }
    if (manifest->api_version != ESPUI_THEME_API_VERSION) {
        return ESPUI_MANIFEST_BAD_API;
    }
    if (espui_str_empty(manifest->renderer_id)) {
        return ESPUI_MANIFEST_BAD_RENDERER;
    }
    if (!orientation_is_valid(manifest->display.orientation) ||
        manifest->display.min_width == 0 ||
        manifest->display.min_height == 0) {
        return ESPUI_MANIFEST_BAD_DISPLAY;
    }

    uint32_t known_scenes = ESPUI_SCENE_MASK_ALL;
    if ((manifest->scene_mask & known_scenes) == 0) {
        return ESPUI_MANIFEST_NO_SCENES;
    }

    return ESPUI_MANIFEST_OK;
}

espui_manifest_status_t espui_manifest_validate(
    const espui_manifest_t *manifest, const espui_display_info_t *display)
{
    espui_manifest_status_t status = espui_manifest_validate_basic(manifest);
    if (status != ESPUI_MANIFEST_OK) {
        return status;
    }
    if (display == NULL || display->width == 0 || display->height == 0 ||
        display->color_depth == 0 || !orientation_is_valid(display->orientation)) {
        return ESPUI_MANIFEST_BAD_DISPLAY;
    }
    if (manifest->display.orientation != ESPUI_ORIENTATION_ANY &&
        manifest->display.orientation != display->orientation) {
        return ESPUI_MANIFEST_BAD_DISPLAY;
    }
    bool fits = display->width >= manifest->display.min_width &&
                display->height >= manifest->display.min_height;
    if (manifest->display.orientation == ESPUI_ORIENTATION_ANY) {
        /* Orientation-neutral themes may be rotated by the board port. */
        fits = fits || (display->width >= manifest->display.min_height &&
                        display->height >= manifest->display.min_width);
    }
    if (!fits) {
        return ESPUI_MANIFEST_BAD_DISPLAY;
    }
    return ESPUI_MANIFEST_OK;
}

const char *espui_manifest_status_name(espui_manifest_status_t status)
{
    switch (status) {
    case ESPUI_MANIFEST_OK:
        return "ok";
    case ESPUI_MANIFEST_NULL_ARG:
        return "null_argument";
    case ESPUI_MANIFEST_BAD_ID:
        return "bad_theme_identity";
    case ESPUI_MANIFEST_BAD_API:
        return "incompatible_api_version";
    case ESPUI_MANIFEST_BAD_RENDERER:
        return "bad_renderer";
    case ESPUI_MANIFEST_BAD_DISPLAY:
        return "unsupported_display";
    case ESPUI_MANIFEST_NO_SCENES:
        return "no_scenes";
    default:
        return "unknown";
    }
}

bool espui_manifest_supports_scene(const espui_manifest_t *manifest,
                                   espui_scene_id_t scene)
{
    if (manifest == NULL || !espui_scene_is_valid(scene)) {
        return false;
    }
    return (manifest->scene_mask & ESPUI_SCENE_MASK(scene)) != 0;
}
