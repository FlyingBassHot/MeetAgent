#include "espui_scene.h"

#include <string.h>

const char *espui_scene_name(espui_scene_id_t scene)
{
    switch (scene) {
    case ESPUI_SCENE_AMBIENT:
        return "ambient";
    case ESPUI_SCENE_FOCUS:
        return "focus";
    case ESPUI_SCENE_ACTIVITY:
        return "activity";
    case ESPUI_SCENE_ATTENTION:
        return "attention";
    case ESPUI_SCENE_SETTINGS:
        return "settings";
    default:
        return "invalid";
    }
}

bool espui_scene_is_valid(espui_scene_id_t scene)
{
    return (unsigned)scene < (unsigned)ESPUI_SCENE_COUNT;
}

static void notify(const espui_scene_manager_t *manager,
                   espui_scene_id_t scene)
{
    if (manager->listener != NULL) {
        manager->listener(scene, manager->listener_ctx);
    }
}

static void push_history(espui_scene_manager_t *manager, espui_scene_id_t scene)
{
    if (manager->depth < ESPUI_MAX_SCENE_STACK) {
        manager->stack[manager->depth++] = scene;
        return;
    }

    /* Bounded history: drop the oldest entry, keep the most recent ones. */
    if (ESPUI_MAX_SCENE_STACK > 1) {
        memmove(&manager->stack[0], &manager->stack[1],
                sizeof(manager->stack[0]) * (ESPUI_MAX_SCENE_STACK - 1));
        manager->stack[ESPUI_MAX_SCENE_STACK - 1] = scene;
    }
}

void espui_scene_manager_init(espui_scene_manager_t *manager,
                              espui_scene_change_cb_t listener,
                              void *listener_ctx)
{
    if (manager == NULL) {
        return;
    }
    memset(manager, 0, sizeof(*manager));
    manager->current = ESPUI_SCENE_AMBIENT;
    manager->previous = ESPUI_SCENE_INVALID;
    manager->resume_scene = ESPUI_SCENE_AMBIENT;
    manager->listener = listener;
    manager->listener_ctx = listener_ctx;
}

esp_err_t espui_scene_manager_request(espui_scene_manager_t *manager,
                                      espui_scene_id_t scene,
                                      espui_scene_source_t source)
{
    if (manager == NULL || !espui_scene_is_valid(scene)) {
        return ESP_ERR_INVALID_ARG;
    }

    if (source == ESPUI_SCENE_SOURCE_ATTENTION) {
        if (manager->current == ESPUI_SCENE_ATTENTION) {
            return ESP_OK;
        }
        manager->resume_scene = manager->current;
        manager->previous = manager->current;
        manager->current = ESPUI_SCENE_ATTENTION;
        notify(manager, manager->current);
        return ESP_OK;
    }

    if (manager->current == scene) {
        return ESP_OK;
    }

    /* Safety attention is sticky while approvals are pending. */
    if (manager->current == ESPUI_SCENE_ATTENTION &&
        manager->attention_count > 0) {
        return ESP_ERR_INVALID_STATE;
    }

    push_history(manager, manager->current);
    manager->previous = manager->current;
    manager->current = scene;
    notify(manager, scene);
    return ESP_OK;
}

esp_err_t espui_scene_manager_back(espui_scene_manager_t *manager)
{
    if (manager == NULL) {
        return ESP_ERR_INVALID_ARG;
    }

    if (manager->current == ESPUI_SCENE_ATTENTION &&
        manager->attention_count > 0) {
        return ESP_ERR_INVALID_STATE;
    }

    if (manager->depth == 0) {
        if (manager->current == ESPUI_SCENE_AMBIENT) {
            return ESP_OK;
        }
        manager->previous = manager->current;
        manager->current = ESPUI_SCENE_AMBIENT;
        notify(manager, manager->current);
        return ESP_OK;
    }

    espui_scene_id_t target = manager->stack[--manager->depth];
    if (target == manager->current) {
        return ESP_OK;
    }
    manager->previous = manager->current;
    manager->current = target;
    notify(manager, manager->current);
    return ESP_OK;
}

esp_err_t espui_scene_manager_set_attention_count(
    espui_scene_manager_t *manager, uint16_t attention_count)
{
    if (manager == NULL) {
        return ESP_ERR_INVALID_ARG;
    }

    manager->attention_count = attention_count;

    if (attention_count > 0 && manager->current != ESPUI_SCENE_ATTENTION) {
        manager->resume_scene =
            espui_scene_is_valid(manager->current) ? manager->current
                                                   : ESPUI_SCENE_AMBIENT;
        manager->previous = manager->current;
        manager->current = ESPUI_SCENE_ATTENTION;
        notify(manager, manager->current);
    } else if (attention_count == 0 &&
               manager->current == ESPUI_SCENE_ATTENTION) {
        espui_scene_id_t target = espui_scene_is_valid(manager->resume_scene)
                                      ? manager->resume_scene
                                      : ESPUI_SCENE_AMBIENT;
        manager->previous = ESPUI_SCENE_ATTENTION;
        manager->current = target;
        notify(manager, manager->current);
    }

    return ESP_OK;
}

esp_err_t espui_scene_manager_force(espui_scene_manager_t *manager,
                                    espui_scene_id_t scene,
                                    espui_scene_source_t source)
{
    (void)source;
    if (manager == NULL || !espui_scene_is_valid(scene)) {
        return ESP_ERR_INVALID_ARG;
    }
    if (manager->current == scene) {
        return ESP_OK;
    }
    manager->previous = manager->current;
    manager->current = scene;
    notify(manager, manager->current);
    return ESP_OK;
}

espui_scene_id_t espui_scene_manager_current(
    const espui_scene_manager_t *manager)
{
    return manager == NULL ? ESPUI_SCENE_INVALID : manager->current;
}

bool espui_scene_manager_in_attention(const espui_scene_manager_t *manager)
{
    return manager != NULL && manager->current == ESPUI_SCENE_ATTENTION;
}
