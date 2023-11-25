#include <SDL2/SDL.h>
#include <iostream>

#include "gui.h"
#include "updater.h"

int mouseX;
int mouseY;
bool mousePressed;
bool mouseHeld;
bool prevMouseHeld;

bool should_run_saturn = false;

int main(int argc, char** argv) {
    if (updater_init()) {
        run_saturn();
        return 0;
    }
    SDL_Init(SDL_INIT_VIDEO);
    SDL_DisplayMode dm;
    SDL_GetDisplayMode(0, 0, &dm);
    SDL_WindowFlags window_flags = (SDL_WindowFlags)(SDL_WINDOW_RESIZABLE | SDL_WINDOW_ALLOW_HIGHDPI);
    SDL_Window* window = SDL_CreateWindow("Saturn Updater", SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, 266, 86, window_flags);
    SDL_Renderer* renderer = SDL_CreateRenderer(window, -1, SDL_RENDERER_PRESENTVSYNC | SDL_RENDERER_ACCELERATED | SDL_RENDERER_TARGETTEXTURE);
    SDL_SetWindowResizable(window, SDL_FALSE);
    gui_set_renderer(renderer);
    bool done = false;
    while (!done) {
        SDL_Event event;
        prevMouseHeld = mouseHeld; 
        while (SDL_PollEvent(&event)) {
            if (event.type == SDL_QUIT) done = true;
            if (event.type == SDL_WINDOWEVENT && event.window.event == SDL_WINDOWEVENT_CLOSE && event.window.windowID == SDL_GetWindowID(window)) done = true;
            if (event.type == SDL_MOUSEBUTTONDOWN) {
                if (event.button.button == SDL_BUTTON_LEFT) mouseHeld = true;
            }
            if (event.type == SDL_MOUSEBUTTONUP) {
                if (event.button.button == SDL_BUTTON_LEFT) mouseHeld = false;
            }
        }
        mousePressed = !prevMouseHeld && mouseHeld;
        SDL_GetMouseState(&mouseX, &mouseY);
        SDL_SetRenderDrawColor(renderer, 31, 31, 31, 255);
        SDL_RenderClear(renderer);
        if (updater()) done = true;
        SDL_RenderPresent(renderer);
    }
    SDL_DestroyRenderer(renderer);
    SDL_DestroyWindow(window);
    SDL_Quit();
    if (should_run_saturn) run_saturn();
    return 0;
}