#include "espui_theme.h"

#include <string.h>

#include "espui_error.h"
#include "espui_util.h"

#define ESPUI_MAX_THEMES 8

static const espui_theme_descriptor_t *s_themes[ESPUI_MAX_THEMES];
static size_t s_theme_count;

void espui_theme_registry_reset(void)
{
    memset(s_themes, 0, sizeof(s_themes));
    s_theme_count = 0;
}

esp_err_t espui_theme_registry_add(const espui_theme_descriptor_t *theme)
{
    if (theme == NULL || theme->manifest == NULL || theme->ops == NULL ||
        espui_str_empty(theme->manifest->theme_id)) {
        return ESP_ERR_INVALID_ARG;
    }
    if (espui_theme_registry_find(theme->manifest->theme_id) != NULL) {
        return ESP_ERR_INVALID_ARG;
    }
    if (s_theme_count >= ESPUI_MAX_THEMES) {
        return ESPUI_ERR_FULL;
    }
    s_themes[s_theme_count++] = theme;
    return ESP_OK;
}

const espui_theme_descriptor_t *espui_theme_registry_find(const char *theme_id)
{
    if (espui_str_empty(theme_id)) {
        return NULL;
    }
    for (size_t i = 0; i < s_theme_count; ++i) {
        if (strcmp(s_themes[i]->manifest->theme_id, theme_id) == 0) {
            return s_themes[i];
        }
    }
    return NULL;
}

size_t espui_theme_registry_count(void)
{
    return s_theme_count;
}

const espui_theme_descriptor_t *espui_theme_registry_get(size_t index)
{
    return index < s_theme_count ? s_themes[index] : NULL;
}
