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

1. Implement `builtin.ascii_terminal` (character grid + partial refresh).
2. Change `renderer_id` in the C manifest and `manifest.json`.
3. Move startup/status/scene rendering to the ASCII renderer.
4. Add motion profiles and the six states: idle / working / completed /
   approval / error / offline.
5. Add theme fonts and density settings.

The theme id and tokens should remain stable so saved settings continue to
work across the renderer swap.
