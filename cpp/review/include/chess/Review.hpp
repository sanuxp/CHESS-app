#pragma once

#include <map>
#include <string>
#include <vector>

#include "chess/Classify.hpp"
#include "chess/Game.hpp"
#include "chess/Openings.hpp"
#include "chess/Pgn.hpp"

namespace chess {

// What the engine said about one position: its best lines (best first, scores from the side to move's view).
// Empty for finished games (checkmate, stalemate, draws).
struct PositionEval {
    std::vector<EngineLine> lines;
    bool terminal() const { return lines.empty(); }
};

struct PlyReview {
    int ply = 0;  // 1-based
    Color mover = Color::White;
    std::string san;
    std::string played;  // UCI
    MoveLabel label = MoveLabel::Best;
    double loss = 0, winBefore = 0, winAfter = 0, accuracy = 0;
    std::string bestMove;  // engine's preferred move, UCI
    std::string bestSan;
    bool hasScoreAfter = false;
    EngineScore scoreAfter;  // from White's point of view
    // True when the engine had a different move it liked better.
    bool hasBetterMove() const { return !bestMove.empty() && bestMove != played && loss > 0.5; }
};

struct GameReview {
    std::vector<PlyReview> plies;
    double accuracyWhite = 100, accuracyBlack = 100;
    std::string opening, openingEco;    // deepest opening matched, e.g. "Ruy Lopez: Morphy Defense"
    std::vector<double> whiteWinChance; // White's win% at positions 0..n, for the graph

    std::map<MoveLabel, int> counts(Color color) const;
};

// Scores every move of `game`. `evals` has one entry per position: before move 1, ..., after the last move.
GameReview reviewGame(const Game& game, const std::vector<PositionEval>& evals, const OpeningBook* book = nullptr,
                      const ReviewConfig& config = ReviewConfig{});

// "[%eval 0.27]" / "[%eval #3]": the tag Lichess and other tools read from PGN comments.
std::string evalTag(const EngineScore& s);

// One PGN annotation per move: the standard glyph and a comment with evaluation, label and preferred move.
std::vector<PlyAnnotation> reviewAnnotations(const GameReview& review);

}  // namespace chess
