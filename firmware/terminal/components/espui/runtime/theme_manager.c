#include "theme_manager.h"

#include <string.h>

#include "espui_error.h"
#include "espui_renderer.h"
#include "espui_theme.h"
#include "espui_tokens.h"
#include "espui_util.h"

static void report_reset(espui_theme_switch_report_t *report,
                         const char *requested_id)
{
    memset(report, 0, sizeof(*report));
    report->status = ESPUI_THEME_SWITCH_OK;
    report->reason = ESPUI_THEME_REASON_NONE;
    report->manifest_status = ESPUI_MANIFEST_OK;
    report->error = ESP_OK;
    if (requested_id != NULL) {
        espui_strlcpy(report->requested_id, requested_id,
                      sizeof(report->requested_id));
    }
}

static bool ops_are_complete(const espui_theme_ops_t *ops)
{
    return ops != NULL && ops->init != NULL && ops->enter_scene != NULL &&
           ops->update != NULL && ops->destroy != NULL;
}

static esp_err_t check_descriptor(const espui_theme_manager_t *manager,
                                  const espui_theme_descriptor_t *descriptor,
                                  espui_theme_switch_report_t *report,
                                  const espui_renderer_t **out_renderer)
{
    if (descriptor == NULL || descriptor->manifest == NULL ||
        !ops_are_complete(descriptor->ops)) {
        report->reason = ESPUI_THEME_REASON_INVALID_DESCRIPTOR;
        report->error = ESPUI_ERR_MANIFEST;
        return ESPUI_ERR_MANIFEST;
    }

    espui_manifest_status_t manifest_status =
        espui_manifest_validate(descriptor->manifest, manager->display);
    if (manifest_status != ESPUI_MANIFEST_OK) {
        report->manifest_status = manifest_status;
        report->error = ESPUI_ERR_MANIFEST;
        report->reason = manifest_status == ESPUI_MANIFEST_BAD_DISPLAY
                             ? ESPUI_THEME_REASON_DISPLAY_UNSUPPORTED
                             : ESPUI_THEME_REASON_MANIFEST;
        return ESPUI_ERR_MANIFEST;
    }

    const espui_renderer_t *renderer = espui_renderer_registry_find(
        descriptor->manifest->renderer_id);
    if (renderer == NULL) {
        report->reason = ESPUI_THEME_REASON_RENDERER_MISSING;
        report->error = ESPUI_ERR_RENDERER_MISSING;
        return ESPUI_ERR_RENDERER_MISSING;
    }

    *out_renderer = renderer;
    return ESP_OK;
}

static esp_err_t activate(espui_theme_manager_t *manager,
                          const espui_theme_descriptor_t *descriptor,
                          const espui_renderer_t *renderer,
                          espui_scene_id_t scene,
                          const espui_view_model_t *model)
{
    if (manager->active != NULL) {
        manager->active->ops->destroy();
        manager->active = NULL;
    }

    /*
     * Publish the pending theme before init so context accessors
     * (espui_renderer / espui_tokens) are usable from init(). A failed init is
     * rolled back and destroy() must tolerate a partially initialized theme.
     */
    manager->renderer = renderer;
    manager->active = descriptor;

    esp_err_t err = descriptor->ops->init(manager->ctx);
    if (err != ESP_OK) {
        descriptor->ops->destroy();
        manager->active = NULL;
        manager->renderer = NULL;
        return ESPUI_ERR_THEME_INIT;
    }

    err = descriptor->ops->enter_scene(scene, model);
    if (err != ESP_OK) {
        descriptor->ops->destroy();
        manager->active = NULL;
        manager->renderer = NULL;
        return ESPUI_ERR_THEME_INIT;
    }

    descriptor->ops->update(model);
    ++manager->switches;
    return ESP_OK;
}

static esp_err_t use_fallback(espui_theme_manager_t *manager,
                              espui_scene_id_t scene,
                              const espui_view_model_t *model,
                              espui_theme_switch_report_t *report,
                              esp_err_t cause_error)
{
    if (manager->fallback == NULL) {
        report->status = ESPUI_THEME_SWITCH_NO_FALLBACK;
        report->reason = ESPUI_THEME_REASON_FALLBACK_FAILED;
        report->error = ESPUI_ERR_FALLBACK_FAILED;
        return ESPUI_ERR_FALLBACK_FAILED;
    }

    if (manager->active == manager->fallback) {
        if (!ops_are_complete(manager->fallback->ops)) {
            report->status = ESPUI_THEME_SWITCH_NO_FALLBACK;
            report->reason = ESPUI_THEME_REASON_FALLBACK_FAILED;
            report->error = ESPUI_ERR_FALLBACK_FAILED;
            return ESPUI_ERR_FALLBACK_FAILED;
        }
        manager->fallback->ops->update(model);
        report->status = ESPUI_THEME_SWITCH_FALLBACK_USED;
        report->error = cause_error;
        espui_strlcpy(report->active_id,
                      manager->fallback->manifest->theme_id,
                      sizeof(report->active_id));
        return cause_error;
    }

    espui_theme_switch_report_t check_report = *report;
    const espui_renderer_t *renderer = NULL;
    esp_err_t check = check_descriptor(manager, manager->fallback, &check_report,
                                       &renderer);
    if (check != ESP_OK) {
        report->status = ESPUI_THEME_SWITCH_NO_FALLBACK;
        report->reason = ESPUI_THEME_REASON_FALLBACK_FAILED;
        report->error = ESPUI_ERR_FALLBACK_FAILED;
        return ESPUI_ERR_FALLBACK_FAILED;
    }

    esp_err_t err = activate(manager, manager->fallback, renderer, scene, model);
    if (err != ESP_OK) {
        report->status = ESPUI_THEME_SWITCH_NO_FALLBACK;
        report->reason = ESPUI_THEME_REASON_FALLBACK_FAILED;
        report->error = ESPUI_ERR_FALLBACK_FAILED;
        return ESPUI_ERR_FALLBACK_FAILED;
    }

    report->status = ESPUI_THEME_SWITCH_FALLBACK_USED;
    report->error = cause_error;
    espui_strlcpy(report->active_id, manager->fallback->manifest->theme_id,
                  sizeof(report->active_id));
    return cause_error;
}

void espui_theme_manager_init(espui_theme_manager_t *manager,
                              const espui_context_t *ctx,
                              const espui_theme_descriptor_t *fallback,
                              const espui_display_info_t *display)
{
    if (manager == NULL) {
        return;
    }
    memset(manager, 0, sizeof(*manager));
    manager->ctx = ctx;
    manager->fallback = fallback;
    manager->display = display;
    manager->initialized = true;
    report_reset(&manager->last_report, NULL);
}

esp_err_t espui_theme_manager_apply(espui_theme_manager_t *manager,
                                    const char *theme_id,
                                    espui_scene_id_t scene,
                                    const espui_view_model_t *model)
{
    if (manager == NULL) {
        return ESP_ERR_INVALID_ARG;
    }

    report_reset(&manager->last_report, theme_id);

    if (!manager->initialized) {
        manager->last_report.status = ESPUI_THEME_SWITCH_NOT_READY;
        manager->last_report.error = ESPUI_ERR_NOT_INITIALIZED;
        return ESPUI_ERR_NOT_INITIALIZED;
    }

    const espui_theme_descriptor_t *descriptor =
        espui_theme_registry_find(theme_id);
    if (descriptor == NULL) {
        manager->last_report.reason = ESPUI_THEME_REASON_UNKNOWN_THEME;
        manager->last_report.error = ESP_ERR_NOT_FOUND;
        return use_fallback(manager, scene, model, &manager->last_report,
                            ESP_ERR_NOT_FOUND);
    }

    const espui_renderer_t *renderer = NULL;
    esp_err_t err = check_descriptor(manager, descriptor,
                                     &manager->last_report, &renderer);
    if (err != ESP_OK) {
        return use_fallback(manager, scene, model, &manager->last_report, err);
    }

    if (descriptor == manager->active) {
        manager->last_report.status = ESPUI_THEME_SWITCH_SAME_THEME;
        manager->last_report.reason = ESPUI_THEME_REASON_NONE;
        manager->last_report.error = ESP_OK;
        espui_strlcpy(manager->last_report.active_id,
                      descriptor->manifest->theme_id,
                      sizeof(manager->last_report.active_id));
        descriptor->ops->update(model);
        return ESP_OK;
    }

    err = activate(manager, descriptor, renderer, scene, model);
    if (err != ESP_OK) {
        manager->last_report.reason = ESPUI_THEME_REASON_INIT_FAILED;
        manager->last_report.error = ESPUI_ERR_THEME_INIT;
        return use_fallback(manager, scene, model, &manager->last_report, err);
    }

    manager->last_report.status = ESPUI_THEME_SWITCH_OK;
    manager->last_report.reason = ESPUI_THEME_REASON_NONE;
    manager->last_report.error = ESP_OK;
    espui_strlcpy(manager->last_report.active_id,
                  descriptor->manifest->theme_id,
                  sizeof(manager->last_report.active_id));
    return ESP_OK;
}

const espui_theme_descriptor_t *espui_theme_manager_active(
    const espui_theme_manager_t *manager)
{
    return manager == NULL ? NULL : manager->active;
}

const espui_renderer_t *espui_theme_manager_renderer(
    const espui_theme_manager_t *manager)
{
    return manager == NULL ? NULL : manager->renderer;
}

const espui_tokens_t *espui_theme_manager_tokens(
    const espui_theme_manager_t *manager)
{
    if (manager == NULL || manager->active == NULL) {
        return NULL;
    }
    return manager->active->tokens != NULL ? manager->active->tokens
                                           : espui_tokens_safe_default();
}

const espui_theme_switch_report_t *espui_theme_manager_last_report(
    const espui_theme_manager_t *manager)
{
    return manager == NULL ? NULL : &manager->last_report;
}

const char *espui_theme_switch_status_name(espui_theme_switch_status_t status)
{
    switch (status) {
    case ESPUI_THEME_SWITCH_OK:
        return "ok";
    case ESPUI_THEME_SWITCH_SAME_THEME:
        return "same_theme";
    case ESPUI_THEME_SWITCH_FALLBACK_USED:
        return "fallback_used";
    case ESPUI_THEME_SWITCH_NO_FALLBACK:
        return "no_fallback";
    case ESPUI_THEME_SWITCH_NOT_READY:
        return "not_ready";
    default:
        return "unknown";
    }
}

const char *espui_theme_switch_reason_name(espui_theme_switch_reason_t reason)
{
    switch (reason) {
    case ESPUI_THEME_REASON_NONE:
        return "none";
    case ESPUI_THEME_REASON_UNKNOWN_THEME:
        return "unknown_theme";
    case ESPUI_THEME_REASON_INVALID_DESCRIPTOR:
        return "invalid_descriptor";
    case ESPUI_THEME_REASON_MANIFEST:
        return "manifest";
    case ESPUI_THEME_REASON_RENDERER_MISSING:
        return "renderer_missing";
    case ESPUI_THEME_REASON_DISPLAY_UNSUPPORTED:
        return "display_unsupported";
    case ESPUI_THEME_REASON_INIT_FAILED:
        return "init_failed";
    case ESPUI_THEME_REASON_FALLBACK_FAILED:
        return "fallback_failed";
    default:
        return "unknown";
    }
}
