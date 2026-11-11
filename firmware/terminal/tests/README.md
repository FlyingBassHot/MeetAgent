# Host tests

These tests run on the development machine without ESP-IDF, LVGL, a panel or
FreeRTOS. They cover ESPUI's pure logic so regressions are caught before the
firmware can be compiled.

```bash
cd firmware/terminal/tests/host
make test      # build + run (ASan/UBSan, -Werror)
make preview STATE=working FRAMES=3 DELAY=300  # print ASCII frames
make syntax    # syntax/type-check every firmware C file with stubs
make clean
```

## What is real vs stubbed

Compiled into the test binary:

- `theme_api/*` (manifest, tokens, types, util, renderer registry)
- `core/action_dispatcher.c`, `view_model.c`, `input_router.c`, `navigation.c`
- `runtime/animation_scheduler.c`, `asset_manager.c`, `memory_manager.c`,
  `scene_manager.c`, `theme_manager.c`, `theme_registry.c`
- `themes/*` (descriptors and manifests; their render ops are not executed)
- `themes/terminal_aesthetic/renderers/ascii_renderer.c`
- `themes/terminal_aesthetic/renderers/ascii_canvas.c`
- `protocol/protocol.c`
- `state/state_adapter.c`

The ASCII preview CLI (`ascii_preview.c`) links only the ASCII module, ViewModel,
Motion and Animation Scheduler; it does not need ESP-IDF or LVGL.

Excluded from the host syntax check (needs managed BSP/LVGL headers, verified
by `idf.py build` instead):

- `main/board_lcd4.c`
- `components/networking/wifi_prov.c`

Excluded from the test binary (need FreeRTOS/LVGL):

- `core/espui.c` (UI task/runtime)
- `runtime/lvgl_renderer.c`
- `hal/*`, `state/mock_state.c`, `networking/*`, `main/app_main.c`

`stubs/esp_err.h`, `esp_log.h`, `esp_timer.h`, `freertos/*`, `lvgl.h` exist only
for host checks. They are never part of the firmware build.

`stubs/espui_runtime_stub.c` provides context accessors for the theme objects
so descriptors can be linked without the real UI task. Theme render ops are
never called in host tests.

ASCII preview examples:

```bash
make preview STATE=all FRAMES=1
make preview STATE=approval FRAMES=3 DELAY=300
```

## Adding tests

1. Add a `test_<area>.c` with a `void test_<area>_run(void)`.
2. Declare and `RUN(...)` it in `test_main.c`.
3. Use the assertions in `test_assert.h`.
4. `make test` and `make syntax` must both pass.

## Limitation

Passing these tests does not prove the firmware builds or displays correctly.
The missing ESP-IDF/LVGL/hardware checks are listed in
`../docs/BUILD_STATUS.md`.
