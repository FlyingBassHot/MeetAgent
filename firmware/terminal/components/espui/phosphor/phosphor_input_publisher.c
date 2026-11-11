#include "phosphor_input_publisher.h"

#include <string.h>

#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"

static const char *TAG = "phosphor_pub";

static SemaphoreHandle_t s_lock;
static phosphor_input_state_t s_state;
static bool s_has_state;
static bool s_has_real_snapshot;

void phosphor_input_publisher_init(void)
{
    if (s_lock != NULL) {
        return;
    }
    s_lock = xSemaphoreCreateMutex();
    if (s_lock == NULL) {
        ESP_LOGE(TAG, "publisher mutex allocation failed");
    }
}

bool phosphor_input_publisher_publish(const phosphor_input_state_t *input,
                                      bool from_snapshot)
{
    if (input == NULL || s_lock == NULL ||
        xSemaphoreTake(s_lock, portMAX_DELAY) != pdTRUE) {
        return false;
    }

    phosphor_input_state_t next = *input;
    if (!from_snapshot && next.completed_event_id == 0) {
        /* Keep the pulse visible until the animation task reads it. The
         * resolver dedupes by id, so repeated reads do not retrigger it. */
        next.completed_event_id = s_state.completed_event_id;
    }
    s_state = next;
    s_has_state = true;
    if (from_snapshot) {
        s_has_real_snapshot = true;
    }
    (void)xSemaphoreGive(s_lock);
    return true;
}

bool phosphor_input_publisher_read(phosphor_input_state_t *out,
                                   bool *has_real_snapshot)
{
    if (out == NULL || s_lock == NULL ||
        xSemaphoreTake(s_lock, portMAX_DELAY) != pdTRUE) {
        return false;
    }
    bool has_state = s_has_state;
    if (has_state) {
        *out = s_state;
    }
    if (has_real_snapshot != NULL) {
        *has_real_snapshot = s_has_real_snapshot;
    }
    (void)xSemaphoreGive(s_lock);
    return has_state;
}

void phosphor_input_publisher_clear(void)
{
    if (s_lock == NULL ||
        xSemaphoreTake(s_lock, portMAX_DELAY) != pdTRUE) {
        return;
    }
    memset(&s_state, 0, sizeof(s_state));
    s_has_state = false;
    s_has_real_snapshot = false;
    (void)xSemaphoreGive(s_lock);
}
