#pragma once

#include <SDL.h>

#include <string>

#include "PieceArt.hpp"
#include "State.hpp"

inline SDL_Rect squareRect(const Layout& L, bool flipped, chess::Square sq) {
    int col = flipped ? 7 - chess::fileOf(sq) : chess::fileOf(sq);
    int row = flipped ? chess::rankOf(sq) : 7 - chess::rankOf(sq);
    return {L.board.x + col * L.square, L.board.y + row * L.square, L.square, L.square};
}

// Returns NoSquare when (x, y) is outside the board.
inline chess::Square squareAt(const Layout& L, bool flipped, int x, int y) {
    if (!contains(L.board, x, y)) return chess::NoSquare;
    int col = (x - L.board.x) / L.square, row = (y - L.board.y) / L.square;
    if (col > 7 || row > 7) return chess::NoSquare;
    int file = flipped ? 7 - col : col;
    int rank = flipped ? row : 7 - row;
    return chess::makeSquare(file, rank);
}

std::string statusHeadline(const chess::Game& g);
std::string statusDetail(const chess::Game& g);
const char* buttonLabel(int id, const State& st);

// Draws one complete frame.
void drawFrame(SDL_Renderer* r, const State& st, const PieceArt& art);
