#include "chess/Review.hpp"

#include <cstdio>

#include "chess/Material.hpp"
#include "chess/MoveGen.hpp"

namespace chess {

std::map<MoveLabel, int> GameReview::counts(Color color) const {
    std::map<MoveLabel, int> m;
    for (const PlyReview& p : plies) {
        if (p.mover == color) ++m[p.label];
    }
    return m;
}

GameReview reviewGame(const Game& game, const std::vector<PositionEval>& evals, const OpeningBook* book,
                      const ReviewConfig& config) {
    const auto& history = game.history();
    GameReview out;
    std::vector<double> accuracies[2];
    bool inBook = book != nullptr;
    const Opening* opening = nullptr;
    bool havePrevLoss = false;
    double previousLoss = 0;

    for (size_t i = 0; i < history.size(); ++i) {
        const HistoryEntry& entry = history[i];
        const Board& before = entry.before;
        const Color mover = before.sideToMove;
        const std::string played = [&] {
            std::string s = squareName(entry.move.from) + squareName(entry.move.to);
            switch (entry.move.promo) {
                case PieceType::Queen: s += 'q'; break;
                case PieceType::Rook: s += 'r'; break;
                case PieceType::Bishop: s += 'b'; break;
                case PieceType::Knight: s += 'n'; break;
                default: break;
            }
            return s;
        }();
        const PositionEval& here = evals[i];
        const PositionEval& after = evals[i + 1];
        const bool isLast = i + 1 == history.size();
        const Board& afterBoard = i + 1 < history.size() ? history[i + 1].before : game.board();

        const Opening* matched = nullptr;
        if (inBook) {
            if (book->contains(afterBoard)) {
                matched = book->nameOf(afterBoard);
                if (matched) opening = matched;
            } else {
                inBook = false;
            }
        }

        AfterResult result = AfterResult::None;
        if (isLast && game.status() == GameStatus::Checkmate) result = AfterResult::Checkmate;
        else if (isLast && isGameOver(game.status())) result = AfterResult::Stalemate;  // any draw counts 50%

        PlyInput input;
        input.played = played;
        input.linesBefore = here.lines;
        input.hasAfterBest = !after.terminal();
        if (!after.terminal()) input.afterBest = after.lines.front().score;
        input.afterResult = after.terminal() && result == AfterResult::None ? AfterResult::Stalemate : result;
        input.legalMoves = static_cast<int>(generateLegal(before).size());
        input.book = inBook;
        if (i > 0) {
            const Move& prev = history[i - 1].move;
            input.recapture = prev.isCapture() && entry.move.isCapture() && prev.to == entry.move.to;
        }
        input.sacrificed =
            sacrificedValue(before, played, after.terminal() ? std::vector<std::string>{} : after.lines.front().pv);
        input.hasOpponentPrevLoss = havePrevLoss;
        input.opponentPrevLoss = previousLoss;

        PlyScore score = classifyMove(input, config);
        if (input.book && score.label != MoveLabel::Book) inBook = false;  // a bad "book" move ends the book
        previousLoss = score.loss;
        havePrevLoss = true;
        accuracies[mover == Color::White ? 0 : 1].push_back(score.accuracy);

        PlyReview ply;
        ply.ply = static_cast<int>(i) + 1;
        ply.mover = mover;
        ply.san = entry.san;
        ply.played = played;
        ply.label = score.label;
        ply.loss = score.loss;
        ply.winBefore = score.winBefore;
        ply.winAfter = score.winAfter;
        ply.accuracy = score.accuracy;
        ply.bestMove = here.lines.front().move;
        for (const Move& m : generateLegal(before)) {
            std::string u = squareName(m.from) + squareName(m.to);
            switch (m.promo) {
                case PieceType::Queen: u += 'q'; break;
                case PieceType::Rook: u += 'r'; break;
                case PieceType::Bishop: u += 'b'; break;
                case PieceType::Knight: u += 'n'; break;
                default: break;
            }
            if (u == ply.bestMove) {
                ply.bestSan = moveToSAN(before, m);
                break;
            }
        }
        if (!after.terminal()) {
            ply.hasScoreAfter = true;
            const EngineScore& s = after.lines.front().score;  // opponent's view
            ply.scoreAfter = mover == Color::White ? s.flipped() : s;
        }
        out.plies.push_back(std::move(ply));
    }

    // White's winning chances at every position, for the graph.
    for (size_t i = 0; i <= history.size(); ++i) {
        const Board& board = i < history.size() ? history[i].before : game.board();
        const PositionEval& e = evals[i];
        if (e.terminal()) {
            out.whiteWinChance.push_back(i == history.size() && game.status() == GameStatus::Checkmate
                                             ? (board.sideToMove == Color::White ? 0 : 100)
                                             : 50);
        } else {
            double w = winPercent(e.lines.front().score);
            out.whiteWinChance.push_back(board.sideToMove == Color::White ? w : 100 - w);
        }
    }

    out.accuracyWhite = gameAccuracy(accuracies[0]);
    out.accuracyBlack = gameAccuracy(accuracies[1]);
    if (opening) {
        out.opening = opening->name;
        out.openingEco = opening->eco;
    }
    return out;
}

std::string evalTag(const EngineScore& s) {
    char buf[40];
    if (s.mate) std::snprintf(buf, sizeof(buf), "[%%eval #%d]", s.value);
    else std::snprintf(buf, sizeof(buf), "[%%eval %.2f]", s.value / 100.0);
    return buf;
}

std::vector<PlyAnnotation> reviewAnnotations(const GameReview& review) {
    std::vector<PlyAnnotation> notes;
    for (const PlyReview& p : review.plies) {
        PlyAnnotation n;
        n.nag = labelNag(p.label);
        if (p.hasScoreAfter) n.comment = evalTag(p.scoreAfter) + " ";
        n.comment += labelTitle(p.label);
        if (p.hasBetterMove()) n.comment += ". Best: " + p.bestSan;
        notes.push_back(std::move(n));
    }
    return notes;
}

}  // namespace chess
