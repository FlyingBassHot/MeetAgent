# Terminal Aesthetic — E2 Mock

This directory intentionally contains a **mock** of the Terminal Aesthetic
theme. It validates the theme pipeline before E3 implements the real renderer.

## What it is

- Theme id: `terminal_aesthetic`
- Manifest version: `0.2.0-mock`
- Renderer: `builtin.label` (LVGL labels/rects)
- Tokens: the Terminal Aesthetic colors from `TerminalUi.md`
- Layout: header, scene label, static ASCII core shape, status, strip, footer
- No animation in `on_tick`

## What it is not

- Not the E3 ASCII renderer
- No character grid, no six-state animation, no ambient/focus/activity layouts
- No custom fonts yet

## E3 plan

- [x] E3a: pure-logic ASCII renderer (character grid, density field, geometry,
  six states, motion profiles, completed pulse, scheduler/scene bindings).
  See `renderers/` and `docs/ASCII_RENDERER.md`.
- [ ] E3b: implement `builtin.ascii_terminal` LVGL character-grid widget and
  partial refresh.
- [ ] Change `renderer_id` in the C manifest and `manifest.json`.
- [ ] Move startup/status/scene rendering to the ASCII renderer.
- [ ] Add theme fonts and density settings.
- [ ] Wire the ASCII renderer into the live theme UI Task.

The theme id and tokens should remain stable so saved settings continue to
work across the renderer swap.
