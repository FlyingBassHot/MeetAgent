/*
 * ESPUI error codes.
 *
 * The numeric base is inside the ESP-IDF "user" range so codes can be logged
 * and inspected without colliding with framework errors.
 */
#pragma once

#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

#define ESPUI_ERR_BASE 0x3F00

#define ESPUI_ERR_MANIFEST (ESPUI_ERR_BASE + 0x01)
#define ESPUI_ERR_RENDERER_MISSING (ESPUI_ERR_BASE + 0x02)
#define ESPUI_ERR_THEME_INIT (ESPUI_ERR_BASE + 0x03)
#define ESPUI_ERR_THEME_NOT_FOUND (ESPUI_ERR_BASE + 0x04)
#define ESPUI_ERR_FALLBACK_FAILED (ESPUI_ERR_BASE + 0x05)
#define ESPUI_ERR_NOT_INITIALIZED (ESPUI_ERR_BASE + 0x06)
#define ESPUI_ERR_FULL (ESPUI_ERR_BASE + 0x07)
#define ESPUI_ERR_DISPLAY (ESPUI_ERR_BASE + 0x08)
#define ESPUI_ERR_QUEUE (ESPUI_ERR_BASE + 0x09)
#define ESPUI_ERR_STATE (ESPUI_ERR_BASE + 0x0A)
#define ESPUI_ERR_THEME_REJECTED (ESPUI_ERR_BASE + 0x0B)

#ifdef __cplusplus
}
#endif
