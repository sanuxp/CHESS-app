#pragma once

// Move scoring and labelling: pure logic, no engine. Follows docs/REVIEW.md, which
// docs/testdata/review.json pins down (the Dart version in flutter/lib/review/classify.dart does the same).

#include <string>
#include <vector>

namespace chess {

// An engine score from the point of view of the side to move.
struct EngineScore {
    bool mate = false;  // `value` is moves to mate (negative: the side to move is being mated) instead of centipawns
    int value = 0;

    static EngineScore Cp(int cp) { return {false, cp}; }
    static EngineScore Mate(int moves) { return {true, moves}; }
    EngineScore flipped() const { return {mate, -value}; }
    std::string str() const;  // "+0.27", "-1.50", "M3", "-M2"
};

// One line of engine output: its first move, score and full variation (UCI moves).
struct EngineLine {
    std::string move;
    EngineScore score;
    std::vector<std::string> pv;
};

enum class MoveLabel { Forced, Book, Brilliant, Great, Best, Excellent, Good, Miss, Inaccuracy, Mistake, Blunder };

const char* labelName(MoveLabel l);    // "brilliant" (matches the test data)
const char* labelTitle(MoveLabel l);   // "Brilliant"
const char* labelSymbol(MoveLabel l);  // "!!", "!", "?!", "?", "??" or ""
int labelNag(MoveLabel l);             // PGN numeric annotation glyph, 0 for none
const char* labelMeaning(MoveLabel l); // "a brilliant sacrifice"

// All thresholds in one place (win-percentage points unless noted). Defaults match docs/REVIEW.md.
struct ReviewConfig {
    double brilliantMaxLoss = 2, brilliantMinSacrifice = 2, brilliantMaxWinBefore = 80, brilliantMinWinAfter = 45;
    double brilliantMateMinSacrifice = 5;
    double greatMaxLoss = 1, greatMinGap = 15, bookMaxLoss = 10;
    double bestMaxLoss = 0.5, excellentMaxLoss = 2, goodMaxLoss = 5;
    double missOpponentLoss = 10, missMinWinBefore = 60, missMinLoss = 5;
    double inaccuracyMaxLoss = 10, mistakeMaxLoss = 20;
};

// Chance of winning (0-100) for the side the score is given for.
double winPercent(const EngineScore& s);
// Accuracy (0-100) of a move that gave away `loss` win-percentage points.
double moveAccuracy(double loss);
// Mean of the arithmetic and harmonic means of the per-move accuracies; 100 when there are none.
double gameAccuracy(const std::vector<double>& accuracies);

enum class AfterResult { None, Checkmate, Stalemate };

// Everything needed to label one move (docs/REVIEW.md section 1).
struct PlyInput {
    std::string played;                // UCI
    std::vector<EngineLine> linesBefore;  // best first, scores from the mover's view
    bool hasAfterBest = false;
    EngineScore afterBest;             // best line after the move, from the opponent's view
    AfterResult afterResult = AfterResult::None;
    int legalMoves = 30;
    bool book = false;
    bool recapture = false;
    double sacrificed = 0;
    bool hasOpponentPrevLoss = false;
    double opponentPrevLoss = 0;
};

struct PlyScore {
    MoveLabel label = MoveLabel::Best;
    double loss = 0, winBefore = 0, winAfter = 0, accuracy = 0;
};

PlyScore classifyMove(const PlyInput& in, const ReviewConfig& config = ReviewConfig{});

}  // namespace chess
