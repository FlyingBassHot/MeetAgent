/*
 * Static asset registry.
 *
 * E2 assets are compiled into the firmware and referenced by name. The
 * registry exists so themes can ask the runtime for fonts/images without
 * knowing where they are stored. Dynamic loading is E3+ work.
 */
#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "esp_err.h"
#include "espui_error.h"
#include "espui_types.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    ESPUI_ASSET_FONT = 0,
    ESPUI_ASSET_IMAGE,
    ESPUI_ASSET_DATA,
} espui_asset_type_t;

typedef struct {
    bool in_use;
    char name[ESPUI_ID_MAX];
    espui_asset_type_t type;
    const void *data;
    size_t size;
} espui_asset_t;

typedef struct {
    espui_asset_t assets[ESPUI_MAX_ASSETS];
    uint8_t count;
} espui_asset_registry_t;

void espui_asset_registry_init(espui_asset_registry_t *registry);
esp_err_t espui_asset_registry_add(espui_asset_registry_t *registry,
                                   const char *name, espui_asset_type_t type,
                                   const void *data, size_t size);
const espui_asset_t *espui_asset_registry_find(
    const espui_asset_registry_t *registry, const char *name);
uint8_t espui_asset_registry_count(const espui_asset_registry_t *registry);

#ifdef __cplusplus
}
#endif
