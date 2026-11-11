/*
 * Wi-Fi provisioning + STA + Node reachability (E4a).
 *
 * Flow:
 *   - no NVS config -> open SoftAP "MeetAgent-XXXX" + HTTP portal @192.168.4.1
 *   - config exists -> STA, 3 attempts, fallback to the same SoftAP
 *   - after STA gets an IP -> GET <node_url>/api/v2/info (HTTP only)
 *
 * Secrets (Wi-Fi password, pairing code) are stored in NVS and never logged
 * or exposed by /status. WebSocket/pairing/snapshot are E4b.
 */
#pragma once

#include <stdbool.h>
#include <stdint.h>

#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    MEET_WIFI_STATE_BOOT = 0,
    MEET_WIFI_STATE_AP,
    MEET_WIFI_STATE_AP_FALLBACK,
    MEET_WIFI_STATE_CONNECTING,
    MEET_WIFI_STATE_CONNECTED,
    MEET_WIFI_STATE_FAILED,
} meet_wifi_state_t;

typedef struct {
    meet_wifi_state_t state;
    char ssid[33];
    char ip[16];
    char node_url[128];
    bool node_reachable;
    char node_name[32];
    char node_version[32];
    int protocol_version;
    int schema_version;
    bool has_config;
} meet_wifi_status_t;

/* NVS must be initialized by app_main before calling this. */
esp_err_t meet_wifi_prov_start(void);

/* Thread-safe snapshot; never contains password/token/pairing code. */
void meet_wifi_prov_get_status(meet_wifi_status_t *out);

const char *meet_wifi_state_name(meet_wifi_state_t state);

#ifdef __cplusplus
}
#endif
