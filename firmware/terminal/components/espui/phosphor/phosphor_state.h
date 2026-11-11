#pragma once

#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define PHOSPHOR_MAX_SESSIONS 8
#define PHOSPHOR_MAX_SATELLITES 4
#define PHOSPHOR_COMPLETION_MS 900u
#define PHOSPHOR_ERROR_GLITCH_MS 220u
#define PHOSPHOR_APPROVAL_PERIOD_MS 1750u

typedef enum {
    PHOSPHOR_SESSION_IDLE = 0,
    PHOSPHOR_SESSION_RUNNING,
    PHOSPHOR_SESSION_WAITING,
    PHOSPHOR_SESSION_COMPLETED,
    PHOSPHOR_SESSION_ERROR,
} phosphor_session_status_t;

typedef struct {
    char session_id[40];
    phosphor_session_status_t status;
} phosphor_session_input_t;

typedef struct {
    bool node_online;
    uint8_t session_count;
    phosphor_session_input_t sessions[PHOSPHOR_MAX_SESSIONS];
    uint8_t approval_count;
    uint8_t error_count;
    uint64_t completed_event_id; /* 0 = no new event */
    uint64_t error_event_id;     /* 0 = no new event */
} phosphor_input_state_t;

typedef enum {
    PHOSPHOR_ACTIVITY_IDLE = 0,
    PHOSPHOR_ACTIVITY_WORKING,
    PHOSPHOR_ACTIVITY_OFFLINE,
} phosphor_activity_t;

#define PHOSPHOR_ATTENTION_APPROVAL (1u << 0)
#define PHOSPHOR_ATTENTION_ERROR (1u << 1)

typedef struct {
    uint8_t slot;
    char session_id[40];
    phosphor_session_status_t status;
} phosphor_satellite_state_t;

typedef struct {
    phosphor_activity_t activity;
    uint8_t attention;
    uint8_t running_count;
    uint8_t waiting_count;
    uint8_t error_count;
    uint8_t completed_count;
    uint8_t satellite_count;
    uint8_t overflow_count;
    phosphor_satellite_state_t satellites[PHOSPHOR_MAX_SATELLITES];
    bool completion_active;
    uint32_t completion_elapsed_ms;
    uint64_t completion_event_id;
    bool error_glitch_active;
    uint32_t error_glitch_elapsed_ms;
    uint32_t approval_elapsed_ms;
    uint32_t error_elapsed_ms;
    float approval_breath;
} phosphor_visual_state_t;

typedef struct {
    uint64_t last_completed_event_id;
    uint64_t completion_start_ms;
    bool completion_active;
    uint64_t last_error_event_id;
    uint32_t error_start_ms;
    bool error_glitch_active;
    uint32_t approval_start_ms;
    uint32_t error_attention_start_ms;
    bool approval_attention_seen;
    bool error_attention_seen;
    char slot_ids[PHOSPHOR_MAX_SATELLITES][40];
} phosphor_state_resolver_t;

void phosphor_state_resolver_init(phosphor_state_resolver_t *resolver);
void phosphor_state_resolver_update(phosphor_state_resolver_t *resolver,
                                    const phosphor_input_state_t *input,
                                    uint32_t now_ms,
                                    phosphor_visual_state_t *visual);

#ifdef __cplusplus
}
#endif
