#include "command_table.h"

#include <string.h>

static void copy_command_id(char *dst, size_t size, const char *src)
{
    if (dst == NULL || size == 0) {
        return;
    }
    if (src == NULL) {
        dst[0] = '\0';
        return;
    }
    size_t length = strlen(src);
    if (length >= size) {
        length = size - 1;
    }
    memcpy(dst, src, length);
    dst[length] = '\0';
}

void meet_pending_commands_init(meet_pending_commands_t *table)
{
    if (table != NULL) {
        memset(table, 0, sizeof(*table));
    }
}

uint8_t meet_pending_commands_count(const meet_pending_commands_t *table)
{
    return table != NULL ? table->count : 0;
}

static int find_command(const meet_pending_commands_t *table,
                        const char *command_id)
{
    for (uint8_t i = 0; i < table->count; ++i) {
        if (strcmp(table->entries[i].command_id, command_id) == 0) {
            return (int)i;
        }
    }
    return -1;
}

esp_err_t meet_pending_commands_add(meet_pending_commands_t *table,
                                    const char *command_id,
                                    const espui_action_t *action,
                                    uint32_t sent_ms)
{
    if (table == NULL || command_id == NULL || command_id[0] == '\0' ||
        action == NULL || action->type == ESPUI_ACTION_NONE) {
        return ESP_ERR_INVALID_ARG;
    }
    if (find_command(table, command_id) >= 0) {
        return ESP_ERR_INVALID_STATE;
    }
    if (table->count >= MEET_PENDING_COMMAND_MAX) {
        return ESP_ERR_NO_MEM;
    }

    meet_pending_command_t *entry = &table->entries[table->count++];
    memset(entry, 0, sizeof(*entry));
    copy_command_id(entry->command_id, sizeof(entry->command_id), command_id);
    entry->action = *action;
    entry->sent_ms = sent_ms;
    return ESP_OK;
}

esp_err_t meet_pending_commands_take(meet_pending_commands_t *table,
                                     const char *command_id,
                                     meet_pending_command_t *out)
{
    if (table == NULL || command_id == NULL || command_id[0] == '\0' ||
        out == NULL) {
        return ESP_ERR_INVALID_ARG;
    }
    int index = find_command(table, command_id);
    if (index < 0) {
        return ESP_ERR_NOT_FOUND;
    }
    *out = table->entries[index];
    table->entries[index] = table->entries[--table->count];
    return ESP_OK;
}

uint8_t meet_pending_commands_take_expired(
    meet_pending_commands_t *table, uint32_t now_ms, uint32_t timeout_ms,
    meet_pending_command_t *out, uint8_t out_capacity)
{
    if (table == NULL || out == NULL || out_capacity == 0) {
        return 0;
    }
    uint8_t produced = 0;
    uint8_t index = 0;
    while (index < table->count && produced < out_capacity) {
        meet_pending_command_t *entry = &table->entries[index];
        if ((uint32_t)(now_ms - entry->sent_ms) < timeout_ms) {
            ++index;
            continue;
        }
        out[produced++] = *entry;
        table->entries[index] = table->entries[--table->count];
    }
    return produced;
}

uint8_t meet_pending_commands_drain(meet_pending_commands_t *table,
                                    meet_pending_command_t *out,
                                    uint8_t out_capacity)
{
    if (table == NULL || out == NULL || out_capacity == 0) {
        return 0;
    }
    uint8_t produced = table->count < out_capacity ? table->count
                                                   : out_capacity;
    for (uint8_t i = 0; i < produced; ++i) {
        out[i] = table->entries[i];
    }
    for (uint8_t i = produced; i < table->count; ++i) {
        table->entries[i - produced] = table->entries[i];
    }
    table->count = (uint8_t)(table->count - produced);
    return produced;
}
