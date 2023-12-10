#ifndef Gui_H
#define Gui_H

#include <SDL2/SDL.h>
#include <string>

#define PROGRESS_TEXT_NONE       0
#define PROGRESS_TEXT_PERCENTAGE 1
#define PROGRESS_TEXT_STEPS      2

extern void gui_set_renderer(SDL_Renderer* renderer);
extern void gui_text(std::string text, int x, int y);
extern void gui_text_centered(std::string text, int x, int y, int w, int h);
extern void gui_rect(int x, int y, int w, int h, int color);
extern bool gui_button(std::string text, int x, int y, int width, int height);
extern void gui_progress(int x, int y, int width, int height, float progress, int text_type = PROGRESS_TEXT_NONE, int steps_max = 0);

#endif