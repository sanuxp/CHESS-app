#pragma once

#include <cstdint>
#include <vector>

#include "chess/Board.hpp"

namespace chess {

// True if any piece of colour `by` attacks square `sq` (regardless of what stands on it).
bool isSquareAttacked(const Board& board, Square sq, Color by);

// True if the king of colour `c` is attacked.
bool isInCheck(const Board& board, Color c);

// Moves that obey piece movement but may leave the own king in check.
void generatePseudoLegal(const Board& board, std::vector<Move>& out);

// Fully legal moves for the side to move. Promotions appear as four separate moves.
std::vector<Move> generateLegal(const Board& board);

// Counts leaf nodes of the legal move tree to the given depth (standard correctness benchmark).
uint64_t perft(const Board& board, int depth);

}  // namespace chess
