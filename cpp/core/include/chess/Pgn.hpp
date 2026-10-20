#pragma once

#include <optional>
#include <string>
#include <vector>

#include "chess/Game.hpp"

namespace chess {

// Standard algebraic notation of `m` played from `before`, including +/# suffix.
std::string moveToSAN(const Board& before, const Move& m);

// Finds the legal move in `board` that `san` denotes (check/mate marks and !? are ignored).
std::optional<Move> moveFromSAN(const Board& board, const std::string& san);

struct PgnHeaders {
    std::string event = "Casual Game";
    std::string site = "Local";
    std::string date = "????.??.??";  // YYYY.MM.DD
    std::string round = "-";
    std::string white = "White";
    std::string black = "Black";
};

// Extra information written after a move: a numeric annotation glyph (1 = !, 2 = ?, 3 = !!, 4 = ??, 6 = ?!)
// and a comment, for example a game review's evaluations.
struct PlyAnnotation {
    int nag = 0;
    std::string comment;
};

// Full PGN text (seven tag roster, FEN/SetUp tags for custom starts, wrapped movetext, result).
// Importable by Lichess, Chess.com, Arena, Scid and other engine front ends.
// `annotations`, if given, has one entry per move.
std::string toPGN(const Game& game, const PgnHeaders& headers = PgnHeaders{},
                  const std::vector<PlyAnnotation>* annotations = nullptr);

}  // namespace chess
