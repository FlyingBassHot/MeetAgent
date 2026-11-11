/*
 * Host-test stub for ESP-IDF nvs_flash.h. Only app_main syntax checking uses
 * this; the Wi-Fi provisioning implementation is checked by idf.py build.
 */
#pragma once

#include "esp_err.h"

#define ESP_ERR_NVS_NO_FREE_PAGES 0x1101
#define ESP_ERR_NVS_NEW_VERSION_FOUND 0x1102

#ifdef __cplusplus
extern "C" {
#endif

esp_err_t nvs_flash_init(void);
esp_err_t nvs_flash_erase(void);

#ifdef __cplusplus
}
#endif
