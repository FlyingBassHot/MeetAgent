/*
 * Networking client contract (E4 implementation).
 *
 * E2 ships a stub so app_main and the action dispatcher have a real boundary
 * to wire against. The final implementation owns Wi-Fi, HTTP pairing, WSS and
 * heartbeat; it calls the state adapter and posts ViewModels to ESPUI.
 */
#pragma once

#include <stdbool.h>
#include <stdint.h>

#include "esp_err.h"
#include "espui_types.h"
#include "meet_protocol.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    MEET_NET_DISCONNECTED = 0,
    MEET_NET_WIFI_CONNECTING,
    MEET_NET_NODE_CONNECTING,
    MEET_NET_ONLINE,
    MEET_NET_ERROR,
} meet_net_state_t;

typedef struct {
    const char *host;   /* Caller-owned. */
    uint16_t port;      /* 0 => 8787 */
    const char *path;   /* NULL => /ws */
    const char *token;  /* Caller-owned NVS copy; never log it. */
    bool use_tls;       /* LAN first; WSS can be enabled later. */
} meet_net_config_t;

typedef void (*meet_net_state_cb_t)(meet_net_state_t state, void *user_ctx);
typedef void (*meet_net_snapshot_cb_t)(const meet_protocol_snapshot_t *snapshot,
                                       void *user_ctx);
typedef void (*meet_net_event_cb_t)(const meet_protocol_event_t *event,
                                    void *user_ctx);
typedef void (*meet_net_action_result_cb_t)(
    const espui_action_result_t *result, void *user_ctx);

typedef struct {
    meet_net_state_cb_t on_state;
    meet_net_snapshot_cb_t on_snapshot;
    meet_net_event_cb_t on_event;
    meet_net_action_result_cb_t on_action_result;
} meet_net_callbacks_t;

typedef struct {
    meet_net_config_t config;
    meet_net_callbacks_t callbacks;
    void *user_ctx;
    meet_net_state_t state;
    bool initialized;
} meet_net_client_t;

esp_err_t meet_net_client_init(meet_net_client_t *client,
                               const meet_net_config_t *config,
                               const meet_net_callbacks_t *callbacks,
                               void *user_ctx);
esp_err_t meet_net_client_start(meet_net_client_t *client);
esp_err_t meet_net_client_stop(meet_net_client_t *client);
esp_err_t meet_net_client_send_action(meet_net_client_t *client,
                                      const espui_action_t *action);
meet_net_state_t meet_net_client_state(const meet_net_client_t *client);
const char *meet_net_state_name(meet_net_state_t state);

#ifdef __cplusplus
}
#endif
