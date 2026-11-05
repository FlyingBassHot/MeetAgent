/*
 * Host-test stub for ESP-IDF esp_log.h.
 */
#pragma once

#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    ESP_LOG_NONE,
    ESP_LOG_ERROR,
    ESP_LOG_WARN,
    ESP_LOG_INFO,
    ESP_LOG_DEBUG,
    ESP_LOG_VERBOSE,
} esp_log_level_t;

void esp_log_write(esp_log_level_t level, const char *tag, const char *format, ...);

#define ESP_LOGE(tag, format, ...) ((void)(tag))
#define ESP_LOGW(tag, format, ...) ((void)(tag))
#define ESP_LOGI(tag, format, ...) ((void)(tag))
#define ESP_LOGD(tag, format, ...) ((void)(tag))
#define ESP_LOGV(tag, format, ...) ((void)(tag))

#ifdef __cplusplus
}
#endif
