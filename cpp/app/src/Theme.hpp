#pragma once

#include <SDL.h>

#include <array>
#include <string>

// Customization shared with the other stacks, see docs/THEMES.md.

struct BoardTheme {
    const char* name;
    SDL_Color light, dark;
};

enum class PieceMode { Hatch, Flat, Gradient };

struct PieceSet {
    const char* name;
    PieceMode mode;
    // Hatch: base + line colour. Flat: base only. Gradient: base = lit (left) edge, line = shadow (right) edge.
    SDL_Color whiteBase, whiteLine, whiteOutline;
    SDL_Color blackBase, blackLine, blackOutline;
};

struct UiTheme {
    const char* name;
    SDL_Color background, panel, text, muted, button, buttonHover, buttonDisabled, buttonText, buttonTextDisabled;
};

constexpr int kBoardThemeCount = 8;
constexpr int kPieceSetCount = 5;
constexpr int kUiThemeCount = 4;

const std::array<BoardTheme, kBoardThemeCount>& boardThemes();
const std::array<PieceSet, kPieceSetCount>& pieceSets();
const std::array<UiTheme, kUiThemeCount>& uiThemes();

struct Settings {
    int board = 0;
    int pieces = 0;
    int ui = 0;
    bool coordinates = true;
    bool hints = true;
    bool animations = true;

    const BoardTheme& boardTheme() const { return boardThemes()[board]; }
    const PieceSet& pieceSet() const { return pieceSets()[pieces]; }
    const UiTheme& uiTheme() const { return uiThemes()[ui]; }

    // Stored as a small key=value text file in the app's preferences folder. Unknown names fall back to defaults.
    static Settings load();
    void save() const;
};
