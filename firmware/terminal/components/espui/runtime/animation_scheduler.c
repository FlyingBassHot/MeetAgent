#include "animation_scheduler.h"

#include <string.h>

#define ESPUI_MOTION_MAX_CALLBACKS_PER_TICK 8

void espui_animation_scheduler_init(espui_animation_scheduler_t *scheduler)
{
    if (scheduler == NULL) {
        return;
    }
    memset(scheduler, 0, sizeof(*scheduler));
    scheduler->speed_percent = 100;
}

int32_t espui_animation_scheduler_add(espui_animation_scheduler_t *scheduler,
                                      const char *name,
                                      espui_motion_tick_fn fn, void *user_ctx,
                                      uint32_t period_ms)
{
    if (scheduler == NULL || fn == NULL ||
        scheduler->count >= ESPUI_MAX_MOTION_SLOTS) {
        return -1;
    }

    for (size_t i = 0; i < ESPUI_MAX_MOTION_SLOTS; ++i) {
        espui_motion_slot_t *slot = &scheduler->slots[i];
        if (slot->used) {
            continue;
        }
        slot->used = true;
        slot->name = name != NULL ? name : "motion";
        slot->fn = fn;
        slot->user_ctx = user_ctx;
        slot->period_ms = period_ms;
        slot->accumulator = 0;
        ++scheduler->count;
        return (int32_t)i;
    }

    return -1;
}

bool espui_animation_scheduler_remove(espui_animation_scheduler_t *scheduler,
                                      int32_t slot)
{
    if (scheduler == NULL || slot < 0 ||
        (size_t)slot >= ESPUI_MAX_MOTION_SLOTS ||
        !scheduler->slots[slot].used) {
        return false;
    }
    memset(&scheduler->slots[slot], 0, sizeof(scheduler->slots[slot]));
    if (scheduler->count > 0) {
        --scheduler->count;
    }
    return true;
}

void espui_animation_scheduler_clear(espui_animation_scheduler_t *scheduler)
{
    if (scheduler == NULL) {
        return;
    }
    memset(scheduler->slots, 0, sizeof(scheduler->slots));
    scheduler->count = 0;
}

void espui_animation_scheduler_set_speed(espui_animation_scheduler_t *scheduler,
                                         uint16_t speed_percent)
{
    if (scheduler != NULL) {
        scheduler->speed_percent = speed_percent;
    }
}

void espui_animation_scheduler_set_paused(espui_animation_scheduler_t *scheduler,
                                          bool paused)
{
    if (scheduler == NULL) {
        return;
    }
    scheduler->paused = paused;
    if (paused) {
        for (size_t i = 0; i < ESPUI_MAX_MOTION_SLOTS; ++i) {
            scheduler->slots[i].accumulator = 0;
        }
    }
}

uint32_t espui_animation_scheduler_tick(espui_animation_scheduler_t *scheduler,
                                        uint32_t elapsed_ms)
{
    if (scheduler == NULL || scheduler->paused ||
        scheduler->speed_percent == 0 || elapsed_ms == 0) {
        return 0;
    }

    uint64_t scaled =
        ((uint64_t)elapsed_ms * scheduler->speed_percent) / 100u;
    uint32_t callbacks = 0;

    for (size_t i = 0; i < ESPUI_MAX_MOTION_SLOTS; ++i) {
        espui_motion_slot_t *slot = &scheduler->slots[i];
        if (!slot->used) {
            continue;
        }

        if (slot->period_ms == 0) {
            slot->fn((uint32_t)scaled, slot->user_ctx);
            ++callbacks;
            continue;
        }

        slot->accumulator += scaled;
        uint32_t per_slot = 0;
        while (slot->accumulator >= slot->period_ms &&
               per_slot < ESPUI_MOTION_MAX_CALLBACKS_PER_TICK) {
            slot->fn(slot->period_ms, slot->user_ctx);
            slot->accumulator -= slot->period_ms;
            ++per_slot;
            ++callbacks;
        }
        if (slot->accumulator >= slot->period_ms) {
            /* Bound catch-up work after a long stall; drop the backlog. */
            slot->accumulator %= slot->period_ms;
        }
    }

    return callbacks;
}
