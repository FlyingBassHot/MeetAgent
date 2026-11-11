#include "mock_state.h"

#include <string.h>

#include "esp_log.h"
#include "espui.h"
#include "espui_util.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "freertos/task.h"

static const char *TAG = "meet_mock_state";

static espui_view_model_t s_model;
static TaskHandle_t s_task;
static SemaphoreHandle_t s_stopped_sem;
static volatile bool s_stop_requested;
static uint32_t s_interval_ms = 3000;
static uint32_t s_phase;

static void set_node(espui_node_status_t status)
{
    s_model.node.status = status;
    s_model.node.connected = status == ESPUI_NODE_ONLINE ||
                             status == ESPUI_NODE_SYNCING;
    s_model.node.syncing = status == ESPUI_NODE_SYNCING;
    s_model.node.last_sync_ms += s_interval_ms;
}

static void apply_phase(void)
{
    const uint32_t phase = s_phase % 4;

    set_node(ESPUI_NODE_ONLINE);
    espui_agent_view_t *agent =
        espui_view_model_upsert_agent(&s_model, "opencode-main");
    if (agent == NULL) {
        return;
    }
    espui_strlcpy(agent->display_name, "OpenCode Main",
                  sizeof(agent->display_name));

    espui_session_view_t *session =
        espui_view_model_upsert_session(&s_model, "ses-demo");
    if (session == NULL) {
        return;
    }
    espui_strlcpy(session->title, "MeetAgent Terminal", sizeof(session->title));
    session->updated_at_ms += s_interval_ms;

    switch (phase) {
    case 0:
        agent->status = ESPUI_AGENT_BUSY;
        session->status = ESPUI_SESSION_RUNNING;
        session->duration_seconds += 3;
        espui_view_model_resolve_attention(&s_model, "per-demo");
        espui_view_model_resolve_attention(&s_model, "done-demo");
        break;
    case 1:
        agent->status = ESPUI_AGENT_BUSY;
        session->status = ESPUI_SESSION_WAITING_APPROVAL;
        espui_view_model_resolve_attention(&s_model, "done-demo");
        if (espui_view_model_push_attention(&s_model, "per-demo",
                                            ESPUI_ATTENTION_APPROVAL) != NULL) {
            espui_attention_view_t *attention = NULL;
            for (size_t i = 0; i < ESPUI_MAX_ATTENTION; ++i) {
                if (s_model.attention[i].in_use &&
                    strcmp(s_model.attention[i].permission_id, "per-demo") == 0) {
                    attention = &s_model.attention[i];
                    break;
                }
            }
            if (attention != NULL) {
                espui_strlcpy(attention->session_id, "ses-demo",
                              sizeof(attention->session_id));
                espui_strlcpy(attention->summary, "read /home/perch/Work",
                              sizeof(attention->summary));
            }
        }
        break;
    case 2:
        agent->status = ESPUI_AGENT_IDLE;
        session->status = ESPUI_SESSION_COMPLETED;
        espui_view_model_resolve_attention(&s_model, "per-demo");
        if (espui_view_model_push_attention(&s_model, "done-demo",
                                            ESPUI_ATTENTION_COMPLETION) != NULL) {
            for (size_t i = 0; i < ESPUI_MAX_ATTENTION; ++i) {
                if (s_model.attention[i].in_use &&
                    strcmp(s_model.attention[i].permission_id, "done-demo") == 0) {
                    espui_strlcpy(s_model.attention[i].summary,
                                  "Session completed",
                                  sizeof(s_model.attention[i].summary));
                    break;
                }
            }
        }
        break;
    case 3:
    default:
        set_node(ESPUI_NODE_OFFLINE);
        agent->status = ESPUI_AGENT_OFFLINE;
        session->status = ESPUI_SESSION_COMPLETED;
        espui_view_model_resolve_attention(&s_model, "per-demo");
        espui_view_model_resolve_attention(&s_model, "done-demo");
        break;
    }

    agent->active_session_count = espui_view_model_active_session_count(&s_model);
    espui_view_model_touch(&s_model);
}

static void mock_task(void *arg)
{
    (void)arg;
    while (!s_stop_requested) {
        apply_phase();
        if (s_stop_requested) {
            break;
        }
        esp_err_t err = espui_post_view_model(&s_model);
        if (err != ESP_OK) {
            ESP_LOGW(TAG, "post failed: %s", esp_err_to_name(err));
        }
        ++s_phase;

        /* Wake at most every 50 ms so stop() is responsive without deleting
         * the task from another context. */
        TickType_t remaining = pdMS_TO_TICKS(s_interval_ms);
        while (remaining > 0 && !s_stop_requested) {
            TickType_t chunk = remaining > pdMS_TO_TICKS(50)
                                   ? pdMS_TO_TICKS(50)
                                   : remaining;
            vTaskDelay(chunk);
            remaining -= chunk;
        }
    }
    if (s_stopped_sem != NULL) {
        (void)xSemaphoreGive(s_stopped_sem);
    }
    s_task = NULL;
    vTaskDelete(NULL);
}

void meet_mock_state_init(espui_view_model_t *model)
{
    if (model == NULL) {
        return;
    }
    espui_view_model_init(&s_model);
    s_model.node.status = ESPUI_NODE_ONLINE;
    s_model.node.connected = true;
    espui_strlcpy(s_model.node.node_id, "node-demo",
                  sizeof(s_model.node.node_id));
    espui_strlcpy(s_model.node.display_name, "Demo Node",
                  sizeof(s_model.node.display_name));

    espui_view_model_t *target = model;
    *target = s_model;
    s_phase = 0;
    apply_phase();
    *target = s_model;
}

esp_err_t meet_mock_state_start(uint32_t interval_ms)
{
    if (s_task != NULL) {
        return ESP_ERR_INVALID_STATE;
    }
    s_interval_ms = interval_ms > 0 ? interval_ms : 3000;
    s_stop_requested = false;
    if (s_stopped_sem == NULL) {
        s_stopped_sem = xSemaphoreCreateBinary();
        if (s_stopped_sem == NULL) {
            return ESP_ERR_NO_MEM;
        }
    }
    if (xTaskCreatePinnedToCore(mock_task, "meet_mock", 4096, NULL, 3, &s_task,
                                tskNO_AFFINITY) != pdPASS) {
        s_task = NULL;
        vSemaphoreDelete(s_stopped_sem);
        s_stopped_sem = NULL;
        return ESP_ERR_NO_MEM;
    }
    return ESP_OK;
}

esp_err_t meet_mock_state_stop(void)
{
    if (s_task == NULL) {
        return ESP_OK;
    }
    s_stop_requested = true;
    if (s_stopped_sem == NULL ||
        xSemaphoreTake(s_stopped_sem, pdMS_TO_TICKS(1000)) != pdTRUE) {
        ESP_LOGW(TAG, "mock task did not stop within 1s");
        return ESP_ERR_TIMEOUT;
    }
    s_task = NULL;
    vSemaphoreDelete(s_stopped_sem);
    s_stopped_sem = NULL;
    return ESP_OK;
}
