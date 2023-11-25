#ifndef RenderUtils_H
#define RenderUtils_H

#include <SDL2/SDL.h>

extern void render_fill_rect(SDL_Renderer* renderer, float x, float y, float w, float h);
extern void render_draw_rect(SDL_Renderer* renderer, float x, float y, float w, float h);
extern void render_draw_tile(SDL_Renderer* renderer, SDL_Texture* tileset, int tile, float x, float y);

#endif