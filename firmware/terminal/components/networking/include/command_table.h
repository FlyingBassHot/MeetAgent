/*
 * Fixed-capacity pending Node command table.
 *
 * Pure logic: no allocation, no FreeRTOS, no cJSON. The networking task owns
 * the mutex around this table; this module only manipulates the entries.
 */
#pragma once

#include <stdint.h>

#include "esp_err.h"
#include "espui_types.h"

#ifdef __cplusplus
extern "C" {
#endif

#define MEET_PENDING_COMMAND_MAX 4
#define MEET_PENDING_COMMAND_ID_MAX 64

typedef struct {
    char command_id[MEET_PENDING_COMMAND_ID_MAX];
    espui_action_t action;
    uint32_t sent_ms;
} meet_pending_command_t;

typedef struct {
    meet_pending_command_t entries[MEET_PENDING_COMMAND_MAX];
    uint8_t count;
} meet_pending_commands_t;

void meet_pending_commands_init(meet_pending_commands_t *table);
uint8_t meet_pending_commands_count(const meet_pending_commands_t *table);

/* Duplicate command IDs are rejected before the capacity check. */
esp_err_t meet_pending_commands_add(meet_pending_commands_t *table,
                                    const char *command_id,
                                    const espui_action_t *action,
                                    uint32_t sent_ms);

esp_err_t meet_pending_commands_take(meet_pending_commands_t *table,
                                     const char *command_id,
                                     meet_pending_command_t *out);

/* Removes and returns entries whose age is >= timeout_ms (wrap-safe). */
uint8_t meet_pending_commands_take_expired(
    meet_pending_commands_t *table, uint32_t now_ms, uint32_t timeout_ms,
    meet_pending_command_t *out, uint8_t out_capacity);

/* Removes up to out_capacity entries, preserving their table order. */
uint8_t meet_pending_commands_drain(meet_pending_commands_t *table,
                                    meet_pending_command_t *out,
                                    uint8_t out_capacity);

#ifdef __cplusplus
}
#endif
