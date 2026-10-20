#include "chess/Classify.hpp"

#include <algorithm>
#include <cmath>
#include <cstdio>

namespace chess {

std::string EngineScore::str() const {
    char buf[32];
    if (mate) {
        std::snprintf(buf, sizeof(buf), value > 0 ? "M%d" : "-M%d", std::abs(value));
    } else {
        std::snprintf(buf, sizeof(buf), "%+.2f", value / 100.0);
    }
    return buf;
}

const char* labelName(MoveLabel l) {
    switch (l) {
        case MoveLabel::Forced: return "forced";
        case MoveLabel::Book: return "book";
        case MoveLabel::Brilliant: return "brilliant";
        case MoveLabel::Great: return "great";
        case MoveLabel::Best: return "best";
        case MoveLabel::Excellent: return "excellent";
        case MoveLabel::Good: return "good";
        case MoveLabel::Miss: return "miss";
        case MoveLabel::Inaccuracy: return "inaccuracy";
        case MoveLabel::Mistake: return "mistake";
        case MoveLabel::Blunder: return "blunder";
    }
    return "";
}

const char* labelTitle(MoveLabel l) {
    switch (l) {
        case MoveLabel::Forced: return "Forced";
        case MoveLabel::Book: return "Book";
        case MoveLabel::Brilliant: return "Brilliant";
        case MoveLabel::Great: return "Great";
        case MoveLabel::Best: return "Best";
        case MoveLabel::Excellent: return "Excellent";
        case MoveLabel::Good: return "Good";
        case MoveLabel::Miss: return "Miss";
        case MoveLabel::Inaccuracy: return "Inaccuracy";
        case MoveLabel::Mistake: return "Mistake";
        case MoveLabel::Blunder: return "Blunder";
    }
    return "";
}

const char* labelSymbol(MoveLabel l) {
    switch (l) {
        case MoveLabel::Brilliant: return "!!";
        case MoveLabel::Great: return "!";
        case MoveLabel::Inaccuracy: return "?!";
        case MoveLabel::Mistake: return "?";
        case MoveLabel::Blunder: return "??";
        default: return "";
    }
}

int labelNag(MoveLabel l) {
    switch (l) {
        case MoveLabel::Brilliant: return 3;
        case MoveLabel::Great: return 1;
        case MoveLabel::Inaccuracy: return 6;
        case MoveLabel::Mistake: return 2;
        case MoveLabel::Blunder: return 4;
        default: return 0;
    }
}

const char* labelMeaning(MoveLabel l) {
    switch (l) {
        case MoveLabel::Brilliant: return "a brilliant sacrifice";
        case MoveLabel::Great: return "a great find, the only good move";
        case MoveLabel::Best: return "the best move";
        case MoveLabel::Excellent: return "an excellent move";
        case MoveLabel::Good: return "a good move";
        case MoveLabel::Book: return "an opening book move";
        case MoveLabel::Forced: return "the only legal move";
        case MoveLabel::Inaccuracy: return "an inaccuracy";
        case MoveLabel::Miss: return "a missed chance";
        case MoveLabel::Mistake: return "a mistake";
        case MoveLabel::Blunder: return "a blunder";
    }
    return "";
}

double winPercent(const EngineScore& s) {
    if (s.mate) return s.value > 0 ? 100.0 : 0.0;
    int cp = std::clamp(s.value, -1000, 1000);
    return 50 + 50 * (2 / (1 + std::exp(-0.00368208 * cp)) - 1);
}

double moveAccuracy(double loss) { return std::clamp(103.1668 * std::exp(-0.04354 * loss) - 3.1669, 0.0, 100.0); }

double gameAccuracy(const std::vector<double>& accuracies) {
    if (accuracies.empty()) return 100;
    double sum = 0, inverse = 0;
    for (double a : accuracies) {
        sum += a;
        inverse += 1 / std::max(a, 0.01);
    }
    double arithmetic = sum / accuracies.size();
    double harmonic = accuracies.size() / inverse;
    return (arithmetic + harmonic) / 2;
}

PlyScore classifyMove(const PlyInput& p, const ReviewConfig& c) {
    const auto& lines = p.linesBefore;
    PlyScore r;
    r.winBefore = winPercent(lines.front().score);
    switch (p.afterResult) {
        case AfterResult::Checkmate: r.winAfter = 100; break;
        case AfterResult::Stalemate: r.winAfter = 50; break;
        case AfterResult::None: r.winAfter = 100 - winPercent(p.afterBest); break;
    }
    r.loss = std::max(0.0, r.winBefore - r.winAfter);
    r.accuracy = moveAccuracy(r.loss);

    const bool playedTop = p.played == lines.front().move;
    const bool mateForMover = lines.front().score.mate && lines.front().score.value > 0;

    if (p.legalMoves == 1) {
        r.label = MoveLabel::Forced;
    } else if (p.book && r.loss <= c.bookMaxLoss) {
        r.label = MoveLabel::Book;
    } else if (r.loss <= c.brilliantMaxLoss &&
               ((p.sacrificed >= c.brilliantMinSacrifice && r.winBefore <= c.brilliantMaxWinBefore &&
                 r.winAfter >= c.brilliantMinWinAfter) ||
                (p.sacrificed >= c.brilliantMateMinSacrifice && mateForMover &&
                 p.afterResult != AfterResult::Checkmate))) {
        r.label = MoveLabel::Brilliant;
    } else if (playedTop && r.loss <= c.greatMaxLoss && lines.size() >= 2 &&
               winPercent(lines[0].score) - winPercent(lines[1].score) >= c.greatMinGap && !p.recapture &&
               p.afterResult != AfterResult::Checkmate) {
        r.label = MoveLabel::Great;
    } else if (playedTop || r.loss <= c.bestMaxLoss) {
        r.label = MoveLabel::Best;
    } else if (r.loss <= c.excellentMaxLoss) {
        r.label = MoveLabel::Excellent;
    } else if (r.loss <= c.goodMaxLoss) {
        r.label = MoveLabel::Good;
    } else if (p.hasOpponentPrevLoss && p.opponentPrevLoss > c.missOpponentLoss && r.winBefore >= c.missMinWinBefore &&
               r.loss >= c.missMinLoss && !(r.loss > c.mistakeMaxLoss && r.winAfter < 50)) {
        r.label = MoveLabel::Miss;
    } else if (r.loss <= c.inaccuracyMaxLoss) {
        r.label = MoveLabel::Inaccuracy;
    } else if (r.loss <= c.mistakeMaxLoss) {
        r.label = MoveLabel::Mistake;
    } else {
        r.label = MoveLabel::Blunder;
    }
    return r;
}

}  // namespace chess
