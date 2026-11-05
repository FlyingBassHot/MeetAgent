/*
 * Action dispatcher.
 *
 * The only path from UI intents to Node commands. The network client supplies
 * a non-blocking send callback. Themes can request actions through the context
 * but cannot call the Node directly.
 */
#pragma once

#include <stdbool.h>

#include "esp_err.h"
#include "espui_types.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef esp_err_t (*espui_action_send_fn)(const espui_action_t *action,
                                          void *user_ctx);

typedef struct {
    espui_action_send_fn send;
    void *user_ctx;
    bool initialized;
} espui_action_dispatcher_t;

void espui_action_dispatcher_init(espui_action_dispatcher_t *dispatcher,
                                  espui_action_send_fn send, void *user_ctx);

/* Returns ESP_ERR_INVALID_STATE when no sender is registered. */
esp_err_t espui_action_dispatcher_dispatch(
    espui_action_dispatcher_t *dispatcher, const espui_action_t *action);

#ifdef __cplusplus
}
#endif
