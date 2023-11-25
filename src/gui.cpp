#include <SDL2/SDL.h>
#include <string>

#include "main.h"
#include "font.h"

SDL_Renderer* current_renderer;
SDL_Texture* font;


void gui_init_font() {
    int numPixels = 112 * 84;
    int* pixels = (int*)malloc(numPixels * 4);
    for (int i = 0; i < numPixels; i++) {
        pixels[i] = (font_data[i / 8] & (1 << (7 - i % 8))) ? 0xFFFFFFFF : 0x00000000;
    }
    SDL_Surface* surface = SDL_CreateRGBSurfaceFrom(pixels, 112, 84, 32, 112 * 4, 0xFF000000, 0x00FF0000, 0x0000FF00, 0x000000FF);
    font = SDL_CreateTextureFromSurface(current_renderer, surface);
    SDL_FreeSurface(surface);
    free(pixels);
}

void gui_set_renderer(SDL_Renderer* renderer) {
    current_renderer = renderer;
}

void gui_text(std::string text, int x, int y) {
    if (font == nullptr) gui_init_font();
    for (int i = 0; i < text.length(); i++) {
        unsigned char character = text[i];
        int charX = character % 16;
        int charY = character / 16 - 2;
        SDL_Rect src = (SDL_Rect){ .x = charX * 7, .y = charY * 14, .w = 7, .h = 14 };
        SDL_Rect dst = (SDL_Rect){ .x = x + i * 7, .y = y, .w = 7, .h = 14 };
        SDL_RenderCopy(current_renderer, font, &src, &dst);
    }
}

void gui_text_centered(std::string text, int x, int y, int w, int h) {
    int tw = text.length() * 7;
    int th = 14;
    if (w == -1) w = tw;
    if (h == -1) h = th;
    x += w / 2 - tw / 2;
    y += h / 2 - th / 2;
    gui_text(text, x, y);
}

void gui_rect(int x, int y, int w, int h, int color) {
    SDL_SetRenderDrawColor(current_renderer, (color >> 24) & 0xFF, (color >> 16) & 0xFF, (color >> 8) & 0xFF, color & 0xFF);
    SDL_Rect rect;
    rect.x = x;
    rect.y = y;
    rect.w = w;
    rect.h = h;
    SDL_RenderFillRect(current_renderer, &rect);
}

bool gui_button(std::string text, int x, int y, int width, int height) {
    int color = 0x2F2F2FFF;
    bool hovering = mouseX >= x && mouseY >= y && mouseX < x + width && mouseY < y + height;
    if (hovering) color = 0x3F3F3FFF;
    gui_rect(x, y, width, height, color);
    gui_text_centered(text, x, y, width, height);
    return hovering && mousePressed;
}

void gui_progress(int x, int y, int width, int height, float progress) {
    if (progress < 0) progress = 0;
    if (progress > 1) progress = 1;
    gui_rect(x, y, width, height, 0x2F2F2FFF);
    gui_rect(x, y, width * progress, height, 0x1F9F1FFF);
}