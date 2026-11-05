#include "asset_manager.h"

#include <string.h>

#include "espui_util.h"

void espui_asset_registry_init(espui_asset_registry_t *registry)
{
    if (registry != NULL) {
        memset(registry, 0, sizeof(*registry));
    }
}

esp_err_t espui_asset_registry_add(espui_asset_registry_t *registry,
                                   const char *name, espui_asset_type_t type,
                                   const void *data, size_t size)
{
    if (registry == NULL || espui_str_empty(name) || data == NULL) {
        return ESP_ERR_INVALID_ARG;
    }
    if (espui_asset_registry_find(registry, name) != NULL) {
        return ESP_ERR_INVALID_ARG;
    }
    if (registry->count >= ESPUI_MAX_ASSETS) {
        return ESPUI_ERR_FULL;
    }

    for (size_t i = 0; i < ESPUI_MAX_ASSETS; ++i) {
        espui_asset_t *asset = &registry->assets[i];
        if (asset->in_use) {
            continue;
        }
        asset->in_use = true;
        espui_strlcpy(asset->name, name, sizeof(asset->name));
        asset->type = type;
        asset->data = data;
        asset->size = size;
        ++registry->count;
        return ESP_OK;
    }

    return ESPUI_ERR_FULL;
}

const espui_asset_t *espui_asset_registry_find(
    const espui_asset_registry_t *registry, const char *name)
{
    if (registry == NULL || espui_str_empty(name)) {
        return NULL;
    }
    for (size_t i = 0; i < ESPUI_MAX_ASSETS; ++i) {
        if (registry->assets[i].in_use &&
            strcmp(registry->assets[i].name, name) == 0) {
            return &registry->assets[i];
        }
    }
    return NULL;
}

uint8_t espui_asset_registry_count(const espui_asset_registry_t *registry)
{
    return registry == NULL ? 0 : registry->count;
}
