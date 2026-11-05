/*
 * Design tokens.
 *
 * Percent fields keep the on-device representation integer-only. JSON theme
 * packages (E3+) map 1.0 to 100, 0.3 to 30, etc.
 */
#pragma once

#include <stdint.h>

#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    uint32_t background; /* 0xRRGGBB */
    uint32_t surface;
    uint32_t primary;
    uint32_t secondary;
    uint32_t muted;
    uint32_t warning;
    uint32_t error;
    uint32_t highlight;
} espui_color_tokens_t;

typedef struct {
    const char *primary_font; /* Asset name, NULL = renderer default. */
    const char *display_font;
    uint16_t scale_percent; /* 100 = 1.0 */
} espui_typography_tokens_t;

typedef struct {
    uint16_t speed_percent;          /* 100 = 1.0 */
    uint8_t idle_amplitude_percent;  /* 0..100 */
    uint16_t work_speed_percent;     /* 100 = 1.0 */
    uint16_t transition_ms;
} espui_motion_tokens_t;

typedef struct {
    const char *theme_id;
    espui_color_tokens_t colors;
    espui_typography_tokens_t typography;
    espui_motion_tokens_t motion;
    uint8_t density_percent; /* 100 = default density */
} espui_tokens_t;

/* Parses "#RRGGBB" or "RRGGBB" into 0xRRGGBB. */
esp_err_t espui_tokens_parse_color(const char *text, uint32_t *out_rgb);

/* Compile-time token tables, also usable in static theme descriptors. */
extern const espui_tokens_t espui_tokens_safe_default_values;
extern const espui_tokens_t espui_tokens_terminal_default_values;

/* Minimal high-contrast fallback used when a theme has no tokens. */
const espui_tokens_t *espui_tokens_safe_default(void);

/* Terminal Aesthetic initial values from TerminalUi.md. */
const espui_tokens_t *espui_tokens_terminal_default(void);

#ifdef __cplusplus
}
#endif
