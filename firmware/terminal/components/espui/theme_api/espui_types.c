#include "espui_types.h"

#include "espui_util.h"

const char *espui_node_status_name(espui_node_status_t status)
{
    switch (status) {
    case ESPUI_NODE_OFFLINE:
        return "offline";
    case ESPUI_NODE_CONNECTING:
        return "connecting";
    case ESPUI_NODE_ONLINE:
        return "online";
    case ESPUI_NODE_SYNCING:
        return "syncing";
    default:
        return "unknown";
    }
}

const char *espui_agent_status_name(espui_agent_status_t status)
{
    switch (status) {
    case ESPUI_AGENT_UNKNOWN:
        return "unknown";
    case ESPUI_AGENT_IDLE:
        return "idle";
    case ESPUI_AGENT_BUSY:
        return "busy";
    case ESPUI_AGENT_ERROR:
        return "error";
    case ESPUI_AGENT_OFFLINE:
        return "offline";
    default:
        return "unknown";
    }
}

const char *espui_session_status_name(espui_session_status_t status)
{
    switch (status) {
    case ESPUI_SESSION_UNKNOWN:
        return "unknown";
    case ESPUI_SESSION_RUNNING:
        return "running";
    case ESPUI_SESSION_WAITING_APPROVAL:
        return "waiting_approval";
    case ESPUI_SESSION_COMPLETED:
        return "completed";
    case ESPUI_SESSION_FAILED:
        return "failed";
    case ESPUI_SESSION_INTERRUPTED:
        return "interrupted";
    default:
        return "unknown";
    }
}

const char *espui_attention_kind_name(espui_attention_kind_t kind)
{
    switch (kind) {
    case ESPUI_ATTENTION_APPROVAL:
        return "approval";
    case ESPUI_ATTENTION_ERROR:
        return "error";
    case ESPUI_ATTENTION_COMPLETION:
        return "completion";
    default:
        return "unknown";
    }
}

const char *espui_input_type_name(espui_input_type_t type)
{
    switch (type) {
    case ESPUI_INPUT_NONE:
        return "none";
    case ESPUI_INPUT_ENCODER_CW:
        return "encoder_cw";
    case ESPUI_INPUT_ENCODER_CCW:
        return "encoder_ccw";
    case ESPUI_INPUT_ENCODER_PRESS:
        return "encoder_press";
    case ESPUI_INPUT_SELECT:
        return "select";
    case ESPUI_INPUT_BACK:
        return "back";
    case ESPUI_INPUT_LONG_PRESS:
        return "long_press";
    case ESPUI_INPUT_TOUCH:
        return "touch";
    default:
        return "unknown";
    }
}

const char *espui_nav_intent_name(espui_nav_intent_t intent)
{
    switch (intent) {
    case ESPUI_NAV_NONE:
        return "none";
    case ESPUI_NAV_NEXT:
        return "next";
    case ESPUI_NAV_PREV:
        return "prev";
    case ESPUI_NAV_ACTIVATE:
        return "activate";
    case ESPUI_NAV_BACK:
        return "back";
    case ESPUI_NAV_SCENE_AMBIENT:
        return "scene_ambient";
    case ESPUI_NAV_SCENE_FOCUS:
        return "scene_focus";
    case ESPUI_NAV_SCENE_ACTIVITY:
        return "scene_activity";
    case ESPUI_NAV_SCENE_ATTENTION:
        return "scene_attention";
    case ESPUI_NAV_SCENE_SETTINGS:
        return "scene_settings";
    case ESPUI_NAV_TOGGLE_ATTENTION:
        return "toggle_attention";
    default:
        return "unknown";
    }
}

const char *espui_action_type_name(espui_action_type_t type)
{
    switch (type) {
    case ESPUI_ACTION_NONE:
        return "none";
    case ESPUI_ACTION_INTERRUPT_SESSION:
        return "session.interrupt";
    case ESPUI_ACTION_RESPOND_PERMISSION:
        return "permission.respond";
    case ESPUI_ACTION_REFRESH_STATE:
        return "node.refresh";
    case ESPUI_ACTION_SET_THEME:
        return "ui.set_theme";
    default:
        return "unknown";
    }
}

espui_action_t espui_action_make(espui_action_type_t type)
{
    espui_action_t action = {0};
    action.type = type;
    action.decision = ESPUI_PERMISSION_REJECT;
    return action;
}

espui_action_t espui_action_interrupt(const char *session_id)
{
    espui_action_t action = espui_action_make(ESPUI_ACTION_INTERRUPT_SESSION);
    espui_strlcpy(action.session_id, session_id, sizeof(action.session_id));
    return action;
}

espui_action_t espui_action_permission(const char *permission_id,
                                       espui_permission_decision_t decision)
{
    espui_action_t action = espui_action_make(ESPUI_ACTION_RESPOND_PERMISSION);
    espui_strlcpy(action.permission_id, permission_id,
                  sizeof(action.permission_id));
    action.decision = decision;
    return action;
}

const char *espui_permission_decision_name(espui_permission_decision_t decision)
{
    switch (decision) {
    case ESPUI_PERMISSION_ONCE:
        return "once";
    case ESPUI_PERMISSION_ALWAYS:
        return "always";
    case ESPUI_PERMISSION_REJECT:
        return "reject";
    default:
        return "reject";
    }
}
