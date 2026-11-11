#include "espui.h"

#include <string.h>

#include "animation_scheduler.h"
#include "asset_manager.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "espui_event.h"
#include "espui_renderer.h"
#include "input_gesture.h"
#include "input_router.h"
#include "navigation.h"
#include "espui_scene.h"
#include "espui_theme.h"
#include "espui_util.h"
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "freertos/semphr.h"
#include "freertos/task.h"
#include "memory_manager.h"

#define TAG "espui"

#define ESPUI_DEFAULT_STACK 8192
#define ESPUI_DEFAULT_PRIORITY 5
#define ESPUI_DEFAULT_TICK_MS 50
#define ESPUI_DEFAULT_QUEUE_LENGTH 4
#define ESPUI_DEFAULT_INPUT_QUEUE_LENGTH 16
#define ESPUI_DEFAULT_INPUT_POLL_MS 20
#define ESPUI_DEFAULT_INPUT_STACK 4096
#define ESPUI_DEFAULT_INPUT_PRIORITY 4
#define ESPUI_DEFAULT_MEMORY_BUDGET (64u * 1024u)

struct espui_context {
    espui_config_t config;
    espui_board_hal_t board;
    const espui_display_info_t *display;

    espui_view_model_t model;
    espui_theme_manager_t theme_manager;
    espui_scene_manager_t scenes;
    espui_input_router_t input_router;
    espui_action_dispatcher_t actions;
    espui_animation_scheduler_t animations;
    espui_asset_registry_t assets;
    espui_memory_budget_t memory;

    QueueHandle_t events;
    QueueHandle_t inputs;
    SemaphoreHandle_t stopped_sem;
    SemaphoreHandle_t input_stopped_sem;
    TaskHandle_t ui_task;
    TaskHandle_t input_task;

    int64_t started_us;
    bool initialized;
    bool started;
    volatile bool running;
    bool renderer_ready;
};

static espui_context_t s_ctx;

static void display_lock(espui_context_t *ctx)
{
    if (ctx->board.display != NULL && ctx->board.display->lock != NULL) {
        ctx->board.display->lock(1000);
    }
}

static void display_unlock(espui_context_t *ctx)
{
    if (ctx->board.display != NULL && ctx->board.display->unlock != NULL) {
        ctx->board.display->unlock();
    }
}

static void on_scene_changed(espui_scene_id_t scene, void *user_ctx)
{
    espui_context_t *ctx = (espui_context_t *)user_ctx;
    if (ctx == NULL) {
        return;
    }
    espui_input_router_set_scene(&ctx->input_router, scene);

    const espui_theme_descriptor_t *active =
        espui_theme_manager_active(&ctx->theme_manager);
    if (active != NULL && active->ops->enter_scene != NULL) {
        esp_err_t err = active->ops->enter_scene(scene, &ctx->model);
        if (err != ESP_OK) {
            ESP_LOGW(TAG, "enter_scene(%s) failed: %s", espui_scene_name(scene),
                     esp_err_to_name(err));
        }
    }
}

static esp_err_t apply_theme_locked(espui_context_t *ctx, const char *theme_id)
{
    espui_scene_id_t scene = espui_scene_manager_current(&ctx->scenes);
    esp_err_t err = espui_theme_manager_apply(&ctx->theme_manager, theme_id,
                                              scene, &ctx->model);
    const espui_theme_descriptor_t *active =
        espui_theme_manager_active(&ctx->theme_manager);
    if (active != NULL && active->manifest != NULL) {
        espui_strlcpy(ctx->model.device.active_theme_id,
                      active->manifest->theme_id,
                      sizeof(ctx->model.device.active_theme_id));
    }
    return err;
}

static void normalize_config(espui_config_t *config)
{
    if (config->ui_task_stack == 0) {
        config->ui_task_stack = ESPUI_DEFAULT_STACK;
    }
    if (config->ui_task_priority == 0) {
        config->ui_task_priority = ESPUI_DEFAULT_PRIORITY;
    }
    if (config->tick_ms == 0) {
        config->tick_ms = ESPUI_DEFAULT_TICK_MS;
    }
    if (config->event_queue_length == 0) {
        config->event_queue_length = ESPUI_DEFAULT_QUEUE_LENGTH;
    }
    if (config->input_queue_length == 0) {
        config->input_queue_length = ESPUI_DEFAULT_INPUT_QUEUE_LENGTH;
    }
    if (config->input_poll_ms == 0) {
        config->input_poll_ms = ESPUI_DEFAULT_INPUT_POLL_MS;
    }
    if (config->input_task_stack == 0) {
        config->input_task_stack = ESPUI_DEFAULT_INPUT_STACK;
    }
    if (config->input_task_priority == 0) {
        config->input_task_priority = ESPUI_DEFAULT_INPUT_PRIORITY;
    }
    if (config->memory_budget_bytes == 0) {
        config->memory_budget_bytes = ESPUI_DEFAULT_MEMORY_BUDGET;
    }
}

static void cleanup_runtime(espui_context_t *ctx)
{
    if (ctx->events != NULL) {
        vQueueDelete(ctx->events);
        ctx->events = NULL;
    }
    if (ctx->inputs != NULL) {
        vQueueDelete(ctx->inputs);
        ctx->inputs = NULL;
    }
    if (ctx->input_stopped_sem != NULL) {
        vSemaphoreDelete(ctx->input_stopped_sem);
        ctx->input_stopped_sem = NULL;
    }
    if (ctx->stopped_sem != NULL) {
        vSemaphoreDelete(ctx->stopped_sem);
        ctx->stopped_sem = NULL;
    }
    if (ctx->renderer_ready) {
        const espui_renderer_t *renderer = espui_renderer_builtin_label();
        if (renderer != NULL && renderer->ops->deinit != NULL) {
            renderer->ops->deinit();
        }
        ctx->renderer_ready = false;
    }
}

static esp_err_t post_input_event(const espui_input_event_t *event)
{
    if (!s_ctx.initialized || s_ctx.inputs == NULL || event == NULL) {
        return ESP_ERR_INVALID_STATE;
    }
    if (xQueueSend(s_ctx.inputs, event, 0) == pdTRUE) {
        return ESP_OK;
    }
    /* Raw TOUCH press/release is lower priority than discrete gestures. */
    if (event->type == ESPUI_INPUT_TOUCH) {
        return ESP_ERR_TIMEOUT;
    }
    if (xQueueSend(s_ctx.inputs, event, pdMS_TO_TICKS(20)) != pdTRUE) {
        return ESP_ERR_TIMEOUT;
    }
    return ESP_OK;
}

static void input_task(void *arg)
{
    espui_context_t *ctx = (espui_context_t *)arg;
    espui_gesture_state_t gesture;
    espui_gesture_init(&gesture);
    uint32_t poll_ms = ctx->config.input_poll_ms;

    while (ctx->running) {
        bool touched = false;
        uint16_t x = gesture.last_x;
        uint16_t y = gesture.last_y;

        const espui_touch_hal_t *touch = ctx->board.touch;
        const espui_display_hal_t *display = ctx->board.display;
        if (touch != NULL && touch->read != NULL) {
            espui_touch_point_t point;
            size_t count = 0;
            bool lock_ok = true;
            if (display != NULL && display->lock != NULL) {
                lock_ok = display->lock(1000) == ESP_OK;
            }
            if (lock_ok) {
                esp_err_t err = touch->read(&point, 1, &count);
                if (display != NULL && display->unlock != NULL) {
                    display->unlock();
                }
                if (err == ESP_OK && count > 0 && point.pressed) {
                    touched = true;
                    x = point.x;
                    y = point.y;
                }
            }
        }

        uint32_t timestamp_ms = espui_uptime_ms(ctx);
        espui_input_event_t events[ESPUI_GESTURE_MAX_EVENTS];
        size_t event_count = espui_gesture_feed(
            &gesture, touched, x, y, timestamp_ms, events,
            ESPUI_GESTURE_MAX_EVENTS);
        for (size_t i = 0; i < event_count; ++i) {
            (void)espui_post_input(&events[i]);
        }

        const espui_input_hal_t *input = ctx->board.input;
        if (input != NULL && input->read_event != NULL) {
            for (int i = 0; i < 4; ++i) {
                espui_input_event_t boot;
                if (input->read_event(&boot, 0) != ESP_OK) break;
                (void)espui_post_input(&boot);
            }
        }

        vTaskDelay(pdMS_TO_TICKS(poll_ms));
    }
    if (ctx->input_stopped_sem != NULL) {
        xSemaphoreGive(ctx->input_stopped_sem);
    }
    ctx->input_task = NULL;
    vTaskDelete(NULL);
}

static void handle_view_model(espui_context_t *ctx,
                              const espui_view_model_t *incoming)
{
    espui_view_model_copy_business(&ctx->model, incoming);
    ctx->model.uptime_ms = espui_uptime_ms(ctx);

    uint16_t attention = espui_view_model_attention_count(&ctx->model);
    espui_input_router_set_attention_count(&ctx->input_router, attention);
    espui_scene_manager_set_attention_count(&ctx->scenes, attention);

    const espui_theme_descriptor_t *active =
        espui_theme_manager_active(&ctx->theme_manager);
    if (active != NULL) {
        active->ops->update(&ctx->model);
    }
}

static void handle_input(espui_context_t *ctx, const espui_input_event_t *event)
{
    espui_nav_intent_t intent = espui_input_router_feed(&ctx->input_router,
                                                        event);
    esp_err_t err = espui_navigation_apply(intent, &ctx->scenes);
    if (err != ESP_OK && err != ESP_ERR_INVALID_STATE) {
        ESP_LOGW(TAG, "navigation %s failed: %s", espui_nav_intent_name(intent),
                 esp_err_to_name(err));
    }
    /* Navigation may be rejected by sticky Attention; keep router scene in
     * sync with the authoritative scene manager. */
    espui_input_router_set_scene(&ctx->input_router,
                                 espui_scene_manager_current(&ctx->scenes));

    const espui_theme_descriptor_t *active =
        espui_theme_manager_active(&ctx->theme_manager);
    if (active != NULL && active->ops->on_input != NULL) {
        active->ops->on_input(event);
    }
    ESP_LOGI(TAG, "input type=%s x=%u y=%u scene=%s",
             espui_input_type_name(event->type), (unsigned)event->x,
             (unsigned)event->y,
             espui_scene_name(espui_scene_manager_current(&ctx->scenes)));
}

static void handle_event(espui_context_t *ctx, const espui_event_t *event)
{
    switch (event->type) {
    case ESPUI_EVENT_VIEW_MODEL:
        display_lock(ctx);
        handle_view_model(ctx, &event->data.view_model);
        display_unlock(ctx);
        break;

    case ESPUI_EVENT_INPUT:
        display_lock(ctx);
        handle_input(ctx, &event->data.input);
        display_unlock(ctx);
        break;

    case ESPUI_EVENT_ACTION_RESULT: {
        display_lock(ctx);
        const espui_theme_descriptor_t *active =
            espui_theme_manager_active(&ctx->theme_manager);
        if (active != NULL && active->ops->on_action_result != NULL) {
            active->ops->on_action_result(&event->data.action_result);
        }
        display_unlock(ctx);
        break;
    }

    case ESPUI_EVENT_SCENE:
        display_lock(ctx);
        espui_scene_manager_request(&ctx->scenes, event->data.scene,
                                    ESPUI_SCENE_SOURCE_USER);
        display_unlock(ctx);
        break;

    case ESPUI_EVENT_THEME: {
        display_lock(ctx);
        esp_err_t err = apply_theme_locked(ctx, event->data.theme_id);
        if (err != ESP_OK) {
            ESP_LOGW(TAG, "theme switch to %s: %s", event->data.theme_id,
                     esp_err_to_name(err));
        }
        display_unlock(ctx);
        break;
    }

    case ESPUI_EVENT_STOP:
        ctx->running = false;
        break;

    case ESPUI_EVENT_NONE:
    default:
        break;
    }
}

static void ui_task(void *arg)
{
    espui_context_t *ctx = (espui_context_t *)arg;
    TickType_t last_ticks = xTaskGetTickCount();

    while (ctx->running) {
        espui_input_event_t input;
        while (xQueueReceive(ctx->inputs, &input, 0) == pdTRUE) {
            display_lock(ctx);
            handle_input(ctx, &input);
            display_unlock(ctx);
        }
        espui_event_t event;
        if (xQueueReceive(ctx->events, &event,
                          pdMS_TO_TICKS(ctx->config.tick_ms)) == pdTRUE) {
            handle_event(ctx, &event);
        }

        TickType_t now_ticks = xTaskGetTickCount();
        uint32_t delta_ms = pdTICKS_TO_MS(now_ticks - last_ticks);
        last_ticks = now_ticks;
        if (delta_ms == 0) {
            continue;
        }

        display_lock(ctx);
        espui_animation_scheduler_tick(&ctx->animations, delta_ms);
        const espui_theme_descriptor_t *active =
            espui_theme_manager_active(&ctx->theme_manager);
        if (active != NULL && active->ops->on_tick != NULL) {
            active->ops->on_tick(delta_ms);
        }
        display_unlock(ctx);
    }

    if (ctx->stopped_sem != NULL) {
        xSemaphoreGive(ctx->stopped_sem);
    }
    vTaskDelete(NULL);
}

esp_err_t espui_init(const espui_config_t *config)
{
    if (config == NULL || config->board == NULL) {
        return ESP_ERR_INVALID_ARG;
    }
    if (s_ctx.initialized) {
        return ESP_ERR_INVALID_STATE;
    }
    if (config->board->display == NULL ||
        config->board->display->get_info == NULL) {
        return ESP_ERR_INVALID_ARG;
    }

    memset(&s_ctx, 0, sizeof(s_ctx));
    s_ctx.config = *config;
    normalize_config(&s_ctx.config);
    s_ctx.board = *config->board;
    s_ctx.display = s_ctx.board.display->get_info();
    if (s_ctx.display == NULL || s_ctx.display->width == 0 ||
        s_ctx.display->height == 0) {
        return ESP_ERR_INVALID_ARG;
    }

    espui_renderer_registry_reset();
    const espui_renderer_t *renderer = espui_renderer_builtin_label();
    if (renderer == NULL || renderer->ops->init == NULL) {
        return ESP_ERR_NOT_SUPPORTED;
    }
    esp_err_t err = espui_renderer_registry_add(renderer);
    if (err != ESP_OK) {
        return err;
    }
    err = renderer->ops->init();
    if (err != ESP_OK) {
        return err;
    }
    s_ctx.renderer_ready = true;

    espui_theme_registry_reset();
    espui_register_builtin_themes();

    espui_view_model_init(&s_ctx.model);
    if (config->initial_model != NULL) {
        espui_view_model_copy(&s_ctx.model, config->initial_model);
    }
    s_ctx.model.uptime_ms = 0;

    espui_scene_manager_init(&s_ctx.scenes, on_scene_changed, &s_ctx);
    espui_input_router_init(&s_ctx.input_router);
    espui_action_dispatcher_init(&s_ctx.actions, config->action_send,
                                 config->action_send_ctx);
    espui_animation_scheduler_init(&s_ctx.animations);
    espui_asset_registry_init(&s_ctx.assets);
    espui_memory_budget_init(&s_ctx.memory, s_ctx.config.memory_budget_bytes);

    espui_theme_manager_init(&s_ctx.theme_manager, &s_ctx,
                             espui_theme_safe(), s_ctx.display);

    s_ctx.events = xQueueCreate(s_ctx.config.event_queue_length,
                                sizeof(espui_event_t));
    s_ctx.inputs = xQueueCreate(s_ctx.config.input_queue_length,
                                sizeof(espui_input_event_t));
    s_ctx.stopped_sem = xSemaphoreCreateBinary();
    s_ctx.input_stopped_sem = xSemaphoreCreateBinary();
    if (s_ctx.events == NULL || s_ctx.inputs == NULL ||
        s_ctx.stopped_sem == NULL || s_ctx.input_stopped_sem == NULL) {
        cleanup_runtime(&s_ctx);
        return ESP_ERR_NO_MEM;
    }

    s_ctx.initialized = true;
    s_ctx.started_us = esp_timer_get_time();

    const char *requested_theme = config->initial_theme_id;
    if (espui_str_empty(requested_theme)) {
        requested_theme = s_ctx.model.device.active_theme_id;
    }
    if (espui_str_empty(requested_theme)) {
        requested_theme = "safe_fallback";
    }
    char initial_theme[ESPUI_ID_MAX];
    espui_strlcpy(initial_theme, requested_theme, sizeof(initial_theme));

    display_lock(&s_ctx);
    err = apply_theme_locked(&s_ctx, initial_theme);
    const espui_theme_descriptor_t *active =
        espui_theme_manager_active(&s_ctx.theme_manager);
    display_unlock(&s_ctx);

    if (active == NULL) {
        ESP_LOGE(TAG, "no usable theme: %s", esp_err_to_name(err));
        s_ctx.initialized = false;
        cleanup_runtime(&s_ctx);
        return err != ESP_OK ? err : ESPUI_ERR_THEME_INIT;
    }
    if (err != ESP_OK) {
        ESP_LOGW(TAG, "initial theme %s failed (%s); running %s", initial_theme,
                 esp_err_to_name(err), active->manifest->theme_id);
    }

    ESP_LOGI(TAG, "ESPUI initialized: theme=%s display=%s %ux%u",
             active->manifest->theme_id,
             s_ctx.display->panel_name != NULL ? s_ctx.display->panel_name
                                               : "unknown",
             (unsigned)s_ctx.display->width, (unsigned)s_ctx.display->height);
    return ESP_OK;
}

esp_err_t espui_start(void)
{
    if (!s_ctx.initialized) {
        return ESP_ERR_INVALID_STATE;
    }
    if (s_ctx.started) {
        return ESP_ERR_INVALID_STATE;
    }

    s_ctx.running = true;
    s_ctx.started_us = esp_timer_get_time();

    BaseType_t core = s_ctx.config.ui_task_core < 0 ? tskNO_AFFINITY
                                                    : s_ctx.config.ui_task_core;
    if (xTaskCreatePinnedToCore(ui_task, "espui_ui", s_ctx.config.ui_task_stack,
                                &s_ctx, s_ctx.config.ui_task_priority,
                                &s_ctx.ui_task, core) != pdPASS) {
        s_ctx.running = false;
        s_ctx.ui_task = NULL;
        return ESP_ERR_NO_MEM;
    }

    if (xTaskCreatePinnedToCore(input_task, "espui_input",
                                s_ctx.config.input_task_stack, &s_ctx,
                                s_ctx.config.input_task_priority,
                                &s_ctx.input_task, core) != pdPASS) {
        s_ctx.running = false;
        espui_event_t event = {.type = ESPUI_EVENT_STOP};
        (void)xQueueSend(s_ctx.events, &event, 0);
        (void)xSemaphoreTake(s_ctx.stopped_sem, pdMS_TO_TICKS(1000));
        s_ctx.ui_task = NULL;
        s_ctx.started = false;
        return ESP_ERR_NO_MEM;
    }

    s_ctx.started = true;
    return ESP_OK;
}

esp_err_t espui_stop(void)
{
    if (!s_ctx.initialized) {
        return ESP_OK;
    }
    if (s_ctx.ui_task != NULL &&
        xTaskGetCurrentTaskHandle() == s_ctx.ui_task) {
        return ESP_ERR_INVALID_STATE;
    }

    if (s_ctx.started) {
        s_ctx.running = false;
        espui_event_t event = {.type = ESPUI_EVENT_STOP};
        (void)xQueueSend(s_ctx.events, &event, 0);
        if (s_ctx.stopped_sem != NULL &&
            xSemaphoreTake(s_ctx.stopped_sem, pdMS_TO_TICKS(1000)) != pdTRUE) {
            ESP_LOGE(TAG, "UI task did not stop in time");
            return ESP_ERR_TIMEOUT;
        }
        s_ctx.started = false;
        s_ctx.ui_task = NULL;
    }
    if (s_ctx.input_task != NULL &&
        xTaskGetCurrentTaskHandle() != s_ctx.input_task) {
        if (s_ctx.input_stopped_sem != NULL) {
            (void)xSemaphoreTake(s_ctx.input_stopped_sem, pdMS_TO_TICKS(1000));
        }
        s_ctx.input_task = NULL;
    }

    display_lock(&s_ctx);
    const espui_theme_descriptor_t *active =
        espui_theme_manager_active(&s_ctx.theme_manager);
    if (active != NULL && active->ops->destroy != NULL) {
        active->ops->destroy();
    }
    s_ctx.theme_manager.active = NULL;
    s_ctx.theme_manager.renderer = NULL;
    display_unlock(&s_ctx);

    cleanup_runtime(&s_ctx);
    s_ctx.initialized = false;
    return ESP_OK;
}

bool espui_initialized(void)
{
    return s_ctx.initialized;
}

bool espui_started(void)
{
    return s_ctx.started;
}

const espui_context_t *espui_context(void)
{
    return s_ctx.initialized ? &s_ctx : NULL;
}

static esp_err_t post_event(const espui_event_t *event)
{
    if (!s_ctx.initialized || s_ctx.events == NULL) {
        return ESP_ERR_INVALID_STATE;
    }
    if (xQueueSend(s_ctx.events, event, 0) != pdTRUE) {
        return ESP_ERR_TIMEOUT;
    }
    return ESP_OK;
}

esp_err_t espui_post_view_model(const espui_view_model_t *model)
{
    if (model == NULL) {
        return ESP_ERR_INVALID_ARG;
    }
    espui_event_t event = {.type = ESPUI_EVENT_VIEW_MODEL};
    event.data.view_model = *model;
    return post_event(&event);
}

esp_err_t espui_post_input(const espui_input_event_t *event)
{
    if (event == NULL) {
        return ESP_ERR_INVALID_ARG;
    }
    return post_input_event(event);
}

esp_err_t espui_post_action_result(const espui_action_result_t *result)
{
    if (result == NULL) {
        return ESP_ERR_INVALID_ARG;
    }
    espui_event_t event = {.type = ESPUI_EVENT_ACTION_RESULT};
    event.data.action_result = *result;
    return post_event(&event);
}

esp_err_t espui_post_scene(espui_scene_id_t scene)
{
    if (!espui_scene_is_valid(scene)) {
        return ESP_ERR_INVALID_ARG;
    }
    espui_event_t event = {.type = ESPUI_EVENT_SCENE};
    event.data.scene = scene;
    return post_event(&event);
}

esp_err_t espui_post_theme(const char *theme_id)
{
    if (espui_str_empty(theme_id) ||
        strlen(theme_id) >= ESPUI_ID_MAX) {
        return ESP_ERR_INVALID_ARG;
    }
    espui_event_t event = {.type = ESPUI_EVENT_THEME};
    espui_strlcpy(event.data.theme_id, theme_id,
                  sizeof(event.data.theme_id));
    return post_event(&event);
}

const espui_view_model_t *espui_view_model(const espui_context_t *ctx)
{
    return ctx == NULL ? NULL : &ctx->model;
}

const espui_renderer_ops_t *espui_renderer(const espui_context_t *ctx)
{
    if (ctx == NULL) {
        return NULL;
    }
    const espui_renderer_t *renderer =
        espui_theme_manager_renderer(&ctx->theme_manager);
    return renderer != NULL ? renderer->ops : NULL;
}

const espui_tokens_t *espui_tokens(const espui_context_t *ctx)
{
    const espui_tokens_t *tokens =
        ctx == NULL ? NULL : espui_theme_manager_tokens(&ctx->theme_manager);
    return tokens != NULL ? tokens : espui_tokens_safe_default();
}

const espui_motion_profile_t *espui_motion_profile(const espui_context_t *ctx)
{
    if (ctx == NULL) {
        return NULL;
    }
    const espui_theme_descriptor_t *active =
        espui_theme_manager_active(&ctx->theme_manager);
    if (active == NULL || active->manifest == NULL) {
        return espui_motion_profile_default();
    }
    const espui_motion_profile_t *profile =
        espui_motion_profile_find(active->manifest->motion_profile_id);
    return profile != NULL ? profile : espui_motion_profile_default();
}

const espui_display_info_t *espui_display(const espui_context_t *ctx)
{
    return ctx == NULL ? NULL : ctx->display;
}

espui_scene_id_t espui_current_scene(const espui_context_t *ctx)
{
    return ctx == NULL ? ESPUI_SCENE_INVALID
                       : espui_scene_manager_current(&ctx->scenes);
}

esp_err_t espui_request_scene(const espui_context_t *ctx,
                              espui_scene_id_t scene)
{
    if (ctx == NULL || !ctx->initialized) {
        return ESP_ERR_INVALID_STATE;
    }
    if (!espui_scene_is_valid(scene)) {
        return ESP_ERR_INVALID_ARG;
    }
    if (scene == espui_scene_manager_current(&ctx->scenes)) {
        return ESP_OK;
    }
    if (ctx->ui_task != NULL &&
        xTaskGetCurrentTaskHandle() == ctx->ui_task) {
        return espui_scene_manager_request(
            (espui_scene_manager_t *)&ctx->scenes, scene,
            ESPUI_SCENE_SOURCE_USER);
    }
    return espui_post_scene(scene);
}

esp_err_t espui_dispatch_action(const espui_context_t *ctx,
                                const espui_action_t *action)
{
    if (ctx == NULL) {
        return ESP_ERR_INVALID_STATE;
    }
    return espui_action_dispatcher_dispatch(
        (espui_action_dispatcher_t *)&ctx->actions, action);
}

bool espui_motion_enabled(const espui_context_t *ctx)
{
    return ctx != NULL && ctx->model.device.motion_enabled;
}

uint16_t espui_focus_index(const espui_context_t *ctx)
{
    return ctx != NULL ? espui_input_router_focus_index(&ctx->input_router)
                       : 0;
}

uint32_t espui_uptime_ms(const espui_context_t *ctx)
{
    if (ctx == NULL) {
        return 0;
    }
    int64_t now = esp_timer_get_time();
    if (ctx->started_us <= 0 || now <= ctx->started_us) {
        return (uint32_t)(now / 1000);
    }
    return (uint32_t)((now - ctx->started_us) / 1000);
}
