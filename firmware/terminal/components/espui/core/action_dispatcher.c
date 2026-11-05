#include "action_dispatcher.h"

#include <string.h>

void espui_action_dispatcher_init(espui_action_dispatcher_t *dispatcher,
                                  espui_action_send_fn send, void *user_ctx)
{
    if (dispatcher == NULL) {
        return;
    }
    memset(dispatcher, 0, sizeof(*dispatcher));
    dispatcher->send = send;
    dispatcher->user_ctx = user_ctx;
    dispatcher->initialized = true;
}

esp_err_t espui_action_dispatcher_dispatch(
    espui_action_dispatcher_t *dispatcher, const espui_action_t *action)
{
    if (dispatcher == NULL || action == NULL ||
        action->type == ESPUI_ACTION_NONE) {
        return ESP_ERR_INVALID_ARG;
    }
    if (!dispatcher->initialized || dispatcher->send == NULL) {
        return ESP_ERR_INVALID_STATE;
    }
    return dispatcher->send(action, dispatcher->user_ctx);
}
