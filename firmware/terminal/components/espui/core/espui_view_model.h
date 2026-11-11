/*
 * ESPUI ViewModel.
 *
 * This is the only state contract between the state adapter (Node data) and
 * themes (visuals). It is fixed-capacity: no heap allocation, no unbounded
 * history, no JSON types.
 *
 * Thread ownership: the UI task owns the active ViewModel. Other tasks must
 * post a copy through espui_post_view_model() and must never mutate it.
 */
#pragma once

#include <stdbool.h>
#include <stdint.h>

#include "espui_types.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    espui_node_status_t status;
    bool connected;
    bool syncing;
    char node_id[ESPUI_ID_MAX];
    char display_name[ESPUI_NAME_MAX];
    uint32_t last_sync_ms;
} espui_node_view_t;

typedef struct {
    bool in_use;
    char agent_id[ESPUI_ID_MAX];
    char display_name[ESPUI_NAME_MAX];
    espui_agent_status_t status;
    uint16_t active_session_count;
} espui_agent_view_t;

typedef struct {
    bool in_use;
    char session_id[ESPUI_ID_MAX];
    char agent_id[ESPUI_ID_MAX];
    char title[ESPUI_TITLE_MAX];
    espui_session_status_t status;
    uint32_t duration_seconds;
    uint32_t updated_at_ms;
} espui_session_view_t;

typedef struct {
    bool in_use;
    char permission_id[ESPUI_ID_MAX];
    char session_id[ESPUI_ID_MAX];
    char summary[ESPUI_TITLE_MAX];
    /* The source summary did not fit in summary[] (or was empty). */
    bool summary_truncated;
    espui_attention_kind_t kind;
    uint32_t created_at_ms;
} espui_attention_view_t;

#define ESPUI_MAX_ACTIVITY 8
#define ESPUI_ACTIVITY_TYPE_MAX 32
#define ESPUI_ACTIVITY_TEXT_MAX 48

typedef struct {
    bool in_use;
    char type[ESPUI_ACTIVITY_TYPE_MAX];
    char session_id[ESPUI_ID_MAX];
    char short_text[ESPUI_ACTIVITY_TEXT_MAX];
    uint32_t timestamp_ms;
} espui_activity_view_t;

typedef struct {
    uint8_t brightness;
    bool night_mode;
    bool motion_enabled;
    uint16_t animation_speed_percent;
    char active_theme_id[ESPUI_ID_MAX];
} espui_device_view_t;

typedef struct {
    uint32_t revision;
    uint32_t uptime_ms;
    espui_node_view_t node;
    espui_agent_view_t agents[ESPUI_MAX_AGENTS];
    espui_session_view_t sessions[ESPUI_MAX_SESSIONS];
    espui_attention_view_t attention[ESPUI_MAX_ATTENTION];
    espui_activity_view_t activity[ESPUI_MAX_ACTIVITY];
    uint8_t activity_count;
    uint16_t approval_count;
    uint16_t error_count;
    uint16_t completion_count;
    /* Transient alerts not represented by an attention queue entry. */
    uint16_t transient_error_count;
    uint16_t transient_completion_count;
    espui_device_view_t device;
} espui_view_model_t;

void espui_view_model_init(espui_view_model_t *model);

/*
 * Clears business state and bumps revision. keep_device preserves device
 * settings (brightness, motion, active theme), which are owned by the user
 * rather than by a Node session.
 */
void espui_view_model_reset(espui_view_model_t *model, bool keep_device);

/* Marks the model changed. Callers normally use the helpers below. */
void espui_view_model_touch(espui_view_model_t *model);

espui_agent_view_t *espui_view_model_upsert_agent(espui_view_model_t *model,
                                                  const char *agent_id);
espui_session_view_t *espui_view_model_upsert_session(
    espui_view_model_t *model, const char *session_id);
espui_attention_view_t *espui_view_model_push_attention(
    espui_view_model_t *model, const char *permission_id,
    espui_attention_kind_t kind);

bool espui_view_model_remove_agent(espui_view_model_t *model,
                                   const char *agent_id);
bool espui_view_model_remove_session(espui_view_model_t *model,
                                     const char *session_id);
bool espui_view_model_resolve_attention(espui_view_model_t *model,
                                        const char *permission_id);

espui_agent_view_t *espui_view_model_find_agent(espui_view_model_t *model,
                                                const char *agent_id);
const espui_agent_view_t *espui_view_model_find_agent_const(
    const espui_view_model_t *model, const char *agent_id);
espui_session_view_t *espui_view_model_find_session(espui_view_model_t *model,
                                                    const char *session_id);
const espui_session_view_t *espui_view_model_find_session_const(
    const espui_view_model_t *model, const char *session_id);

uint16_t espui_view_model_agent_count(const espui_view_model_t *model);
uint16_t espui_view_model_session_count(const espui_view_model_t *model);
uint16_t espui_view_model_active_session_count(
    const espui_view_model_t *model);
uint16_t espui_view_model_attention_count(const espui_view_model_t *model);

/*
 * Safety attention only: pending approvals. Transient errors/completions are
 * deliberately excluded so they cannot force or pin the Attention scene.
 */
uint16_t espui_view_model_safety_attention_count(
    const espui_view_model_t *model);

/* Fixed 8-entry recent-activity ring (oldest first, no allocation). */
void espui_view_model_push_activity(espui_view_model_t *model,
                                    const char *type,
                                    const char *session_id,
                                    const char *short_text,
                                    uint32_t timestamp_ms);
uint8_t espui_view_model_activity_count(const espui_view_model_t *model);
const espui_activity_view_t *espui_view_model_activity_at(
    const espui_view_model_t *model, uint8_t index);

/* Recomputes attention counters after a bulk mutation. */
void espui_view_model_recount_attention(espui_view_model_t *model);

/* Transient alert counters for tool.failed / message.completed (capped). */
void espui_view_model_note_error(espui_view_model_t *model);
void espui_view_model_note_completion(espui_view_model_t *model);

/* Fixed-size copy. */
void espui_view_model_copy(espui_view_model_t *dst,
                           const espui_view_model_t *src);

/*
 * Copies business state (node/agents/sessions/attention) but keeps the
 * destination device settings. Bumps the destination revision. Used by the
 * UI task when a state-adapter task posts a fresh ViewModel.
 */
void espui_view_model_copy_business(espui_view_model_t *dst,
                                    const espui_view_model_t *src);

#ifdef __cplusplus
}
#endif
