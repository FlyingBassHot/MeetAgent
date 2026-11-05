#include "espui_renderer.h"

#include <string.h>

#include "espui_error.h"
#include "espui_util.h"

#define ESPUI_MAX_RENDERERS 4

static const espui_renderer_t *s_renderers[ESPUI_MAX_RENDERERS];
static size_t s_renderer_count;

void espui_renderer_registry_reset(void)
{
    memset(s_renderers, 0, sizeof(s_renderers));
    s_renderer_count = 0;
}

esp_err_t espui_renderer_registry_add(const espui_renderer_t *renderer)
{
    if (renderer == NULL || renderer->ops == NULL ||
        espui_str_empty(renderer->id)) {
        return ESP_ERR_INVALID_ARG;
    }
    if (espui_renderer_registry_find(renderer->id) != NULL) {
        return ESP_ERR_INVALID_ARG;
    }
    if (s_renderer_count >= ESPUI_MAX_RENDERERS) {
        return ESPUI_ERR_FULL;
    }
    s_renderers[s_renderer_count++] = renderer;
    return ESP_OK;
}

const espui_renderer_t *espui_renderer_registry_find(const char *id)
{
    if (espui_str_empty(id)) {
        return NULL;
    }
    for (size_t i = 0; i < s_renderer_count; ++i) {
        if (strcmp(s_renderers[i]->id, id) == 0) {
            return s_renderers[i];
        }
    }
    return NULL;
}

size_t espui_renderer_registry_count(void)
{
    return s_renderer_count;
}

const espui_renderer_t *espui_renderer_registry_get(size_t index)
{
    return index < s_renderer_count ? s_renderers[index] : NULL;
}
