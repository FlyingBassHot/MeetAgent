idf_component_register(
    SRCS
        "theme_api/espui_manifest.c"
        "theme_api/espui_motion.c"
        "theme_api/espui_tokens.c"
        "theme_api/espui_types.c"
        "theme_api/espui_util.c"
        "theme_api/renderer_registry.c"

        "core/action_dispatcher.c"
        "core/espui.c"
        "core/espui_view_model.c"
        "core/input_router.c"
        "core/navigation.c"

        "runtime/animation_scheduler.c"
        "runtime/asset_manager.c"
        "runtime/lvgl_renderer.c"
        "runtime/memory_manager.c"
        "runtime/scene_manager.c"
        "runtime/theme_manager.c"
        "runtime/theme_registry.c"

        "themes/builtin_themes.c"
        "themes/safe/safe_theme.c"
        "themes/terminal_aesthetic/terminal_theme.c"

        "hal/board_stub.c"
        "hal/display_stub.c"
        "hal/input_stub.c"
        "hal/power_stub.c"
        "hal/touch_stub.c"
    INCLUDE_DIRS
        "."
        "theme_api"
        "core"
        "runtime"
        "hal"
        "themes"
    PRIV_REQUIRES
        esp_timer
        lvgl
)
