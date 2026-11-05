/*
 * E2 networking stub.
 *
 * No Wi-Fi, pairing or WebSocket code yet. The contract stays real so app_main
 * and themes already use the final boundary. E4 replaces this file.
 */
#include "net_client.h"

#include <string.h>

#include "esp_log.h"

static const char *TAG = "meet_net";

static void set_state(meet_net_client_t *client, meet_net_state_t state)
{
    if (client == NULL) {
        return;
    }
    client->state = state;
    if (client->callbacks.on_state != NULL) {
        client->callbacks.on_state(state, client->user_ctx);
    }
}

esp_err_t meet_net_client_init(meet_net_client_t *client,
                               const meet_net_config_t *config,
                               const meet_net_callbacks_t *callbacks,
                               void *user_ctx)
{
    if (client == NULL || config == NULL || callbacks == NULL) {
        return ESP_ERR_INVALID_ARG;
    }
    memset(client, 0, sizeof(*client));
    client->config = *config;
    client->callbacks = *callbacks;
    client->user_ctx = user_ctx;
    client->initialized = true;
    client->state = MEET_NET_DISCONNECTED;
    return ESP_OK;
}

esp_err_t meet_net_client_start(meet_net_client_t *client)
{
    if (client == NULL || !client->initialized) {
        return ESP_ERR_INVALID_STATE;
    }
    ESP_LOGW(TAG, "network transport is an E4 task");
    set_state(client, MEET_NET_ERROR);
    return ESP_ERR_NOT_SUPPORTED;
}

esp_err_t meet_net_client_stop(meet_net_client_t *client)
{
    if (client == NULL || !client->initialized) {
        return ESP_ERR_INVALID_STATE;
    }
    set_state(client, MEET_NET_DISCONNECTED);
    return ESP_OK;
}

esp_err_t meet_net_client_send_action(meet_net_client_t *client,
                                      const espui_action_t *action)
{
    (void)action;
    if (client == NULL || !client->initialized) {
        return ESP_ERR_INVALID_STATE;
    }
    ESP_LOGW(TAG, "command send is an E4 task");
    return ESP_ERR_NOT_SUPPORTED;
}

meet_net_state_t meet_net_client_state(const meet_net_client_t *client)
{
    return client == NULL ? MEET_NET_DISCONNECTED : client->state;
}

const char *meet_net_state_name(meet_net_state_t state)
{
    switch (state) {
    case MEET_NET_DISCONNECTED:
        return "disconnected";
    case MEET_NET_WIFI_CONNECTING:
        return "wifi_connecting";
    case MEET_NET_NODE_CONNECTING:
        return "node_connecting";
    case MEET_NET_ONLINE:
        return "online";
    case MEET_NET_ERROR:
        return "error";
    default:
        return "unknown";
    }
}
