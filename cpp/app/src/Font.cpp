#include "Font.hpp"

#include <cctype>
#include <cstdint>
#include <vector>

namespace font {

namespace {

struct Glyph {
    char c;
    uint8_t rows[7];  // 5 bits per row, bit 4 = leftmost pixel
};

const Glyph kGlyphs[] = {
    {' ', {0, 0, 0, 0, 0, 0, 0}},
    {'A', {0b01110, 0b10001, 0b10001, 0b11111, 0b10001, 0b10001, 0b10001}},
    {'B', {0b11110, 0b10001, 0b10001, 0b11110, 0b10001, 0b10001, 0b11110}},
    {'C', {0b01110, 0b10001, 0b10000, 0b10000, 0b10000, 0b10001, 0b01110}},
    {'D', {0b11110, 0b10001, 0b10001, 0b10001, 0b10001, 0b10001, 0b11110}},
    {'E', {0b11111, 0b10000, 0b10000, 0b11110, 0b10000, 0b10000, 0b11111}},
    {'F', {0b11111, 0b10000, 0b10000, 0b11110, 0b10000, 0b10000, 0b10000}},
    {'G', {0b01110, 0b10001, 0b10000, 0b10111, 0b10001, 0b10001, 0b01111}},
    {'H', {0b10001, 0b10001, 0b10001, 0b11111, 0b10001, 0b10001, 0b10001}},
    {'I', {0b01110, 0b00100, 0b00100, 0b00100, 0b00100, 0b00100, 0b01110}},
    {'J', {0b00111, 0b00010, 0b00010, 0b00010, 0b00010, 0b10010, 0b01100}},
    {'K', {0b10001, 0b10010, 0b10100, 0b11000, 0b10100, 0b10010, 0b10001}},
    {'L', {0b10000, 0b10000, 0b10000, 0b10000, 0b10000, 0b10000, 0b11111}},
    {'M', {0b10001, 0b11011, 0b10101, 0b10101, 0b10001, 0b10001, 0b10001}},
    {'N', {0b10001, 0b11001, 0b10101, 0b10011, 0b10001, 0b10001, 0b10001}},
    {'O', {0b01110, 0b10001, 0b10001, 0b10001, 0b10001, 0b10001, 0b01110}},
    {'P', {0b11110, 0b10001, 0b10001, 0b11110, 0b10000, 0b10000, 0b10000}},
    {'Q', {0b01110, 0b10001, 0b10001, 0b10001, 0b10101, 0b10010, 0b01101}},
    {'R', {0b11110, 0b10001, 0b10001, 0b11110, 0b10100, 0b10010, 0b10001}},
    {'S', {0b01111, 0b10000, 0b10000, 0b01110, 0b00001, 0b00001, 0b11110}},
    {'T', {0b11111, 0b00100, 0b00100, 0b00100, 0b00100, 0b00100, 0b00100}},
    {'U', {0b10001, 0b10001, 0b10001, 0b10001, 0b10001, 0b10001, 0b01110}},
    {'V', {0b10001, 0b10001, 0b10001, 0b10001, 0b10001, 0b01010, 0b00100}},
    {'W', {0b10001, 0b10001, 0b10001, 0b10101, 0b10101, 0b11011, 0b10001}},
    {'X', {0b10001, 0b10001, 0b01010, 0b00100, 0b01010, 0b10001, 0b10001}},
    {'Y', {0b10001, 0b10001, 0b01010, 0b00100, 0b00100, 0b00100, 0b00100}},
    {'Z', {0b11111, 0b00001, 0b00010, 0b00100, 0b01000, 0b10000, 0b11111}},
    {'0', {0b01110, 0b10001, 0b10011, 0b10101, 0b11001, 0b10001, 0b01110}},
    {'1', {0b00100, 0b01100, 0b00100, 0b00100, 0b00100, 0b00100, 0b01110}},
    {'2', {0b01110, 0b10001, 0b00001, 0b00010, 0b00100, 0b01000, 0b11111}},
    {'3', {0b11110, 0b00001, 0b00001, 0b01110, 0b00001, 0b00001, 0b11110}},
    {'4', {0b00010, 0b00110, 0b01010, 0b10010, 0b11111, 0b00010, 0b00010}},
    {'5', {0b11111, 0b10000, 0b11110, 0b00001, 0b00001, 0b10001, 0b01110}},
    {'6', {0b00110, 0b01000, 0b10000, 0b11110, 0b10001, 0b10001, 0b01110}},
    {'7', {0b11111, 0b00001, 0b00010, 0b00100, 0b01000, 0b01000, 0b01000}},
    {'8', {0b01110, 0b10001, 0b10001, 0b01110, 0b10001, 0b10001, 0b01110}},
    {'9', {0b01110, 0b10001, 0b10001, 0b01111, 0b00001, 0b00010, 0b01100}},
    {'-', {0, 0, 0, 0b11111, 0, 0, 0}},
    {':', {0, 0b00100, 0b00100, 0, 0b00100, 0b00100, 0}},
    {'.', {0, 0, 0, 0, 0, 0b00110, 0b00110}},
    {',', {0, 0, 0, 0, 0b00110, 0b00100, 0b01000}},
    {'!', {0b00100, 0b00100, 0b00100, 0b00100, 0b00100, 0, 0b00100}},
    {'/', {0b00001, 0b00001, 0b00010, 0b00100, 0b01000, 0b10000, 0b10000}},
    {'#', {0b01010, 0b01010, 0b11111, 0b01010, 0b11111, 0b01010, 0b01010}},
    {'+', {0, 0b00100, 0b00100, 0b11111, 0b00100, 0b00100, 0}},
    {'=', {0, 0, 0b11111, 0, 0b11111, 0, 0}},
    {'<', {0b00010, 0b00100, 0b01000, 0b10000, 0b01000, 0b00100, 0b00010}},
    {'>', {0b01000, 0b00100, 0b00010, 0b00001, 0b00010, 0b00100, 0b01000}},
    {'?', {0b01110, 0b10001, 0b00001, 0b00010, 0b00100, 0, 0b00100}},
};

const Glyph& glyphFor(char c) {
    char up = static_cast<char>(std::toupper(static_cast<unsigned char>(c)));
    for (const Glyph& g : kGlyphs) {
        if (g.c == up) return g;
    }
    return kGlyphs[sizeof(kGlyphs) / sizeof(kGlyphs[0]) - 1];  // '?'
}

}  // namespace

int advance(int scale) { return 6 * scale; }
int lineHeight(int scale) { return 9 * scale; }

int textWidth(const std::string& text, int scale) {
    return text.empty() ? 0 : static_cast<int>(text.size()) * advance(scale) - scale;
}

void draw(SDL_Renderer* r, const std::string& text, int x, int y, int scale, SDL_Color color) {
    SDL_SetRenderDrawBlendMode(r, SDL_BLENDMODE_BLEND);
    SDL_SetRenderDrawColor(r, color.r, color.g, color.b, color.a);
    std::vector<SDL_Rect> rects;
    int penX = x;
    for (char c : text) {
        const Glyph& g = glyphFor(c);
        for (int row = 0; row < 7; ++row) {
            for (int col = 0; col < 5; ++col) {
                if (g.rows[row] & (1 << (4 - col))) {
                    rects.push_back(SDL_Rect{penX + col * scale, y + row * scale, scale, scale});
                }
            }
        }
        penX += advance(scale);
    }
    if (!rects.empty()) SDL_RenderFillRects(r, rects.data(), static_cast<int>(rects.size()));
}

int drawWrapped(SDL_Renderer* r, const std::string& text, int x, int y, int maxWidth, int scale, SDL_Color color,
                bool centered) {
    std::vector<std::string> lines;
    std::string line, word;
    auto flushWord = [&]() {
        if (word.empty()) return;
        std::string candidate = line.empty() ? word : line + " " + word;
        if (!line.empty() && textWidth(candidate, scale) > maxWidth) {
            lines.push_back(line);
            line = word;
        } else {
            line = candidate;
        }
        word.clear();
    };
    for (char c : text) {
        if (c == ' ') flushWord();
        else word += c;
    }
    flushWord();
    if (!line.empty()) lines.push_back(line);

    int cy = y;
    for (const std::string& l : lines) {
        int lx = centered ? x + (maxWidth - textWidth(l, scale)) / 2 : x;
        draw(r, l, lx, cy, scale, color);
        cy += lineHeight(scale);
    }
    return cy - y;
}

}  // namespace font
