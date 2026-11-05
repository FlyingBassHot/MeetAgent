#include "espui_tokens.h"

#include <ctype.h>
#include <string.h>

#include "espui_error.h"

const espui_tokens_t espui_tokens_safe_default_values = {
    .theme_id = "safe_fallback",
    .colors =
        {
            .background = 0x000000,
            .surface = 0x101010,
            .primary = 0xFFFFFF,
            .secondary = 0xC0C0C0,
            .muted = 0x808080,
            .warning = 0xFFC400,
            .error = 0xFF5252,
            .highlight = 0xFFFFFF,
        },
    .typography =
        {
            .primary_font = "default",
            .display_font = "default",
            .scale_percent = 100,
        },
    .motion =
        {
            .speed_percent = 100,
            .idle_amplitude_percent = 0,
            .work_speed_percent = 100,
            .transition_ms = 0,
        },
    .density_percent = 100,
};

const espui_tokens_t espui_tokens_terminal_default_values = {
    .theme_id = "terminal_aesthetic",
    .colors =
        {
            .background = 0x080C0A,
            .surface = 0x101813,
            .primary = 0xA7F36B,
            .secondary = 0x62C79B,
            .muted = 0x587364,
            .warning = 0xF2B75E,
            .error = 0xF06C67,
            .highlight = 0xD9FFC3,
        },
    .typography =
        {
            .primary_font = "terminal_mono",
            .display_font = "terminal_display",
            .scale_percent = 100,
        },
    .motion =
        {
            .speed_percent = 100,
            .idle_amplitude_percent = 30,
            .work_speed_percent = 100,
            .transition_ms = 220,
        },
    .density_percent = 100,
};

static int hex_nibble(char c)
{
    if (c >= '0' && c <= '9') {
        return c - '0';
    }
    if (c >= 'a' && c <= 'f') {
        return 10 + (c - 'a');
    }
    if (c >= 'A' && c <= 'F') {
        return 10 + (c - 'A');
    }
    return -1;
}

esp_err_t espui_tokens_parse_color(const char *text, uint32_t *out_rgb)
{
    if (text == NULL || out_rgb == NULL) {
        return ESP_ERR_INVALID_ARG;
    }

    if (text[0] == '#') {
        ++text;
    }

    if (strlen(text) != 6) {
        return ESP_ERR_INVALID_ARG;
    }

    uint32_t value = 0;
    for (size_t i = 0; i < 6; ++i) {
        int nibble = hex_nibble(text[i]);
        if (nibble < 0) {
            return ESP_ERR_INVALID_ARG;
        }
        value = (value << 4) | (uint32_t)nibble;
    }

    *out_rgb = value;
    return ESP_OK;
}

const espui_tokens_t *espui_tokens_safe_default(void)
{
    return &espui_tokens_safe_default_values;
}

const espui_tokens_t *espui_tokens_terminal_default(void)
{
    return &espui_tokens_terminal_default_values;
}
