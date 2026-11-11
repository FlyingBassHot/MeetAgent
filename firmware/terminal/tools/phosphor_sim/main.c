/*
 * PHOSPHOR CORE P4 PC simulator.
 *
 * Same pure state/motion/geometry/glyph modules as ESP32. Keys 1..6 select
 * Idle / Working / Working+Approval / Working+Error / Offline / Completed.
 * Headless mode renders fixed state/time golden frames.
 */
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <SDL2/SDL.h>

#include "phosphor_demo.h"
#include "phosphor_motion.h"
#include "phosphor_scene.h"
#include "phosphor_state.h"
#include "phosphor_types.h"

#define SCREEN_W 480
#define SCREEN_H 480

static void fill_screen(uint16_t *screen, const uint16_t *core)
{
    const uint16_t background = 0x0861; /* #080C0A */
    for (int i = 0; i < SCREEN_W * SCREEN_H; ++i) {
        screen[i] = background;
    }
    for (int y = 0; y < PHOSPHOR_CANVAS_H; ++y) {
        memcpy(&screen[(PHOSPHOR_CANVAS_Y + y) * SCREEN_W + PHOSPHOR_CANVAS_X],
               &core[y * PHOSPHOR_CANVAS_W],
               PHOSPHOR_CANVAS_W * sizeof(uint16_t));
    }
}

static void render_state(uint16_t *core, phosphor_motion_state_t *motion,
                         phosphor_state_resolver_t *resolver,
                         phosphor_demo_phase_t phase, uint32_t cycle,
                         uint32_t now_ms, bool *was_offline,
                         uint32_t *offline_start_ms)
{
    phosphor_input_state_t input;
    phosphor_visual_state_t visual;
    phosphor_demo_build_input(phase, cycle, &input);
    phosphor_state_resolver_update(resolver, &input, now_ms, &visual);
    if (visual.activity == PHOSPHOR_ACTIVITY_OFFLINE) {
        if (!*was_offline) {
            *was_offline = true;
            *offline_start_ms = now_ms;
        }
        if (now_ms - *offline_start_ms < 550u) {
            phosphor_motion_update(motion, now_ms, PHOSPHOR_MODE_IDLE);
        }
    } else {
        *was_offline = false;
        phosphor_motion_mode_t mode = visual.activity == PHOSPHOR_ACTIVITY_WORKING
                                          ? PHOSPHOR_MODE_WORKING
                                          : PHOSPHOR_MODE_IDLE;
        phosphor_motion_update(motion, now_ms, mode);
    }
    phosphor_scene_render_visual_rgb565(core, PHOSPHOR_CANVAS_W,
                                        PHOSPHOR_CANVAS_H, motion, &visual);
}

static void advance_state(phosphor_motion_state_t *motion,
                          phosphor_state_resolver_t *resolver,
                          phosphor_demo_phase_t phase, uint32_t cycle,
                          uint32_t target_ms)
{
    phosphor_input_state_t input;
    phosphor_demo_build_input(phase, cycle, &input);
    phosphor_visual_state_t visual;
    phosphor_state_resolver_update(resolver, &input, 0, &visual);
    uint32_t next = 16;
    for (; next <= target_ms; next += 16) {
        phosphor_state_resolver_update(resolver, &input, next, &visual);
        if (visual.activity != PHOSPHOR_ACTIVITY_OFFLINE) {
            phosphor_motion_mode_t mode =
                visual.activity == PHOSPHOR_ACTIVITY_WORKING
                    ? PHOSPHOR_MODE_WORKING : PHOSPHOR_MODE_IDLE;
            phosphor_motion_update(motion, next, mode);
        }
    }
}

static int save_bmp(const uint16_t *screen, const char *path)
{
    SDL_Surface *surface = SDL_CreateRGBSurfaceWithFormat(
        0, SCREEN_W, SCREEN_H, 16, SDL_PIXELFORMAT_RGB565);
    if (surface == NULL) {
        fprintf(stderr, "surface: %s\n", SDL_GetError());
        return 1;
    }
    memcpy(surface->pixels, screen, SCREEN_W * SCREEN_H * sizeof(uint16_t));
    int rc = SDL_SaveBMP(surface, path);
    if (rc != 0) {
        fprintf(stderr, "save %s: %s\n", path, SDL_GetError());
    }
    SDL_FreeSurface(surface);
    return rc == 0 ? 0 : 1;
}

static int run_window(uint16_t *screen, uint16_t *core)
{
    SDL_Window *window = SDL_CreateWindow(
        "PHOSPHOR CORE P4", SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
        SCREEN_W, SCREEN_H, 0);
    if (window == NULL) {
        fprintf(stderr, "window: %s\n", SDL_GetError());
        return 1;
    }
    SDL_Renderer *renderer = SDL_CreateRenderer(window, -1, 0);
    SDL_Texture *texture = renderer != NULL
                               ? SDL_CreateTexture(renderer,
                                                   SDL_PIXELFORMAT_RGB565,
                                                   SDL_TEXTUREACCESS_STREAMING,
                                                   SCREEN_W, SCREEN_H)
                               : NULL;
    if (renderer == NULL || texture == NULL) {
        fprintf(stderr, "renderer/texture: %s\n", SDL_GetError());
        if (texture != NULL) SDL_DestroyTexture(texture);
        if (renderer != NULL) SDL_DestroyRenderer(renderer);
        SDL_DestroyWindow(window);
        return 1;
    }

    phosphor_motion_state_t motion;
    phosphor_state_resolver_t resolver;
    phosphor_motion_init(&motion, PHOSPHOR_MODE_IDLE);
    phosphor_state_resolver_init(&resolver);
    phosphor_demo_phase_t phase = PHOSPHOR_DEMO_IDLE;
    uint32_t cycle = 1;
    uint32_t start = SDL_GetTicks();
    bool was_offline = false;
    uint32_t offline_start_ms = 0;
    int running = 1;
    while (running) {
        SDL_Event event;
        while (SDL_PollEvent(&event)) {
            if (event.type == SDL_QUIT ||
                (event.type == SDL_KEYDOWN &&
                 event.key.keysym.sym == SDLK_ESCAPE)) {
                running = 0;
            } else if (event.type == SDL_KEYDOWN) {
                switch (event.key.keysym.sym) {
                case SDLK_1: phase = PHOSPHOR_DEMO_IDLE; break;
                case SDLK_2: phase = PHOSPHOR_DEMO_WORKING; break;
                case SDLK_3: phase = PHOSPHOR_DEMO_APPROVAL; break;
                case SDLK_4: phase = PHOSPHOR_DEMO_ERROR; ++cycle; break;
                case SDLK_5: phase = PHOSPHOR_DEMO_OFFLINE; break;
                case SDLK_6:
                    phase = PHOSPHOR_DEMO_COMPLETED;
                    ++cycle;
                    break;
                default: break;
                }
            }
        }
        uint32_t now = SDL_GetTicks() - start;
        render_state(core, &motion, &resolver, phase, cycle, now,
                     &was_offline, &offline_start_ms);
        fill_screen(screen, core);
        SDL_UpdateTexture(texture, NULL, screen, SCREEN_W * sizeof(uint16_t));
        SDL_RenderClear(renderer);
        SDL_RenderCopy(renderer, texture, NULL, NULL);
        SDL_RenderPresent(renderer);
        SDL_Delay(16);
    }
    SDL_DestroyTexture(texture);
    SDL_DestroyRenderer(renderer);
    SDL_DestroyWindow(window);
    return 0;
}

static phosphor_demo_phase_t parse_state(const char *state)
{
    if (strcmp(state, "idle") == 0) return PHOSPHOR_DEMO_IDLE;
    if (strcmp(state, "working") == 0) return PHOSPHOR_DEMO_WORKING;
    if (strcmp(state, "approval") == 0) return PHOSPHOR_DEMO_APPROVAL;
    if (strcmp(state, "error") == 0) return PHOSPHOR_DEMO_ERROR;
    if (strcmp(state, "offline") == 0) return PHOSPHOR_DEMO_OFFLINE;
    if (strcmp(state, "completed") == 0) return PHOSPHOR_DEMO_COMPLETED;
    return (phosphor_demo_phase_t)-1;
}

int main(int argc, char **argv)
{
    const char *screenshot = NULL;
    phosphor_demo_phase_t phase = PHOSPHOR_DEMO_IDLE;
    uint32_t time_ms = 3000;
    for (int i = 1; i < argc; ++i) {
        if (strcmp(argv[i], "--screenshot") == 0 && i + 1 < argc) {
            screenshot = argv[++i];
        } else if (strcmp(argv[i], "--state") == 0 && i + 1 < argc) {
            phase = parse_state(argv[++i]);
            if ((int)phase < 0) {
                fprintf(stderr, "state must be idle|working|approval|error|offline|completed\n");
                return 2;
            }
        } else if (strcmp(argv[i], "--time-ms") == 0 && i + 1 < argc) {
            time_ms = (uint32_t)strtoul(argv[++i], NULL, 10);
        } else {
            fprintf(stderr,
                    "usage: %s [--screenshot file.bmp] "
                    "[--state idle|working|approval|error|offline|completed] "
                    "[--time-ms N]\n",
                    argv[0]);
            return 2;
        }
    }

    if (SDL_Init(SDL_INIT_VIDEO) != 0) {
        fprintf(stderr, "SDL_Init: %s\n", SDL_GetError());
        return 1;
    }
    uint16_t *screen = calloc(SCREEN_W * SCREEN_H, sizeof(uint16_t));
    uint16_t *core = calloc(PHOSPHOR_CANVAS_W * PHOSPHOR_CANVAS_H,
                            sizeof(uint16_t));
    if (screen == NULL || core == NULL) {
        free(screen);
        free(core);
        SDL_Quit();
        return 1;
    }
    int rc;
    if (screenshot != NULL) {
        phosphor_motion_state_t motion;
        phosphor_state_resolver_t resolver;
        phosphor_motion_init(&motion, PHOSPHOR_MODE_IDLE);
        phosphor_state_resolver_init(&resolver);
        advance_state(&motion, &resolver, phase, 1, time_ms);
        phosphor_input_state_t input;
        phosphor_visual_state_t visual;
        phosphor_demo_build_input(phase, 1, &input);
        phosphor_state_resolver_update(&resolver, &input, time_ms, &visual);
        phosphor_scene_render_visual_rgb565(core, PHOSPHOR_CANVAS_W,
                                            PHOSPHOR_CANVAS_H, &motion,
                                            &visual);
        fill_screen(screen, core);
        rc = save_bmp(screen, screenshot);
    } else {
        rc = run_window(screen, core);
    }
    free(screen);
    free(core);
    SDL_Quit();
    return rc;
}
