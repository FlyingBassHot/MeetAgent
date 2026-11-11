/*
 * MeetAgent Terminal entry point (E0/E1/E2 scaffold).
 *
 * Boot order:
 *   1. board HAL init (Waveshare LCD-4 BSP)
 *   2. ESPUI init (renderer, theme registry, ViewModel, queues)
 *   3. ESPUI UI task
 *   4. mock state task (replaced by networking + state adapter in E4)
 */
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include "espui.h"
#include "espui_board.h"
#include "board_lcd4.h"
#include "mock_state.h"
#include "net_client.h"
#include "phosphor_lvgl_port.h"
#include "wifi_prov.h"
#include "nvs_flash.h"

static const char *TAG = "app_main";
static meet_net_client_t s_net;

static esp_err_t send_action(const espui_action_t *action, void *user_ctx)
{
    return meet_net_client_send_action((meet_net_client_t *)user_ctx, action);
}

static void on_net_state(meet_net_state_t state, void *user_ctx)
{
    (void)state; /* host syntax stubs drop ESP_LOG arguments */
    (void)user_ctx;
    ESP_LOGI(TAG, "network state: %s", meet_net_state_name(state));
}

static void on_action_result(const espui_action_result_t *result,
                             void *user_ctx)
{
    (void)user_ctx;
    if (result == NULL) {
        return;
    }
    ESP_LOGI(TAG, "action result: type=%s status=%d",
             espui_action_type_name(result->action.type), (int)result->status);
    esp_err_t err = espui_post_action_result(result);
    if (err != ESP_OK) {
        ESP_LOGW(TAG, "action result post failed: %s", esp_err_to_name(err));
    }
}

static esp_err_t init_nvs(void)
{
    esp_err_t err = nvs_flash_init();
    if (err == ESP_ERR_NVS_NO_FREE_PAGES ||
        err == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_LOGW(TAG, "NVS needs erase (%s); erasing NVS", esp_err_to_name(err));
        err = nvs_flash_erase();
        if (err == ESP_OK) {
            err = nvs_flash_init();
        }
    }
    return err;
}

static esp_err_t init_board(const espui_board_hal_t *board)
{
    if (board == NULL || board->display == NULL ||
        board->display->init == NULL) {
        return ESP_ERR_INVALID_ARG;
    }

    esp_err_t err = board->display->init();
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "display init failed: %s", esp_err_to_name(err));
        return err;
    }
    if (board->touch != NULL && board->touch->init != NULL) {
        err = board->touch->init();
        if (err != ESP_OK) {
            ESP_LOGW(TAG, "touch init failed: %s", esp_err_to_name(err));
        }
    }
    if (board->input != NULL && board->input->init != NULL) {
        err = board->input->init();
        if (err != ESP_OK) {
            ESP_LOGW(TAG, "input init failed: %s", esp_err_to_name(err));
        }
    }
    if (board->power != NULL && board->power->init != NULL) {
        err = board->power->init();
        if (err != ESP_OK) {
            ESP_LOGW(TAG, "power init failed: %s", esp_err_to_name(err));
        }
    }
    return ESP_OK;
}

void app_main(void)
{
    const espui_board_hal_t *board = espui_board_lcd4();
    esp_err_t err = init_board(board);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "board bring-up failed; stopping");
        return;
    }
    err = init_nvs();
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "NVS init failed: %s", esp_err_to_name(err));
        return;
    }

    meet_net_config_t net_config = {
        .host = NULL, /* Set by provisioning in E4. */
        .port = MEET_PROTOCOL_DEFAULT_PORT,
        .path = MEET_PROTOCOL_WS_PATH,
        .token = NULL,
        .use_tls = false, /* LAN first. */
    };
    meet_net_callbacks_t net_callbacks = {
        .on_state = on_net_state,
        .on_action_result = on_action_result,
    };
    err = meet_net_client_init(&s_net, &net_config, &net_callbacks, NULL);
    if (err != ESP_OK) {
        ESP_LOGW(TAG, "network client init failed: %s", esp_err_to_name(err));
    }

    espui_view_model_t initial_model;
    meet_mock_state_init(&initial_model);

    espui_config_t config = {
        .board = board,
        .initial_model = &initial_model,
        .initial_theme_id = "terminal_aesthetic",
        .action_send = send_action,
        .action_send_ctx = &s_net,
        .ui_task_stack = 8192,
        .ui_task_priority = 5,
        .ui_task_core = -1,
        .tick_ms = 50,
        .event_queue_length = 4,
        .input_queue_length = 16,
        .input_poll_ms = 20,
        .memory_budget_bytes = 64 * 1024,
    };

    err = espui_init(&config);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "ESPUI init failed: %s", esp_err_to_name(err));
        return;
    }
    err = espui_start();
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "ESPUI start failed: %s", esp_err_to_name(err));
        (void)espui_stop();
        return;
    }

    err = phosphor_lvgl_port_init(board->display);
    if (err != ESP_OK) {
        ESP_LOGW(TAG, "PHOSPHOR P1 overlay not started: %s",
                 esp_err_to_name(err));
    }

    err = meet_mock_state_start(3000);
    if (err != ESP_OK) {
        ESP_LOGW(TAG, "mock state not started: %s", esp_err_to_name(err));
    }

    err = meet_wifi_prov_start();
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Wi-Fi provisioning start failed: %s",
                 esp_err_to_name(err));
    }

    err = meet_net_client_start(&s_net);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Node WebSocket client start failed: %s",
                 esp_err_to_name(err));
    }

    ESP_LOGI(TAG, "terminal scaffold running (LCD-4 BSP, mock state)");
    ESP_LOGI(TAG, "Wi-Fi provisioning + Node WebSocket started (E4b-3)");
    vTaskDelay(pdMS_TO_TICKS(10000));
}
