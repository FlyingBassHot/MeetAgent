/*
 * PHOSPHOR CORE P3 PC simulator.
 *
 * Same pure geometry/motion/glyph renderer as ESP32; this file only adapts
 * SDL2 display/input. 1 = Idle, 2 = Working; headless screenshots use a
 * fixed mode/time for deterministic golden frames.
 */
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <SDL2/SDL.h>

#include "phosphor_motion.h"
#include "phosphor_scene.h"
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

static void advance_motion(phosphor_motion_state_t *motion, uint32_t time_ms,
                           phosphor_motion_mode_t mode)
{
    uint32_t next = 16;
    for (; next <= time_ms; next += 16) {
        phosphor_motion_update(motion, next, mode);
    }
    if (motion->last_time_ms < time_ms) {
        phosphor_motion_update(motion, time_ms, mode);
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
        "PHOSPHOR CORE P3", SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
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
    phosphor_motion_init(&motion, PHOSPHOR_MODE_IDLE);
    phosphor_motion_mode_t mode = PHOSPHOR_MODE_IDLE;
    uint32_t last = SDL_GetTicks();
    int running = 1;
    while (running) {
        SDL_Event event;
        while (SDL_PollEvent(&event)) {
            if (event.type == SDL_QUIT ||
                (event.type == SDL_KEYDOWN &&
                 event.key.keysym.sym == SDLK_ESCAPE)) {
                running = 0;
            } else if (event.type == SDL_KEYDOWN &&
                       event.key.keysym.sym == SDLK_1) {
                mode = PHOSPHOR_MODE_IDLE;
            } else if (event.type == SDL_KEYDOWN &&
                       event.key.keysym.sym == SDLK_2) {
                mode = PHOSPHOR_MODE_WORKING;
            }
        }
        uint32_t now = SDL_GetTicks();
        phosphor_motion_update(&motion, now - last + motion.last_time_ms,
                               mode);
        last = now;
        phosphor_scene_render_motion_rgb565(core, PHOSPHOR_CANVAS_W,
                                            PHOSPHOR_CANVAS_H, &motion);
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

int main(int argc, char **argv)
{
    const char *screenshot = NULL;
    phosphor_motion_mode_t mode = PHOSPHOR_MODE_IDLE;
    uint32_t time_ms = 0;
    for (int i = 1; i < argc; ++i) {
        if (strcmp(argv[i], "--screenshot") == 0 && i + 1 < argc) {
            screenshot = argv[++i];
        } else if (strcmp(argv[i], "--state") == 0 && i + 1 < argc) {
            const char *state = argv[++i];
            if (strcmp(state, "idle") == 0) {
                mode = PHOSPHOR_MODE_IDLE;
            } else if (strcmp(state, "working") == 0) {
                mode = PHOSPHOR_MODE_WORKING;
            } else {
                fprintf(stderr, "state must be idle|working\n");
                return 2;
            }
        } else if (strcmp(argv[i], "--time-ms") == 0 && i + 1 < argc) {
            time_ms = (uint32_t)strtoul(argv[++i], NULL, 10);
        } else {
            fprintf(stderr,
                    "usage: %s [--screenshot file.bmp] [--state idle|working]"
                    " [--time-ms N]\n",
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
        phosphor_motion_init(&motion, mode);
        advance_motion(&motion, time_ms, mode);
        phosphor_scene_render_motion_rgb565(core, PHOSPHOR_CANVAS_W,
                                            PHOSPHOR_CANVAS_H, &motion);
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
