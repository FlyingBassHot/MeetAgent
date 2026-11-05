#include <stdint.h>

#include "test_assert.h"
#include "espui_tokens.h"

static void test_parse_color(void)
{
    uint32_t rgb = 0;

    TEST_ASSERT_EQ_INT(ESP_OK, espui_tokens_parse_color("#A7F36B", &rgb));
    TEST_ASSERT_EQ_INT(0xA7F36B, rgb);

    TEST_ASSERT_EQ_INT(ESP_OK, espui_tokens_parse_color("a7f36b", &rgb));
    TEST_ASSERT_EQ_INT(0xA7F36B, rgb);

    TEST_ASSERT_EQ_INT(ESP_OK, espui_tokens_parse_color("#080c0a", &rgb));
    TEST_ASSERT_EQ_INT(0x080C0A, rgb);

    TEST_ASSERT_EQ_INT(ESP_ERR_INVALID_ARG,
                       espui_tokens_parse_color(NULL, &rgb));
    TEST_ASSERT_EQ_INT(ESP_ERR_INVALID_ARG, espui_tokens_parse_color("#12345", &rgb));
    TEST_ASSERT_EQ_INT(ESP_ERR_INVALID_ARG,
                       espui_tokens_parse_color("#1234567", &rgb));
    TEST_ASSERT_EQ_INT(ESP_ERR_INVALID_ARG,
                       espui_tokens_parse_color("#GGGGGG", &rgb));
    TEST_ASSERT_EQ_INT(ESP_ERR_INVALID_ARG,
                       espui_tokens_parse_color("0x123456", &rgb));
    TEST_ASSERT_EQ_INT(ESP_ERR_INVALID_ARG, espui_tokens_parse_color("#123456", NULL));
}

static void test_safe_defaults(void)
{
    const espui_tokens_t *tokens = espui_tokens_safe_default();

    TEST_ASSERT_NOT_NULL(tokens);
    TEST_ASSERT_NOT_NULL(tokens->theme_id);
    TEST_ASSERT_STR_EQ("safe_fallback", tokens->theme_id);
    TEST_ASSERT_EQ_INT(100, tokens->typography.scale_percent);
    TEST_ASSERT_EQ_INT(100, tokens->motion.speed_percent);
    TEST_ASSERT_EQ_INT(0, tokens->motion.idle_amplitude_percent);
    TEST_ASSERT_EQ_INT(100, tokens->density_percent);
    TEST_ASSERT_EQ_INT(0x000000, tokens->colors.background);
    TEST_ASSERT_EQ_INT(0xFFFFFF, tokens->colors.primary);
}

static void test_terminal_defaults(void)
{
    const espui_tokens_t *tokens = espui_tokens_terminal_default();

    TEST_ASSERT_NOT_NULL(tokens);
    TEST_ASSERT_STR_EQ("terminal_aesthetic", tokens->theme_id);
    TEST_ASSERT_EQ_INT(0x080C0A, tokens->colors.background);
    TEST_ASSERT_EQ_INT(0x101813, tokens->colors.surface);
    TEST_ASSERT_EQ_INT(0xA7F36B, tokens->colors.primary);
    TEST_ASSERT_EQ_INT(0x62C79B, tokens->colors.secondary);
    TEST_ASSERT_EQ_INT(0x587364, tokens->colors.muted);
    TEST_ASSERT_EQ_INT(0xF2B75E, tokens->colors.warning);
    TEST_ASSERT_EQ_INT(0xF06C67, tokens->colors.error);
    TEST_ASSERT_EQ_INT(0xD9FFC3, tokens->colors.highlight);
    TEST_ASSERT_NOT_NULL(tokens->typography.primary_font);
    TEST_ASSERT_NOT_NULL(tokens->typography.display_font);
    TEST_ASSERT_EQ_INT(100, tokens->motion.speed_percent);
    TEST_ASSERT_EQ_INT(30, tokens->motion.idle_amplitude_percent);
    TEST_ASSERT_EQ_INT(220, tokens->motion.transition_ms);
}

void test_tokens_run(void)
{
    test_parse_color();
    test_safe_defaults();
    test_terminal_defaults();
}
