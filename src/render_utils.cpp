#include "render_utils.h"

void render_fill_rect(SDL_Renderer* renderer, float x, float y, float w, float h) {
    SDL_FRect rect;
    rect.x = x;
    rect.y = y;
    rect.w = w;
    rect.h = h;
    SDL_RenderFillRectF(renderer, &rect);
}

void render_draw_rect(SDL_Renderer* renderer, float x, float y, float w, float h) {
    SDL_FRect rect;
    rect.x = x;
    rect.y = y;
    rect.w = w;
    rect.h = h;
    SDL_RenderDrawRectF(renderer, &rect);
}

void render_draw_tile(SDL_Renderer* renderer, SDL_Texture* tileset, int tile, float x, float y) {
    int X = tile % 16;
    int Y = tile / 16;
    SDL_Rect src;
    src.x = X * 16;
    src.y = Y * 16;
    src.w = 16;
    src.h = 16;
    SDL_FRect dst;
    dst.x = x;
    dst.y = y;
    dst.w = 32;
    dst.h = 32;
    SDL_RenderCopyF(renderer, tileset, &src, &dst);
}