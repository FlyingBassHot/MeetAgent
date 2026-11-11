/*
 * PHOSPHOR CORE P1 PC simulator.
 *
 * The renderer and glyph atlas are the same pure C modules used on ESP32.
 * Only this display adapter is SDL2-specific.
 */
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <SDL2/SDL.h>

#include "phosphor_scene.h"
#include "phosphor_types.h"

#define SCREEN_W 480
#define SCREEN_H 480

static void compose_screen(uint16_t *screen)
{
    const uint16_t background = 0x0861; /* #080C0A */
    for (int i = 0; i < SCREEN_W * SCREEN_H; ++i) {
        screen[i] = background;
    }
    uint16_t core[PHOSPHOR_CANVAS_W * PHOSPHOR_CANVAS_H];
    phosphor_scene_render_rgb565(core, PHOSPHOR_CANVAS_W,
                                 PHOSPHOR_CANVAS_H);
    for (int y = 0; y < PHOSPHOR_CANVAS_H; ++y) {
        memcpy(&screen[(PHOSPHOR_CANVAS_Y + y) * SCREEN_W + PHOSPHOR_CANVAS_X],
               &core[y * PHOSPHOR_CANVAS_W],
               PHOSPHOR_CANVAS_W * sizeof(uint16_t));
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

static int run_window(const uint16_t *screen)
{
    SDL_Window *window = SDL_CreateWindow(
        "PHOSPHOR CORE P1", SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
        SCREEN_W, SCREEN_H, 0);
    if (window == NULL) {
        fprintf(stderr, "window: %s\n", SDL_GetError());
        return 1;
    }
    SDL_Renderer *renderer = SDL_CreateRenderer(window, -1, 0);
    SDL_Texture *texture = renderer != NULL
                               ? SDL_CreateTexture(renderer,
                                                   SDL_PIXELFORMAT_RGB565,
                                                   SDL_TEXTUREACCESS_STATIC,
                                                   SCREEN_W, SCREEN_H)
                               : NULL;
    if (renderer == NULL || texture == NULL) {
        fprintf(stderr, "renderer/texture: %s\n", SDL_GetError());
        if (texture != NULL) SDL_DestroyTexture(texture);
        if (renderer != NULL) SDL_DestroyRenderer(renderer);
        SDL_DestroyWindow(window);
        return 1;
    }
    SDL_UpdateTexture(texture, NULL, screen, SCREEN_W * sizeof(uint16_t));
    SDL_RenderClear(renderer);
    SDL_RenderCopy(renderer, texture, NULL, NULL);
    SDL_RenderPresent(renderer);

    int running = 1;
    while (running) {
        SDL_Event event;
        while (SDL_PollEvent(&event)) {
            if (event.type == SDL_QUIT ||
                (event.type == SDL_KEYDOWN && event.key.keysym.sym == SDLK_ESCAPE)) {
                running = 0;
            }
        }
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
    for (int i = 1; i < argc; ++i) {
        if (strcmp(argv[i], "--screenshot") == 0 && i + 1 < argc) {
            screenshot = argv[++i];
        } else {
            fprintf(stderr, "usage: %s [--screenshot file.bmp]\n", argv[0]);
            return 2;
        }
    }

    if (SDL_Init(SDL_INIT_VIDEO) != 0) {
        fprintf(stderr, "SDL_Init: %s\n", SDL_GetError());
        return 1;
    }
    uint16_t *screen = calloc(SCREEN_W * SCREEN_H, sizeof(uint16_t));
    if (screen == NULL) {
        SDL_Quit();
        return 1;
    }
    compose_screen(screen);
    int rc;
    if (screenshot != NULL) {
        rc = save_bmp(screen, screenshot);
    } else {
        rc = run_window(screen);
    }
    free(screen);
    SDL_Quit();
    return rc;
}
