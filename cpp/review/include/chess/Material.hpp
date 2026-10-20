#pragma once

#include <string>
#include <vector>

#include "chess/Board.hpp"

namespace chess {

// Applies a UCI move (e2e4, e7e8q) if it is legal in `board`; returns false and leaves the board alone otherwise.
bool applyUci(Board& board, const std::string& uci);

// Material in pawns the mover gives up by playing `played` from `before`, judged over the move and the first
// three moves of the engine's variation `pv` that follows it. Trades score 0 (docs/REVIEW.md section 4).
double sacrificedValue(const Board& before, const std::string& played, const std::vector<std::string>& pv);

}  // namespace chess
