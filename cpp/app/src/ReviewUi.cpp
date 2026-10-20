#include "ReviewUi.hpp"

#include <algorithm>
#include <cmath>
#include <cstdio>

#include "Font.hpp"
#include "Renderer.hpp"

using namespace chess;

namespace {

// ---------------------------------------------------------------- small drawing helpers

void setColor(SDL_Renderer* r, SDL_Color c) {
    SDL_SetRenderDrawBlendMode(r, c.a == 255 ? SDL_BLENDMODE_NONE : SDL_BLENDMODE_BLEND);
    SDL_SetRenderDrawColor(r, c.r, c.g, c.b, c.a);
}

void fillRect(SDL_Renderer* r, SDL_Rect rect, SDL_Color c) {
    setColor(r, c);
    SDL_RenderFillRect(r, &rect);
}

void fillCircle(SDL_Renderer* r, int cx, int cy, int radius, SDL_Color c) {
    setColor(r, c);
    for (int dy = -radius; dy <= radius; ++dy) {
        int dx = static_cast<int>(std::sqrt(static_cast<double>(radius * radius - dy * dy)));
        SDL_RenderDrawLine(r, cx - dx, cy + dy, cx + dx, cy + dy);
    }
}

void fillTriangles(SDL_Renderer* r, const std::vector<SDL_FPoint>& pts, SDL_Color c) {
    setColor(r, c);
    std::vector<SDL_Vertex> v;
    for (const SDL_FPoint& p : pts) {
        SDL_Vertex vert{};
        vert.position = p;
        vert.color = c;
        v.push_back(vert);
    }
    SDL_RenderGeometry(r, nullptr, v.data(), static_cast<int>(v.size()), nullptr, 0);
}

SDL_Rect inset(SDL_Rect r, int by) { return {r.x + by, r.y + by, r.w - 2 * by, r.h - 2 * by}; }

SDL_Rect cellRect(const ReviewLayout& R, bool flipped, Square sq) {
    int col = flipped ? 7 - fileOf(sq) : fileOf(sq);
    int row = flipped ? rankOf(sq) : 7 - rankOf(sq);
    return {R.board.x + col * R.square, R.board.y + row * R.square, R.square, R.square};
}

void drawPiece(SDL_Renderer* r, const PieceArt& art, Piece p, SDL_Rect cell) {
    SDL_Texture* tex = art.get(p);
    if (!tex) return;
    SDL_Rect dst = inset(cell, std::max(1, cell.w / 16));
    SDL_RenderCopy(r, tex, nullptr, &dst);
}

SDL_Color labelColor(MoveLabel l) {
    switch (l) {
        case MoveLabel::Brilliant: return {27, 170, 166, 255};
        case MoveLabel::Great: return {92, 139, 176, 255};
        case MoveLabel::Best: return {129, 182, 76, 255};
        case MoveLabel::Excellent: return {150, 188, 75, 255};
        case MoveLabel::Good: return {163, 177, 138, 255};
        case MoveLabel::Book: return {168, 136, 101, 255};
        case MoveLabel::Forced: return {138, 138, 138, 255};
        case MoveLabel::Inaccuracy: return {247, 198, 49, 255};
        case MoveLabel::Mistake: return {229, 143, 42, 255};
        case MoveLabel::Miss: return {224, 90, 79, 255};
        case MoveLabel::Blunder: return {202, 52, 49, 255};
    }
    return {138, 138, 138, 255};
}

void drawStar(SDL_Renderer* r, float cx, float cy, float outer, SDL_Color c) {
    const float inner = outer * 0.45f;
    std::vector<SDL_FPoint> tris;
    for (int i = 0; i < 10; ++i) {
        float a0 = -3.14159265f / 2 + i * 3.14159265f / 5, a1 = a0 + 3.14159265f / 5;
        float r0 = i % 2 == 0 ? outer : inner, r1 = i % 2 == 0 ? inner : outer;
        tris.push_back({cx, cy});
        tris.push_back({cx + std::cos(a0) * r0, cy + std::sin(a0) * r0});
        tris.push_back({cx + std::cos(a1) * r1, cy + std::sin(a1) * r1});
    }
    fillTriangles(r, tris, c);
}

// A coloured disc with the label's mark: text for !! ! ?! ? ?? + > X, shapes for best, good and book.
void drawBadge(SDL_Renderer* r, int cx, int cy, int radius, MoveLabel label) {
    SDL_Color white{255, 255, 255, 255};
    fillCircle(r, cx, cy, radius, labelColor(label));
    auto text = [&](const char* t) {
        int sc = std::max(1, radius * 2 / 14);
        while (sc > 1 && font::textWidth(t, sc) > radius * 2 - 2) --sc;
        font::draw(r, t, cx - font::textWidth(t, sc) / 2, cy - (7 * sc) / 2, sc, white);
    };
    switch (label) {
        case MoveLabel::Brilliant: text("!!"); break;
        case MoveLabel::Great: text("!"); break;
        case MoveLabel::Inaccuracy: text("?!"); break;
        case MoveLabel::Mistake: text("?"); break;
        case MoveLabel::Blunder: text("??"); break;
        case MoveLabel::Excellent: text("+"); break;
        case MoveLabel::Forced: text(">"); break;
        case MoveLabel::Miss: text("X"); break;
        case MoveLabel::Best: drawStar(r, static_cast<float>(cx), static_cast<float>(cy) + 0.5f, radius * 0.72f, white); break;
        case MoveLabel::Good: {  // check mark
            setColor(r, white);
            int t = std::max(1, radius / 5);
            for (int k = 0; k < t; ++k) {
                SDL_RenderDrawLine(r, cx - radius / 2, cy + k, cx - radius / 6, cy + radius / 3 + k);
                SDL_RenderDrawLine(r, cx - radius / 6, cy + radius / 3 + k, cx + radius / 2, cy - radius / 3 + k);
            }
            break;
        }
        case MoveLabel::Book: {  // open book: two pages
            int w = std::max(2, radius / 2), h = std::max(3, radius);
            fillRect(r, SDL_Rect{cx - w - 1, cy - h / 2, w, h}, white);
            fillRect(r, SDL_Rect{cx + 1, cy - h / 2, w, h}, white);
            break;
        }
    }
}

void drawButton(SDL_Renderer* r, const State& st, SDL_Rect rc, const std::string& label, bool hover, bool disabled,
                int scale) {
    const UiTheme& ui = st.settings.uiTheme();
    fillRect(r, rc, disabled ? ui.buttonDisabled : hover ? ui.buttonHover : ui.button);
    int sc = scale;
    while (sc > 1 && font::textWidth(label, sc) > rc.w - 6) --sc;
    font::draw(r, label, rc.x + (rc.w - font::textWidth(label, sc)) / 2, rc.y + (rc.h - 7 * sc) / 2, sc,
               disabled ? ui.buttonTextDisabled : ui.buttonText);
}

enum Hover {
    HNone = -1, HBack = 1, HFlip, HArrow, HSave, HNav0 = 10, HDepth0 = 20, HStart = 30, HOpen, HCancel
};

std::string fmt(const char* f, double v) {
    char buf[48];
    std::snprintf(buf, sizeof(buf), f, v);
    return buf;
}

// ---------------------------------------------------------------- results screen

void drawBoard(SDL_Renderer* r, const State& st, const ReviewSession& s, const ReviewLayout& R, const PieceArt& art) {
    const SDL_Color light = st.settings.boardTheme().light, dark = st.settings.boardTheme().dark;
    const Board b = s.positionAt(s.cursor);
    for (Square sq = 0; sq < 64; ++sq) {
        bool l = (fileOf(sq) + rankOf(sq)) % 2 == 1;
        fillRect(r, cellRect(R, s.flipped, sq), l ? light : dark);
    }
    const PlyReview* ply = s.cursor > 0 ? &s.review->plies[static_cast<size_t>(s.cursor) - 1] : nullptr;
    Move played;
    if (s.cursor > 0) played = s.game.history()[static_cast<size_t>(s.cursor) - 1].move;
    if (ply) {
        SDL_Color tint{205, 210, 106, 120};
        fillRect(r, cellRect(R, s.flipped, played.from), tint);
        fillRect(r, cellRect(R, s.flipped, played.to), tint);
    }

    int cs = std::max(1, R.square / 44);
    for (int i = 0; st.settings.coordinates && i < 8; ++i) {
        Square fileSq = makeSquare(i, s.flipped ? 7 : 0);
        SDL_Rect cell = cellRect(R, s.flipped, fileSq);
        bool l = (fileOf(fileSq) + rankOf(fileSq)) % 2 == 1;
        font::draw(r, std::string(1, static_cast<char>('a' + i)), cell.x + cell.w - font::advance(cs) - cs,
                   cell.y + cell.h - 8 * cs, cs, l ? dark : light);
        Square rankSq = makeSquare(s.flipped ? 7 : 0, i);
        SDL_Rect rc = cellRect(R, s.flipped, rankSq);
        bool lr = (fileOf(rankSq) + rankOf(rankSq)) % 2 == 1;
        font::draw(r, std::string(1, static_cast<char>('1' + i)), rc.x + 2 * cs, rc.y + 2 * cs, cs, lr ? dark : light);
    }

    for (Square sq = 0; sq < 64; ++sq) {
        Piece p = b.pieceAt(sq);
        if (!p.isNone()) drawPiece(r, art, p, cellRect(R, s.flipped, sq));
    }

    // The engine's preferred move, when the played one was worse.
    if (ply && s.showBest && ply->hasBetterMove() && ply->bestMove.size() >= 4) {
        Square from = squareFromName(ply->bestMove.substr(0, 2)), to = squareFromName(ply->bestMove.substr(2, 2));
        if (from != NoSquare && to != NoSquare) {
            SDL_Rect a = cellRect(R, s.flipped, from), c = cellRect(R, s.flipped, to);
            float x0 = a.x + a.w / 2.0f, y0 = a.y + a.h / 2.0f, x1 = c.x + c.w / 2.0f, y1 = c.y + c.h / 2.0f;
            float dx = x1 - x0, dy = y1 - y0, len = std::sqrt(dx * dx + dy * dy);
            if (len > 1) {
                float ux = dx / len, uy = dy / len, nx = -uy, ny = ux;
                float cell = static_cast<float>(R.square);
                float width = cell * 0.17f, head = cell * 0.36f;
                float tx = x0 + ux * cell * 0.3f, ty = y0 + uy * cell * 0.3f;
                float kx = x1 - ux * head, ky = y1 - uy * head;
                SDL_Color green{129, 182, 76, 200};
                std::vector<SDL_FPoint> tris = {
                    {tx + nx * width / 2, ty + ny * width / 2}, {kx + nx * width / 2, ky + ny * width / 2},
                    {kx - nx * width / 2, ky - ny * width / 2}, {tx + nx * width / 2, ty + ny * width / 2},
                    {kx - nx * width / 2, ky - ny * width / 2}, {tx - nx * width / 2, ty - ny * width / 2},
                    {kx + nx * head * 0.6f, ky + ny * head * 0.6f}, {x1, y1}, {kx - nx * head * 0.6f, ky - ny * head * 0.6f},
                };
                fillTriangles(r, tris, green);
            }
        }
    }

    // Label badge on the destination square of the move just played.
    if (ply) {
        SDL_Rect to = cellRect(R, s.flipped, played.to);
        int radius = std::max(6, R.square * 24 / 100);
        int cx = to.x + to.w - radius, cy = to.y + radius;
        fillCircle(r, cx, cy + 1, radius, SDL_Color{0, 0, 0, 90});
        fillCircle(r, cx, cy, radius + 1, SDL_Color{255, 255, 255, 255});
        drawBadge(r, cx, cy, radius, ply->label);
    }
}

void drawGraph(SDL_Renderer* r, const State& st, const ReviewSession& s, const ReviewLayout& R) {
    const GameReview& rv = *s.review;
    SDL_Rect g = R.graph;
    fillRect(r, g, SDL_Color{43, 43, 48, 255});
    const auto& v = rv.whiteWinChance;
    if (v.size() < 2 || g.w < 2) return;
    auto valueAt = [&](double t) {
        double pos = t * (v.size() - 1);
        size_t i0 = static_cast<size_t>(pos), i1 = std::min(i0 + 1, v.size() - 1);
        return v[i0] + (v[i1] - v[i0]) * (pos - i0);
    };
    for (int x = 0; x < g.w; ++x) {
        double win = valueAt(static_cast<double>(x) / (g.w - 1));
        int y = static_cast<int>(g.h * (1 - win / 100));
        fillRect(r, SDL_Rect{g.x + x, g.y + y, 1, g.h - y}, SDL_Color{237, 237, 237, 255});
        fillRect(r, SDL_Rect{g.x + x, g.y + y, 1, 1}, SDL_Color{154, 154, 165, 255});
    }
    fillRect(r, SDL_Rect{g.x, g.y + g.h / 2, g.w, 1}, SDL_Color{255, 138, 42, 110});
    for (const PlyReview& p : rv.plies) {
        switch (p.label) {
            case MoveLabel::Brilliant: case MoveLabel::Great: case MoveLabel::Mistake: case MoveLabel::Miss: case MoveLabel::Blunder: {
                int x = g.x + static_cast<int>(static_cast<double>(p.ply) / (v.size() - 1) * (g.w - 1));
                int y = g.y + static_cast<int>(g.h * (1 - v[static_cast<size_t>(p.ply)] / 100));
                fillCircle(r, x, y, std::max(2, R.fontScale + 1), labelColor(p.label));
                break;
            }
            default: break;
        }
    }
    int cx = g.x + static_cast<int>(static_cast<double>(s.cursor) / (v.size() - 1) * (g.w - 1));
    fillRect(r, SDL_Rect{cx, g.y, 2, g.h}, SDL_Color{255, 138, 42, 255});
    (void)st;
}

void drawList(SDL_Renderer* r, const State& st, const ReviewSession& s, const ReviewLayout& R) {
    const UiTheme& ui = st.settings.uiTheme();
    const GameReview& rv = *s.review;
    fillRect(r, R.list, SDL_Color{ui.background.r, ui.background.g, ui.background.b, 160});

    const bool firstIsWhite = s.game.initialBoard().sideToMove == Color::White;
    const int plies = static_cast<int>(rv.plies.size());
    const int rows = (plies + (firstIsWhite ? 0 : 1) + 1) / 2;
    const int numW = R.fontScale * 6 * 3 + 4;
    const int barW = std::max(4, R.fontScale * 2);
    const int cellW = (R.list.w - numW - barW) / 2;
    const int fs = R.fontScale;

    SDL_Rect clip = R.list;
    SDL_RenderSetClipRect(r, &clip);
    for (int row = s.listScroll; row < rows; ++row) {
        int y = R.list.y + (row - s.listScroll) * R.rowHeight;
        if (y >= R.list.y + R.list.h) break;
        int number = s.game.initialBoard().fullmoveNumber + row;
        font::drawExact(r, std::to_string(number) + ".", R.list.x + 4, y + (R.rowHeight - 7 * fs) / 2, fs, ui.muted);
        for (int side = 0; side < 2; ++side) {
            int ply = row * 2 + side + (firstIsWhite ? 1 : 0);  // 1-based; 0 = the empty White cell of a Black-first game
            SDL_Rect cell{R.list.x + numW + side * cellW, y, cellW, R.rowHeight};
            if (ply < 1 || ply > plies) continue;
            const PlyReview& p = rv.plies[static_cast<size_t>(ply) - 1];
            bool selected = s.cursor == ply;
            if (selected) fillRect(r, cell, ui.button);
            int sc = fs;
            int room = cell.w - R.rowHeight - 6;
            while (sc > 1 && font::textWidth(p.san, sc) > room) --sc;
            font::drawExact(r, p.san, cell.x + 4, y + (R.rowHeight - 7 * sc) / 2, sc, selected ? ui.buttonText : ui.text);
            int radius = std::max(5, R.rowHeight * 36 / 100);
            drawBadge(r, cell.x + cell.w - radius - 3, y + R.rowHeight / 2, radius, p.label);
        }
    }
    SDL_RenderSetClipRect(r, nullptr);

    int visible = std::max(1, R.list.h / R.rowHeight);
    if (rows > visible) {  // scroll bar
        SDL_Rect track{R.list.x + R.list.w - barW, R.list.y, barW, R.list.h};
        fillRect(r, track, SDL_Color{0, 0, 0, 70});
        int thumbH = std::max(10, R.list.h * visible / rows);
        int thumbY = R.list.y + (R.list.h - thumbH) * s.listScroll / std::max(1, rows - visible);
        fillRect(r, SDL_Rect{track.x, thumbY, barW, thumbH}, ui.muted);
    }
}

void drawDetail(SDL_Renderer* r, const State& st, const ReviewSession& s, const ReviewLayout& R) {
    const UiTheme& ui = st.settings.uiTheme();
    const GameReview& rv = *s.review;
    fillRect(r, R.detail, SDL_Color{ui.background.r, ui.background.g, ui.background.b, 130});
    int pad = R.pad / 2 + 2;
    const int fs = R.fontScale;
    if (s.cursor == 0) {
        font::drawWrapped(r, "Starting position. Use the arrows or click a move.", R.detail.x + pad, R.detail.y + pad,
                          R.detail.w - 2 * pad, std::max(1, fs - 1), ui.muted);
        return;
    }
    const PlyReview& p = rv.plies[static_cast<size_t>(s.cursor) - 1];
    int radius = std::max(8, fs * 7);
    drawBadge(r, R.detail.x + pad + radius, R.detail.y + R.detail.h / 2, radius, p.label);
    int tx = R.detail.x + pad * 2 + radius * 2;
    int tw = R.detail.x + R.detail.w - pad - tx;

    std::string head = p.san, rest = std::string("  ") + labelTitle(p.label);
    int sc = fs;
    while (sc > 1 && font::textWidth(head + rest, sc) > tw) --sc;
    int y = R.detail.y + pad;
    font::drawExact(r, head, tx, y, sc, ui.text);
    font::drawExact(r, rest, tx + font::textWidth(head, sc), y, sc, labelColor(p.label));
    y += font::lineHeight(sc) + 2;

    std::string sub;
    if (p.hasBetterMove()) sub += "Best was " + p.bestSan + "   ";
    if (p.hasScoreAfter) sub += "Eval " + p.scoreAfter.str() + "   ";
    sub += "Accuracy " + fmt("%.0f%%", p.accuracy);
    font::drawWrappedExact(r, sub, tx, y, tw, std::max(1, fs - 1), ui.muted);
}

void drawResults(SDL_Renderer* r, const State& st, const ReviewSession& s, const ReviewLayout& R, const PieceArt& art) {
    const UiTheme& ui = st.settings.uiTheme();
    const GameReview& rv = *s.review;
    const int fs = R.fontScale;

    drawBoard(r, st, s, R, art);
    fillRect(r, R.panel, ui.panel);

    drawButton(r, st, R.btnBack, "BACK", s.hover == HBack, false, fs);
    drawButton(r, st, R.btnFlip, "FLIP", s.hover == HFlip, false, fs);
    drawButton(r, st, R.btnArrow, s.showBest ? "ARROW: ON" : "ARROW: OFF", s.hover == HArrow, false, fs);
    drawButton(r, st, R.btnSave, "SAVE PGN", s.hover == HSave, false, fs);

    auto accuracy = [&](SDL_Rect box, const char* who, double value, SDL_Color disc) {
        fillRect(r, box, SDL_Color{ui.background.r, ui.background.g, ui.background.b, 130});
        int d = box.h / 3;
        fillCircle(r, box.x + d + 4, box.y + box.h / 2, d, ui.muted);
        fillCircle(r, box.x + d + 4, box.y + box.h / 2, d - 1, disc);
        int sc = std::max(1, fs - 1);
        std::string value_text = fmt("%.1f%%", value);
        int vs = fs;
        while (vs > 1 && font::textWidth(value_text, vs) + box.h > box.w - 4) --vs;
        font::draw(r, value_text, box.x + box.w - font::textWidth(value_text, vs) - 6, box.y + (box.h - 7 * vs) / 2, vs, ui.text);
        std::string label = who;
        if (box.w > font::textWidth(label, sc) + font::textWidth(value_text, vs) + box.h + 12) {
            font::draw(r, label, box.x + d * 2 + 10, box.y + (box.h - 7 * sc) / 2, sc, ui.muted);
        }
    };
    accuracy(R.accWhite, "WHITE", rv.accuracyWhite, SDL_Color{248, 246, 238, 255});
    accuracy(R.accBlack, "BLACK", rv.accuracyBlack, SDL_Color{20, 20, 24, 255});

    if (!rv.opening.empty()) {
        std::string text = rv.openingEco + " " + rv.opening;
        int sc = std::max(1, fs - 1);
        while (sc > 1 && font::textWidth(text, sc) > R.opening.w) --sc;
        font::drawExact(r, text, R.opening.x, R.opening.y, sc, ui.muted);
    }

    drawGraph(r, st, s, R);
    drawDetail(r, st, s, R);

    const char* navLabels[4] = {"<<", "<", ">", ">>"};
    for (int i = 0; i < 4; ++i) {
        bool disabled = i < 2 ? s.cursor == 0 : s.cursor == s.moveCount();
        drawButton(r, st, R.nav[static_cast<size_t>(i)], navLabels[i], s.hover == HNav0 + i, disabled, fs);
    }
    drawList(r, st, s, R);
}

// ---------------------------------------------------------------- setup, progress and error card

void drawCard(SDL_Renderer* r, const State& st, const ReviewSession& s, const ReviewLayout& R) {
    const UiTheme& ui = st.settings.uiTheme();
    const int fs = R.fontScale;
    fillRect(r, R.card, ui.panel);
    const int x = R.card.x + R.pad * 2, w = R.card.w - R.pad * 4;
    int y = R.card.y + R.pad * 2;

    y += font::drawWrapped(r, "REVIEW WITH STOCKFISH", x, y, w, fs + 1, ui.text) + R.pad / 2;
    std::string source = s.moveCount() == 0 ? "There are no moves yet. Play a game, or open a PGN file."
                                            : "Reviewing " + s.source + ": " + std::to_string(s.moveCount()) + " moves.";
    y += font::drawWrappedExact(r, source, x, y, w, fs, ui.text) + R.pad / 2;
    if (!s.warning.empty()) y += font::drawWrappedExact(r, s.warning, x, y, w, std::max(1, fs - 1), ui.muted) + R.pad / 2;

    if (s.phase == ReviewSession::Phase::Error) {
        int h = font::drawWrappedExact(r, s.error, x + 6, y + 6, w - 12, std::max(1, fs - 1), ui.text);
        fillRect(r, SDL_Rect{x, y, w, h + 12}, SDL_Color{202, 52, 49, 70});
        font::drawWrappedExact(r, s.error, x + 6, y + 6, w - 12, std::max(1, fs - 1), ui.text);
        y += h + 12 + R.pad / 2;
    }

    if (s.phase == ReviewSession::Phase::Running) {
        const int total = std::max(1, s.total.load());
        const int done = s.done.load();
        fillRect(r, R.progress, SDL_Color{0, 0, 0, 80});
        fillRect(r, SDL_Rect{R.progress.x, R.progress.y, R.progress.w * std::min(done, total) / total, R.progress.h},
                 SDL_Color{129, 182, 76, 255});
        font::drawExact(r, "Analysing position " + std::to_string(done) + " of " + std::to_string(total), x,
                        R.progress.y + R.progress.h + R.pad / 2, fs, ui.text);
        font::drawWrappedExact(r, "Depth " + std::to_string(s.depth()) + ". This can take a minute or two for a long game.", x,
                               R.progress.y + R.progress.h + R.pad / 2 + font::lineHeight(fs) + 2, w, std::max(1, fs - 1), ui.muted);
        drawButton(r, st, R.btnCancel, "CANCEL", s.hover == HCancel, false, fs);
        return;
    }

    font::drawWrapped(r, "HOW DEEP SHOULD STOCKFISH LOOK?", x, R.depth[0].y - font::lineHeight(std::max(1, fs - 1)) - 4, w,
                      std::max(1, fs - 1), ui.muted);
    const char* names[3] = {"QUICK", "STANDARD", "DEEP"};
    for (int i = 0; i < 3; ++i) {
        std::string label = std::string(names[i]) + " " + std::to_string(ReviewSession::kDepths[i]);
        SDL_Rect b = R.depth[static_cast<size_t>(i)];
        bool selected = s.depthIndex == i;
        fillRect(r, b, selected ? SDL_Color{129, 182, 76, 255} : (s.hover == HDepth0 + i ? ui.buttonHover : ui.button));
        int sc = fs;
        while (sc > 1 && font::textWidth(label, sc) > b.w - 6) --sc;
        font::draw(r, label, b.x + (b.w - font::textWidth(label, sc)) / 2, b.y + (b.h - 7 * sc) / 2, sc,
                   selected ? SDL_Color{20, 30, 10, 255} : ui.buttonText);
    }
    drawButton(r, st, R.btnStart, s.phase == ReviewSession::Phase::Error ? "TRY AGAIN" : "START REVIEW", s.hover == HStart,
               s.moveCount() == 0, fs);
    drawButton(r, st, R.btnOpen, "OPEN A PGN FILE", s.hover == HOpen, false, fs);
    drawButton(r, st, R.btnCardBack, "BACK", s.hover == HBack, false, fs);
}

}  // namespace

// ================================================================ layout

ReviewLayout computeReviewLayout(const Layout& L, const ReviewSession& s) {
    ReviewLayout R;
    R.landscape = L.landscape;
    R.fontScale = L.fontScale;
    R.scale = L.scale;
    R.pad = std::max(6, static_cast<int>(12 * L.scale));
    R.gap = std::max(4, static_cast<int>(8 * L.scale));
    const int m = std::max(8, static_cast<int>(16 * L.scale));
    const int W = L.width, H = L.height, fs = L.fontScale;

    // ----- card (setup, progress, error)
    {
        int cw = std::min(W - 2 * m, static_cast<int>(580 * L.scale));
        int ch = std::min(H - 2 * m, static_cast<int>(420 * L.scale));
        R.card = {(W - cw) / 2, (H - ch) / 2, cw, ch};
        int x = R.card.x + R.pad * 2, w = R.card.w - R.pad * 4;
        int bh = std::max(30, static_cast<int>(38 * L.scale));
        int bottom = R.card.y + R.card.h - R.pad * 2;
        R.btnCardBack = {x, bottom - bh, w, bh};
        R.btnOpen = {x, R.btnCardBack.y - R.gap - bh, w, bh};
        R.btnStart = {x, R.btnOpen.y - R.gap - bh, w, bh};
        int gw = (w - 2 * R.gap) / 3;
        for (int i = 0; i < 3; ++i) R.depth[static_cast<size_t>(i)] = {x + i * (gw + R.gap), R.btnStart.y - R.gap * 2 - bh, gw, bh};
        R.progress = {x, R.card.y + R.card.h / 2 - bh / 2, w, std::max(10, bh / 2)};
        R.btnCancel = {x, bottom - bh, w, bh};
    }

    // ----- results screen
    if (L.landscape) {
        R.board = L.board;
        R.square = L.square;
        R.panel = L.panel;
    } else {
        int side = std::min(W - 2 * m, static_cast<int>(H * 0.46f));
        R.square = std::max(1, side / 8);
        int bs = R.square * 8;
        R.board = {(W - bs) / 2, m, bs, bs};
        R.panel = {m, R.board.y + bs + m, W - 2 * m, std::max(0, H - (R.board.y + bs + m) - m)};
    }
    const int x = R.panel.x + R.pad, w = R.panel.w - 2 * R.pad;
    int y = R.panel.y + R.pad;
    const int bh = std::max(24, static_cast<int>((L.landscape ? 32 : 28) * L.scale));

    int bw = (w - 3 * R.gap) / 4;
    R.btnBack = {x, y, bw, bh};
    R.btnFlip = {x + (bw + R.gap), y, bw, bh};
    R.btnArrow = {x + 2 * (bw + R.gap), y, bw, bh};
    R.btnSave = {x + 3 * (bw + R.gap), y, w - 3 * (bw + R.gap), bh};
    y += bh + R.gap;

    int accH = std::max(26, static_cast<int>(34 * L.scale));
    int halfW = (w - R.gap) / 2;
    R.accWhite = {x, y, halfW, accH};
    R.accBlack = {x + halfW + R.gap, y, w - halfW - R.gap, accH};
    y += accH + R.gap / 2;

    R.opening = {x, y, w, font::lineHeight(std::max(1, fs - 1))};
    y += R.opening.h + R.gap / 2;

    int graphH = static_cast<int>((L.landscape ? 64 : 44) * L.scale);
    R.graph = {x, y, w, graphH};
    y += graphH + R.gap;

    int detailH = font::lineHeight(fs) + font::lineHeight(std::max(1, fs - 1)) * 2 + R.pad * 2;
    R.detail = {x, y, w, detailH};
    y += detailH + R.gap;

    int navW = (w - 3 * R.gap) / 4;
    for (int i = 0; i < 4; ++i) R.nav[static_cast<size_t>(i)] = {x + i * (navW + R.gap), y, i == 3 ? w - 3 * (navW + R.gap) : navW, bh};
    y += bh + R.gap;

    R.rowHeight = font::lineHeight(fs) + 6;
    int listH = std::max(0, R.panel.y + R.panel.h - R.pad - y);
    R.list = {x, y, w, listH};
    (void)s;
    return R;
}

// ================================================================ drawing

void drawReview(SDL_Renderer* r, const State& st, const PieceArt& art) {
    const ReviewSession& s = *st.review;
    SDL_Color bg = st.settings.uiTheme().background;
    setColor(r, bg);
    SDL_RenderClear(r);
    ReviewLayout R = computeReviewLayout(st.layout, s);
    if (s.phase == ReviewSession::Phase::Done && s.review) drawResults(r, st, s, R, art);
    else drawCard(r, st, s, R);
}

// ================================================================ input

namespace {

int rowsIn(const ReviewSession& s) {
    if (!s.review) return 0;
    const bool firstIsWhite = s.game.initialBoard().sideToMove == Color::White;
    return (static_cast<int>(s.review->plies.size()) + (firstIsWhite ? 0 : 1) + 1) / 2;
}

void clampScroll(ReviewSession& s, const ReviewLayout& R) {
    int visible = std::max(1, R.list.h / R.rowHeight);
    s.listScroll = std::clamp(s.listScroll, 0, std::max(0, rowsIn(s) - visible));
}

}  // namespace

void reviewEnsureVisible(State& st) {
    ReviewSession& s = *st.review;
    if (!s.review || s.cursor == 0) return;
    ReviewLayout R = computeReviewLayout(st.layout, s);
    const bool firstIsWhite = s.game.initialBoard().sideToMove == Color::White;
    int row = (s.cursor - 1 + (firstIsWhite ? 0 : 1)) / 2;
    int visible = std::max(1, R.list.h / R.rowHeight);
    if (row < s.listScroll) s.listScroll = row;
    else if (row >= s.listScroll + visible) s.listScroll = row - visible + 1;
    clampScroll(s, R);
}

void reviewHover(State& st, int x, int y) {
    ReviewSession& s = *st.review;
    ReviewLayout R = computeReviewLayout(st.layout, s);
    s.hover = HNone;
    auto at = [&](const SDL_Rect& rc, int id) {
        if (contains(rc, x, y)) s.hover = id;
    };
    if (s.phase == ReviewSession::Phase::Done) {
        at(R.btnBack, HBack);
        at(R.btnFlip, HFlip);
        at(R.btnArrow, HArrow);
        at(R.btnSave, HSave);
        for (int i = 0; i < 4; ++i) at(R.nav[static_cast<size_t>(i)], HNav0 + i);
    } else if (s.phase == ReviewSession::Phase::Running) {
        at(R.btnCancel, HCancel);
    } else {
        for (int i = 0; i < 3; ++i) at(R.depth[static_cast<size_t>(i)], HDepth0 + i);
        at(R.btnStart, HStart);
        at(R.btnOpen, HOpen);
        at(R.btnCardBack, HBack);
    }
}

ReviewAction reviewPress(State& st, int x, int y) {
    ReviewSession& s = *st.review;
    ReviewLayout R = computeReviewLayout(st.layout, s);

    if (s.phase == ReviewSession::Phase::Running) {
        if (contains(R.btnCancel, x, y)) s.cancel();
        return ReviewAction::None;
    }
    if (s.phase != ReviewSession::Phase::Done) {
        for (int i = 0; i < 3; ++i) {
            if (contains(R.depth[static_cast<size_t>(i)], x, y)) s.depthIndex = i;
        }
        if (contains(R.btnStart, x, y) && s.moveCount() > 0) s.start();
        if (contains(R.btnOpen, x, y)) return ReviewAction::OpenPgn;
        if (contains(R.btnCardBack, x, y)) return ReviewAction::Close;
        return ReviewAction::None;
    }

    if (contains(R.btnBack, x, y)) return ReviewAction::Close;
    if (contains(R.btnFlip, x, y)) s.flipped = !s.flipped;
    if (contains(R.btnArrow, x, y)) s.showBest = !s.showBest;
    if (contains(R.btnSave, x, y)) return ReviewAction::SavePgn;
    if (contains(R.nav[0], x, y)) s.goTo(0);
    if (contains(R.nav[1], x, y)) s.goTo(s.cursor - 1);
    if (contains(R.nav[2], x, y)) s.goTo(s.cursor + 1);
    if (contains(R.nav[3], x, y)) s.goTo(s.moveCount());

    if (contains(R.graph, x, y) && R.graph.w > 1) {
        int n = static_cast<int>(s.review->whiteWinChance.size());
        s.goTo(static_cast<int>(std::lround(static_cast<double>(x - R.graph.x) / (R.graph.w - 1) * (n - 1))));
    }
    if (contains(R.list, x, y)) {
        const bool firstIsWhite = s.game.initialBoard().sideToMove == Color::White;
        int row = s.listScroll + (y - R.list.y) / R.rowHeight;
        int numW = R.fontScale * 6 * 3 + 4;
        int barW = std::max(4, R.fontScale * 2);
        int cellW = (R.list.w - numW - barW) / 2;
        int rel = x - R.list.x - numW;
        if (rel >= 0 && rel < 2 * cellW) {
            int side = rel / cellW;
            int ply = row * 2 + side + (firstIsWhite ? 1 : 0);
            if (ply >= 1 && ply <= static_cast<int>(s.review->plies.size())) s.goTo(ply);
        }
    }
    reviewEnsureVisible(st);
    return ReviewAction::None;
}

ReviewAction reviewKey(State& st, SDL_Keycode key) {
    ReviewSession& s = *st.review;
    if (key == SDLK_ESCAPE) {
        if (s.phase == ReviewSession::Phase::Running) {
            s.cancel();
            return ReviewAction::None;
        }
        return ReviewAction::Close;
    }
    if (s.phase != ReviewSession::Phase::Done) return ReviewAction::None;
    switch (key) {
        case SDLK_RIGHT: s.goTo(s.cursor + 1); break;
        case SDLK_LEFT: s.goTo(s.cursor - 1); break;
        case SDLK_HOME: s.goTo(0); break;
        case SDLK_END: s.goTo(s.moveCount()); break;
        case SDLK_f: s.flipped = !s.flipped; break;
        default: return ReviewAction::None;
    }
    reviewEnsureVisible(st);
    return ReviewAction::None;
}

void reviewWheel(State& st, int wheelY) {
    ReviewSession& s = *st.review;
    if (s.phase != ReviewSession::Phase::Done) return;
    ReviewLayout R = computeReviewLayout(st.layout, s);
    s.listScroll -= wheelY * 2;
    clampScroll(s, R);
}
