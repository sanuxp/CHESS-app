#pragma once

#include <SDL.h>

#include <string>

// Built-in 5x7 pixel font (letters, digits and a little punctuation), so the app needs no font file and looks
// the same on every platform. draw() writes upper-case; drawExact() keeps lower-case (for chess notation).
namespace font {

int advance(int scale);
int lineHeight(int scale);
int textWidth(const std::string& text, int scale);
void draw(SDL_Renderer* r, const std::string& text, int x, int y, int scale, SDL_Color color);
void drawExact(SDL_Renderer* r, const std::string& text, int x, int y, int scale, SDL_Color color);
// Word-wraps inside maxWidth. Returns the height used.
int drawWrapped(SDL_Renderer* r, const std::string& text, int x, int y, int maxWidth, int scale, SDL_Color color,
                bool centered = false);
int drawWrappedExact(SDL_Renderer* r, const std::string& text, int x, int y, int maxWidth, int scale, SDL_Color color,
                     bool centered = false);

}  // namespace font
