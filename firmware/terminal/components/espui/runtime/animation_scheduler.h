/*
 * Animation scheduler.
 *
 * Fixed slots, a single logical clock, theme-level speed scaling and pause.
 * It never creates LVGL objects and never allocates. The UI task calls tick()
 * with the elapsed monotonic milliseconds.
 */
#pragma once

#include <stdbool.h>
#include <stdint.h>

#include "espui_types.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef void (*espui_motion_tick_fn)(uint32_t delta_ms, void *user_ctx);

typedef struct {
    bool used;
    const char *name;
    espui_motion_tick_fn fn;
    void *user_ctx;
    uint32_t period_ms; /* 0 = once per tick */
    uint64_t accumulator;
} espui_motion_slot_t;

typedef struct {
    espui_motion_slot_t slots[ESPUI_MAX_MOTION_SLOTS];
    uint8_t count;
    uint16_t speed_percent; /* 100 = 1.0 */
    bool paused;
} espui_animation_scheduler_t;

void espui_animation_scheduler_init(espui_animation_scheduler_t *scheduler);

/* Returns the slot index, or -1 when full/invalid. */
int32_t espui_animation_scheduler_add(espui_animation_scheduler_t *scheduler,
                                      const char *name,
                                      espui_motion_tick_fn fn, void *user_ctx,
                                      uint32_t period_ms);
bool espui_animation_scheduler_remove(espui_animation_scheduler_t *scheduler,
                                      int32_t slot);
void espui_animation_scheduler_clear(espui_animation_scheduler_t *scheduler);

void espui_animation_scheduler_set_speed(espui_animation_scheduler_t *scheduler,
                                         uint16_t speed_percent);
/* Pausing does not accumulate backlog; resume starts from zero. */
void espui_animation_scheduler_set_paused(espui_animation_scheduler_t *scheduler,
                                          bool paused);

/* Advances time and returns the number of callbacks invoked. */
uint32_t espui_animation_scheduler_tick(espui_animation_scheduler_t *scheduler,
                                        uint32_t elapsed_ms);

#ifdef __cplusplus
}
#endif
