#include "espui_view_model.h"

#include <string.h>

#include "espui_util.h"

#define ESPUI_DEFAULT_BRIGHTNESS 70
#define ESPUI_DEFAULT_ANIMATION_SPEED 100
#define ESPUI_DEFAULT_THEME "safe_fallback"

static void reset_device_defaults(espui_device_view_t *device)
{
    memset(device, 0, sizeof(*device));
    device->brightness = ESPUI_DEFAULT_BRIGHTNESS;
    device->night_mode = false;
    device->motion_enabled = true;
    device->animation_speed_percent = ESPUI_DEFAULT_ANIMATION_SPEED;
    espui_strlcpy(device->active_theme_id, ESPUI_DEFAULT_THEME,
                  sizeof(device->active_theme_id));
}

void espui_view_model_touch(espui_view_model_t *model)
{
    if (model != NULL) {
        ++model->revision;
    }
}

void espui_view_model_init(espui_view_model_t *model)
{
    if (model == NULL) {
        return;
    }
    memset(model, 0, sizeof(*model));
    model->revision = 1;
    model->node.status = ESPUI_NODE_OFFLINE;
    reset_device_defaults(&model->device);
    espui_view_model_recount_attention(model);
}

void espui_view_model_reset(espui_view_model_t *model, bool keep_device)
{
    if (model == NULL) {
        return;
    }

    uint32_t next_revision = model->revision;
    espui_device_view_t saved_device = model->device;

    memset(model, 0, sizeof(*model));
    model->revision = next_revision;
    model->node.status = ESPUI_NODE_OFFLINE;
    if (keep_device) {
        model->device = saved_device;
    } else {
        reset_device_defaults(&model->device);
    }
    espui_view_model_touch(model);
    espui_view_model_recount_attention(model);
}

espui_agent_view_t *espui_view_model_find_agent(espui_view_model_t *model,
                                                const char *agent_id)
{
    if (model == NULL || espui_str_empty(agent_id)) {
        return NULL;
    }
    for (size_t i = 0; i < ESPUI_MAX_AGENTS; ++i) {
        if (model->agents[i].in_use &&
            strcmp(model->agents[i].agent_id, agent_id) == 0) {
            return &model->agents[i];
        }
    }
    return NULL;
}

const espui_agent_view_t *espui_view_model_find_agent_const(
    const espui_view_model_t *model, const char *agent_id)
{
    return espui_view_model_find_agent((espui_view_model_t *)model, agent_id);
}

espui_session_view_t *espui_view_model_find_session(espui_view_model_t *model,
                                                    const char *session_id)
{
    if (model == NULL || espui_str_empty(session_id)) {
        return NULL;
    }
    for (size_t i = 0; i < ESPUI_MAX_SESSIONS; ++i) {
        if (model->sessions[i].in_use &&
            strcmp(model->sessions[i].session_id, session_id) == 0) {
            return &model->sessions[i];
        }
    }
    return NULL;
}

const espui_session_view_t *espui_view_model_find_session_const(
    const espui_view_model_t *model, const char *session_id)
{
    return espui_view_model_find_session((espui_view_model_t *)model,
                                         session_id);
}

espui_agent_view_t *espui_view_model_upsert_agent(espui_view_model_t *model,
                                                  const char *agent_id)
{
    if (model == NULL || espui_str_empty(agent_id)) {
        return NULL;
    }

    espui_agent_view_t *agent = espui_view_model_find_agent(model, agent_id);
    if (agent == NULL) {
        for (size_t i = 0; i < ESPUI_MAX_AGENTS; ++i) {
            if (!model->agents[i].in_use) {
                agent = &model->agents[i];
                memset(agent, 0, sizeof(*agent));
                agent->in_use = true;
                espui_strlcpy(agent->agent_id, agent_id,
                              sizeof(agent->agent_id));
                break;
            }
        }
    }
    if (agent == NULL) {
        return NULL;
    }

    espui_view_model_touch(model);
    return agent;
}

espui_session_view_t *espui_view_model_upsert_session(
    espui_view_model_t *model, const char *session_id)
{
    if (model == NULL || espui_str_empty(session_id)) {
        return NULL;
    }

    espui_session_view_t *session =
        espui_view_model_find_session(model, session_id);
    if (session == NULL) {
        for (size_t i = 0; i < ESPUI_MAX_SESSIONS; ++i) {
            if (!model->sessions[i].in_use) {
                session = &model->sessions[i];
                memset(session, 0, sizeof(*session));
                session->in_use = true;
                espui_strlcpy(session->session_id, session_id,
                              sizeof(session->session_id));
                break;
            }
        }
    }
    if (session == NULL) {
        return NULL;
    }

    espui_view_model_touch(model);
    return session;
}

espui_attention_view_t *espui_view_model_push_attention(
    espui_view_model_t *model, const char *permission_id,
    espui_attention_kind_t kind)
{
    if (model == NULL || espui_str_empty(permission_id)) {
        return NULL;
    }

    espui_attention_view_t *attention = NULL;
    for (size_t i = 0; i < ESPUI_MAX_ATTENTION; ++i) {
        if (model->attention[i].in_use &&
            strcmp(model->attention[i].permission_id, permission_id) == 0) {
            attention = &model->attention[i];
            attention->kind = kind;
            break;
        }
    }

    if (attention == NULL) {
        for (size_t i = 0; i < ESPUI_MAX_ATTENTION; ++i) {
            if (!model->attention[i].in_use) {
                attention = &model->attention[i];
                memset(attention, 0, sizeof(*attention));
                attention->in_use = true;
                attention->kind = kind;
                espui_strlcpy(attention->permission_id, permission_id,
                              sizeof(attention->permission_id));
                break;
            }
        }
    }

    if (attention == NULL) {
        return NULL;
    }

    espui_view_model_touch(model);
    espui_view_model_recount_attention(model);
    return attention;
}

bool espui_view_model_remove_agent(espui_view_model_t *model,
                                   const char *agent_id)
{
    espui_agent_view_t *agent = espui_view_model_find_agent(model, agent_id);
    if (agent == NULL) {
        return false;
    }
    memset(agent, 0, sizeof(*agent));
    espui_view_model_touch(model);
    return true;
}

bool espui_view_model_remove_session(espui_view_model_t *model,
                                     const char *session_id)
{
    espui_session_view_t *session =
        espui_view_model_find_session(model, session_id);
    if (session == NULL) {
        return false;
    }
    memset(session, 0, sizeof(*session));
    espui_view_model_touch(model);
    return true;
}

bool espui_view_model_resolve_attention(espui_view_model_t *model,
                                        const char *permission_id)
{
    if (model == NULL || espui_str_empty(permission_id)) {
        return false;
    }

    for (size_t i = 0; i < ESPUI_MAX_ATTENTION; ++i) {
        if (model->attention[i].in_use &&
            strcmp(model->attention[i].permission_id, permission_id) == 0) {
            memset(&model->attention[i], 0, sizeof(model->attention[i]));
            espui_view_model_touch(model);
            espui_view_model_recount_attention(model);
            return true;
        }
    }
    return false;
}

uint16_t espui_view_model_agent_count(const espui_view_model_t *model)
{
    if (model == NULL) {
        return 0;
    }
    uint16_t count = 0;
    for (size_t i = 0; i < ESPUI_MAX_AGENTS; ++i) {
        if (model->agents[i].in_use) {
            ++count;
        }
    }
    return count;
}

uint16_t espui_view_model_session_count(const espui_view_model_t *model)
{
    if (model == NULL) {
        return 0;
    }
    uint16_t count = 0;
    for (size_t i = 0; i < ESPUI_MAX_SESSIONS; ++i) {
        if (model->sessions[i].in_use) {
            ++count;
        }
    }
    return count;
}

uint16_t espui_view_model_active_session_count(
    const espui_view_model_t *model)
{
    if (model == NULL) {
        return 0;
    }
    uint16_t count = 0;
    for (size_t i = 0; i < ESPUI_MAX_SESSIONS; ++i) {
        if (!model->sessions[i].in_use) {
            continue;
        }
        if (model->sessions[i].status == ESPUI_SESSION_RUNNING ||
            model->sessions[i].status == ESPUI_SESSION_WAITING_APPROVAL) {
            ++count;
        }
    }
    return count;
}

void espui_view_model_recount_attention(espui_view_model_t *model)
{
    if (model == NULL) {
        return;
    }
    uint16_t approvals = 0;
    uint16_t errors = 0;
    uint16_t completions = 0;

    for (size_t i = 0; i < ESPUI_MAX_ATTENTION; ++i) {
        if (!model->attention[i].in_use) {
            continue;
        }
        switch (model->attention[i].kind) {
        case ESPUI_ATTENTION_APPROVAL:
            ++approvals;
            break;
        case ESPUI_ATTENTION_ERROR:
            ++errors;
            break;
        case ESPUI_ATTENTION_COMPLETION:
            ++completions;
            break;
        default:
            break;
        }
    }

    uint32_t total_errors = (uint32_t)errors + model->transient_error_count;
    uint32_t total_completions =
        (uint32_t)completions + model->transient_completion_count;
    model->approval_count = approvals;
    model->error_count = (uint16_t)(total_errors > ESPUI_ALERT_COUNT_MAX
                                        ? ESPUI_ALERT_COUNT_MAX
                                        : total_errors);
    model->completion_count =
        (uint16_t)(total_completions > ESPUI_ALERT_COUNT_MAX
                       ? ESPUI_ALERT_COUNT_MAX
                       : total_completions);
}

void espui_view_model_note_error(espui_view_model_t *model)
{
    if (model == NULL) {
        return;
    }
    if (model->transient_error_count < ESPUI_ALERT_COUNT_MAX) {
        ++model->transient_error_count;
    }
    espui_view_model_touch(model);
    espui_view_model_recount_attention(model);
}

void espui_view_model_note_completion(espui_view_model_t *model)
{
    if (model == NULL) {
        return;
    }
    if (model->transient_completion_count < ESPUI_ALERT_COUNT_MAX) {
        ++model->transient_completion_count;
    }
    espui_view_model_touch(model);
    espui_view_model_recount_attention(model);
}

uint16_t espui_view_model_attention_count(const espui_view_model_t *model)
{
    if (model == NULL) {
        return 0;
    }
    return (uint16_t)(model->approval_count + model->error_count +
                      model->completion_count);
}

void espui_view_model_copy(espui_view_model_t *dst,
                           const espui_view_model_t *src)
{
    if (dst == NULL || src == NULL || dst == src) {
        return;
    }
    *dst = *src;
}

void espui_view_model_copy_business(espui_view_model_t *dst,
                                    const espui_view_model_t *src)
{
    if (dst == NULL || src == NULL || dst == src) {
        return;
    }

    espui_device_view_t device = dst->device;
    uint32_t revision = dst->revision;
    *dst = *src;
    dst->device = device;
    dst->revision = revision;
    espui_view_model_touch(dst);
}
