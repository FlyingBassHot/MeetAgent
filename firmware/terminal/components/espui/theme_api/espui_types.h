/*
 * ESPUI shared types.
 *
 * Only fixed-size, allocation-free value types live here so that the theme
 * API, UI core and HAL can share them without pulling in LVGL or FreeRTOS.
 */
#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

#define ESPUI_THEME_API_VERSION 1

#define ESPUI_ID_MAX 40
#define ESPUI_NAME_MAX 32
#define ESPUI_TITLE_MAX 48
#define ESPUI_SHORT_TEXT_MAX 64
#define ESPUI_LABEL_MAX 64

#define ESPUI_MAX_AGENTS 8
#define ESPUI_MAX_SESSIONS 8
#define ESPUI_MAX_ATTENTION 6
#define ESPUI_ALERT_COUNT_MAX 99
#define ESPUI_MAX_SCENE_STACK 8
#define ESPUI_MAX_MOTION_SLOTS 8
#define ESPUI_MAX_ASSETS 16

/* Display orientation. Physical panel orientation after rotation. */
typedef enum {
    ESPUI_ORIENTATION_PORTRAIT = 0,
    ESPUI_ORIENTATION_LANDSCAPE,
    ESPUI_ORIENTATION_ANY,
} espui_orientation_t;

/* Abstract input events produced by the HAL / input task. */
typedef enum {
    ESPUI_INPUT_NONE = 0,
    ESPUI_INPUT_ENCODER_CW,
    ESPUI_INPUT_ENCODER_CCW,
    ESPUI_INPUT_ENCODER_PRESS,
    ESPUI_INPUT_SELECT,
    ESPUI_INPUT_BACK,
    ESPUI_INPUT_LONG_PRESS,
    ESPUI_INPUT_TOUCH,
    ESPUI_INPUT_TAP,
    ESPUI_INPUT_SWIPE_LEFT,
    ESPUI_INPUT_SWIPE_RIGHT,
    ESPUI_INPUT_SWIPE_UP,
    ESPUI_INPUT_SWIPE_DOWN,
} espui_input_type_t;

typedef struct {
    espui_input_type_t type;
    int32_t value;        /* Encoder delta or button index. */
    uint16_t x;           /* Touch coordinates, 0 when unused. */
    uint16_t y;
    bool pressed;
    uint32_t timestamp_ms; /* Device uptime when the event was produced. */
} espui_input_event_t;

/* Abstract UI intents. Themes may only produce these, never raw commands. */
typedef enum {
    ESPUI_NAV_NONE = 0,
    ESPUI_NAV_NEXT,
    ESPUI_NAV_PREV,
    ESPUI_NAV_ACTIVATE,
    ESPUI_NAV_BACK,
    ESPUI_NAV_SCENE_AMBIENT,
    ESPUI_NAV_SCENE_FOCUS,
    ESPUI_NAV_SCENE_ACTIVITY,
    ESPUI_NAV_SCENE_ATTENTION,
    ESPUI_NAV_SCENE_SETTINGS,
    ESPUI_NAV_TOGGLE_ATTENTION,
} espui_nav_intent_t;

/* Actions the UI core may forward to the Node command client. */
typedef enum {
    ESPUI_ACTION_NONE = 0,
    ESPUI_ACTION_INTERRUPT_SESSION,
    ESPUI_ACTION_RESPOND_PERMISSION,
    ESPUI_ACTION_REFRESH_STATE,
    ESPUI_ACTION_SET_THEME,
} espui_action_type_t;

typedef enum {
    ESPUI_PERMISSION_ONCE = 0,
    ESPUI_PERMISSION_ALWAYS,
    ESPUI_PERMISSION_REJECT,
} espui_permission_decision_t;

typedef struct {
    espui_action_type_t type;
    char session_id[ESPUI_ID_MAX];
    char permission_id[ESPUI_ID_MAX];
    char theme_id[ESPUI_ID_MAX];
    espui_permission_decision_t decision;
} espui_action_t;

typedef enum {
    ESPUI_ACTION_STATUS_PENDING = 0,
    ESPUI_ACTION_STATUS_OK,
    ESPUI_ACTION_STATUS_FAILED,
    ESPUI_ACTION_STATUS_TIMEOUT,
    ESPUI_ACTION_STATUS_REJECTED,
} espui_action_status_t;

typedef struct {
    espui_action_t action;
    espui_action_status_t status;
    char message[ESPUI_SHORT_TEXT_MAX];
} espui_action_result_t;

/* ViewModel / protocol status enums. */
typedef enum {
    ESPUI_NODE_OFFLINE = 0,
    ESPUI_NODE_CONNECTING,
    ESPUI_NODE_ONLINE,
    ESPUI_NODE_SYNCING,
} espui_node_status_t;

typedef enum {
    ESPUI_AGENT_UNKNOWN = 0,
    ESPUI_AGENT_IDLE,
    ESPUI_AGENT_BUSY,
    ESPUI_AGENT_ERROR,
    ESPUI_AGENT_OFFLINE,
} espui_agent_status_t;

typedef enum {
    ESPUI_SESSION_UNKNOWN = 0,
    ESPUI_SESSION_RUNNING,
    ESPUI_SESSION_WAITING_APPROVAL,
    ESPUI_SESSION_COMPLETED,
    ESPUI_SESSION_FAILED,
    ESPUI_SESSION_INTERRUPTED,
} espui_session_status_t;

typedef enum {
    ESPUI_ATTENTION_APPROVAL = 0,
    ESPUI_ATTENTION_ERROR,
    ESPUI_ATTENTION_COMPLETION,
} espui_attention_kind_t;

const char *espui_node_status_name(espui_node_status_t status);
const char *espui_agent_status_name(espui_agent_status_t status);
const char *espui_session_status_name(espui_session_status_t status);
const char *espui_attention_kind_name(espui_attention_kind_t kind);
const char *espui_input_type_name(espui_input_type_t type);
const char *espui_nav_intent_name(espui_nav_intent_t intent);
const char *espui_action_type_name(espui_action_type_t type);

/* Fills action_id fields safely; the action type is left untouched. */
espui_action_t espui_action_make(espui_action_type_t type);
espui_action_t espui_action_interrupt(const char *session_id);
espui_action_t espui_action_permission(const char *permission_id,
                                       espui_permission_decision_t decision);
const char *espui_permission_decision_name(espui_permission_decision_t decision);

#ifdef __cplusplus
}
#endif
