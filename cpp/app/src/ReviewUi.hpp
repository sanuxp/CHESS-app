#pragma once

#include <SDL.h>

#include <array>

#include "PieceArt.hpp"
#include "State.hpp"

// The review screen: setup / progress card, then the board with labels, accuracy, evaluation graph and move list.

// What the app must do after a click or key press in the review screen.
enum class ReviewAction { None, Close, SavePgn, OpenPgn };

struct ReviewLayout {
    bool landscape = true;
    int fontScale = 2;
    float scale = 1.0f;
    int pad = 12, gap = 8;

    // Results screen
    SDL_Rect board{};
    int square = 0;
    SDL_Rect panel{};
    SDL_Rect btnBack{}, btnFlip{}, btnArrow{}, btnSave{};
    SDL_Rect accWhite{}, accBlack{}, opening{}, graph{}, detail{};
    std::array<SDL_Rect, 4> nav{};
    SDL_Rect list{};
    int rowHeight = 20;

    // Setup, progress and error card
    SDL_Rect card{};
    std::array<SDL_Rect, 3> depth{};
    SDL_Rect btnStart{}, btnOpen{}, btnCancel{}, btnCardBack{}, progress{};
};

ReviewLayout computeReviewLayout(const Layout& L, const ReviewSession& s);

void drawReview(SDL_Renderer* r, const State& st, const PieceArt& art);

// Input. Positions are renderer pixels. The returned action is performed by the app (file dialogs, closing).
ReviewAction reviewPress(State& st, int x, int y);
void reviewHover(State& st, int x, int y);
ReviewAction reviewKey(State& st, SDL_Keycode key);
void reviewWheel(State& st, int wheelY);

// Scrolls the move list so the selected move is visible.
void reviewEnsureVisible(State& st);
