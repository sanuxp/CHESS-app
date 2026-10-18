#include "Renderer.hpp"

#include <algorithm>
#include <cmath>

#include "Font.hpp"

using namespace chess;

namespace {

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

void ring(SDL_Renderer* r, int cx, int cy, int outer, int inner, SDL_Color c) {
    setColor(r, c);
    for (int dy = -outer; dy <= outer; ++dy) {
        int xo = static_cast<int>(std::sqrt(static_cast<double>(outer * outer - dy * dy)));
        if (std::abs(dy) < inner) {
            int xi = static_cast<int>(std::sqrt(static_cast<double>(inner * inner - dy * dy)));
            SDL_RenderDrawLine(r, cx - xo, cy + dy, cx - xi - 1, cy + dy);
            SDL_RenderDrawLine(r, cx + xi + 1, cy + dy, cx + xo, cy + dy);
        } else {
            SDL_RenderDrawLine(r, cx - xo, cy + dy, cx + xo, cy + dy);
        }
    }
}

SDL_Rect inset(SDL_Rect r, int by) { return {r.x + by, r.y + by, r.w - 2 * by, r.h - 2 * by}; }

void drawPiece(SDL_Renderer* r, const PieceArt& art, Piece p, SDL_Rect cell, Uint8 alpha = 255) {
    SDL_Texture* tex = art.get(p);
    if (!tex) return;
    SDL_SetTextureAlphaMod(tex, alpha);
    SDL_Rect dst = inset(cell, std::max(1, cell.w / 16));
    SDL_RenderCopy(r, tex, nullptr, &dst);
    SDL_SetTextureAlphaMod(tex, 255);
}

SDL_Rect lerpRect(SDL_Rect a, SDL_Rect b, double t) {
    return {static_cast<int>(a.x + (b.x - a.x) * t), static_cast<int>(a.y + (b.y - a.y) * t), a.w, a.h};
}

void drawBoard(SDL_Renderer* r, const State& st, const PieceArt& art) {
    const Layout& L = st.layout;
    const Game& g = st.game;
    const Board& b = g.board();
    const SDL_Color kLightSquare = st.settings.boardTheme().light, kDarkSquare = st.settings.boardTheme().dark;

    for (Square sq = 0; sq < 64; ++sq) {
        bool light = (fileOf(sq) + rankOf(sq)) % 2 == 1;
        fillRect(r, squareRect(L, st.flipped, sq), light ? kLightSquare : kDarkSquare);
    }

    // Last move, selection and check tints.
    if (std::optional<Move> last = g.lastMove()) {
        SDL_Color tint{205, 210, 106, 120};
        fillRect(r, squareRect(L, st.flipped, last->from), tint);
        fillRect(r, squareRect(L, st.flipped, last->to), tint);
    }
    if (st.selected != NoSquare) fillRect(r, squareRect(L, st.flipped, st.selected), SDL_Color{246, 246, 105, 150});
    if (g.inCheck() || g.status() == GameStatus::Checkmate) {
        Square k = b.kingSquare(b.sideToMove);
        if (k != NoSquare) {
            SDL_Rect cell = squareRect(L, st.flipped, k);
            fillCircle(r, cell.x + cell.w / 2, cell.y + cell.h / 2, cell.w / 2 - 2, SDL_Color{220, 50, 50, 150});
        }
    }

    // Coordinates in the board corners.
    int cs = std::max(1, L.square / 44);
    for (int i = 0; st.settings.coordinates && i < 8; ++i) {
        Square fileSq = makeSquare(i, st.flipped ? 7 : 0);
        SDL_Rect cell = squareRect(L, st.flipped, fileSq);
        bool light = (fileOf(fileSq) + rankOf(fileSq)) % 2 == 1;
        font::draw(r, std::string(1, static_cast<char>('a' + i)), cell.x + cell.w - font::advance(cs) - cs,
                   cell.y + cell.h - 8 * cs, cs, light ? kDarkSquare : kLightSquare);
        Square rankSq = makeSquare(st.flipped ? 7 : 0, i);
        SDL_Rect rc = squareRect(L, st.flipped, rankSq);
        bool lightR = (fileOf(rankSq) + rankOf(rankSq)) % 2 == 1;
        font::draw(r, std::string(1, static_cast<char>('1' + i)), rc.x + 2 * cs, rc.y + 2 * cs, cs,
                   lightR ? kDarkSquare : kLightSquare);
    }

    const Animation& a = st.anim;
    bool anim = a.active;

    // Pieces at rest.
    for (Square sq = 0; sq < 64; ++sq) {
        Piece p = b.pieceAt(sq);
        if (p.isNone()) continue;
        if (anim && (sq == a.hiddenA || sq == a.hiddenB)) continue;
        if (anim && a.hasCaptured && sq == a.capturedSquare) continue;
        if (st.dragging && sq == st.dragFrom) continue;
        drawPiece(r, art, p, squareRect(L, st.flipped, sq));
    }

    // Legal-move hints.
    for (const Move& m : st.targets) {
        if (!st.settings.hints) break;
        SDL_Rect cell = squareRect(L, st.flipped, m.to);
        int cx = cell.x + cell.w / 2, cy = cell.y + cell.h / 2;
        bool occupied = !b.pieceAt(m.to).isNone() || m.isEnPassant();
        if (occupied) ring(r, cx, cy, cell.w * 46 / 100, cell.w * 38 / 100, SDL_Color{40, 40, 40, 110});
        else fillCircle(r, cx, cy, std::max(3, cell.w * 16 / 100), SDL_Color{40, 40, 40, 100});
    }

    // Pieces in flight.
    if (anim) {
        double t = a.eased();
        if (a.hasCaptured) {
            double alpha = a.capturedFadesIn ? a.progress() : 1.0 - a.progress();
            drawPiece(r, art, a.captured, squareRect(L, st.flipped, a.capturedSquare), static_cast<Uint8>(255 * alpha));
        }
        if (a.hasRook) {
            drawPiece(r, art, a.rook, lerpRect(squareRect(L, st.flipped, a.rookFrom), squareRect(L, st.flipped, a.rookTo), t));
        }
        drawPiece(r, art, a.piece, lerpRect(squareRect(L, st.flipped, a.from), squareRect(L, st.flipped, a.to), t));
    }

    // Piece held by the mouse or finger.
    if (st.dragging && st.dragFrom != NoSquare) {
        int size = L.square * 11 / 10;
        SDL_Rect cell{st.dragX - size / 2, st.dragY - size / 2, size, size};
        drawPiece(r, art, b.pieceAt(st.dragFrom), cell);
    }
}

void drawButton(SDL_Renderer* r, const State& st, int id) {
    const SDL_Rect& rc = st.layout.buttons[id];
    bool disabled = id == BtnUndo && (st.game.history().empty() || st.anim.active);
    bool hover = st.hoverButton == id && !disabled;
    const UiTheme& ui = st.settings.uiTheme();
    fillRect(r, rc, disabled ? ui.buttonDisabled : hover ? ui.buttonHover : ui.button);
    std::string label = buttonLabel(id, st);
    int scale = st.layout.fontScale;
    while (scale > 1 && font::textWidth(label, scale) > rc.w - 8) --scale;
    int tx = rc.x + (rc.w - font::textWidth(label, scale)) / 2;
    int ty = rc.y + (rc.h - 7 * scale) / 2;
    font::draw(r, label, tx, ty, scale, disabled ? ui.buttonTextDisabled : ui.buttonText);
}

void drawPanel(SDL_Renderer* r, const State& st) {
    const Layout& L = st.layout;
    const UiTheme& ui = st.settings.uiTheme();
    const SDL_Color kText = ui.text, kMuted = ui.muted;
    fillRect(r, L.panel, ui.panel);
    int pad = static_cast<int>(14 * L.scale);
    int x = L.panel.x + pad, w = L.panel.w - 2 * pad;
    int y = L.panel.y + pad;
    int fs = L.fontScale;

    if (L.landscape) {
        font::draw(r, "CHESS", x, y, fs * 2, kText);
        y += font::lineHeight(fs * 2) + pad / 2;
    }

    // Whose turn it is, with a disc in that colour.
    const Board& b = st.game.board();
    bool over = isGameOver(st.game.status());
    int dot = 5 * fs;
    SDL_Color disc = b.sideToMove == Color::White ? SDL_Color{248, 246, 238, 255} : SDL_Color{20, 20, 24, 255};
    fillCircle(r, x + dot, y + 4 * fs, dot + 1, kMuted);
    fillCircle(r, x + dot, y + 4 * fs, dot - 1, disc);
    y += font::drawWrapped(r, statusHeadline(st.game), x + 2 * dot + 8, y, w - 2 * dot - 8, fs, kText) + fs * 2;

    std::string detail = statusDetail(st.game);
    if (!detail.empty()) {
        SDL_Color c = over ? SDL_Color{255, 200, 90, 255} : SDL_Color{255, 120, 120, 255};
        y += font::drawWrapped(r, detail, x, y, w, fs, c) + fs;
    }

    for (int i = 0; i < BtnCount; ++i) drawButton(r, st, i);

    // Toast sits just above the first button, fading out.
    if (!st.toast.empty() && st.toastAge < 4.0) {
        double fade = st.toastAge < 3.0 ? 1.0 : 4.0 - st.toastAge;
        SDL_Color c = kMuted;
        c.a = static_cast<Uint8>(255 * fade);
        int ty = L.buttons[0].y - font::lineHeight(fs) * 3 - pad / 2;
        if (!L.landscape) ty = std::max(y, L.buttons[0].y - font::lineHeight(fs) * 2 - pad / 2);
        font::drawWrapped(r, st.toast, x, ty, w, std::max(1, fs - 1), c);
    }
}

void drawBanner(SDL_Renderer* r, const State& st) {
    const Layout& L = st.layout;
    int h = L.square * 2;
    SDL_Rect band{L.board.x, L.board.y + (L.board.h - h) / 2, L.board.w, h};
    fillRect(r, band, SDL_Color{15, 15, 20, 205});
    int scale = std::max(2, L.fontScale * 2);
    std::string head = statusHeadline(st.game);
    std::string detail = statusDetail(st.game);
    // Headline reads "WHITE TO MOVE" while playing, so show the result-oriented text instead.
    std::string big = detail.empty() ? head : detail;
    while (scale > 1 && font::textWidth(big, scale) > band.w - 20) --scale;
    font::draw(r, big, band.x + (band.w - font::textWidth(big, scale)) / 2, band.y + band.h / 2 - 4 * scale, scale,
               SDL_Color{255, 214, 120, 255});
}

void drawPromotion(SDL_Renderer* r, const State& st, const PieceArt& art) {
    const Layout& L = st.layout;
    fillRect(r, L.board, SDL_Color{0, 0, 0, 140});
    auto rects = promotionRects(L);
    int gap = rects[0].w / 8;
    SDL_Rect frame{rects[0].x - gap * 2, rects[0].y - gap * 2, rects[3].x + rects[3].w - rects[0].x + gap * 4,
                   rects[0].h + gap * 4};
    fillRect(r, frame, SDL_Color{50, 50, 60, 245});
    const PieceType types[4] = {PieceType::Queen, PieceType::Rook, PieceType::Bishop, PieceType::Knight};
    for (int i = 0; i < 4; ++i) {
        fillRect(r, rects[i], i == st.hoverPromo ? st.settings.uiTheme().buttonHover : st.settings.boardTheme().light);
        drawPiece(r, art, Piece{types[i], st.promoColor}, rects[i]);
    }
    int scale = std::max(1, L.fontScale);
    font::draw(r, "CHOOSE A PIECE", frame.x + (frame.w - font::textWidth("CHOOSE A PIECE", scale)) / 2,
               frame.y - font::lineHeight(scale) - gap, scale, SDL_Color{240, 240, 245, 255});
}

void drawArrow(SDL_Renderer* r, SDL_Rect cell, bool left, int scale, SDL_Color c) {
    std::string g = left ? "<" : ">";
    int tw = font::textWidth(g, scale);
    int x = left ? cell.x + cell.h / 2 - tw / 2 : cell.x + cell.w - cell.h / 2 - tw / 2;
    font::draw(r, g, x, cell.y + (cell.h - 7 * scale) / 2, scale, c);
}

// Small preview of the option shown on a row.
void drawSwatch(SDL_Renderer* r, const State& st, const PieceArt& art, int row, SDL_Rect box) {
    const Settings& s = st.settings;
    if (row == RowBoard) {
        int h = box.h / 2;
        for (int i = 0; i < 4; ++i) {
            SDL_Rect q{box.x + (i % 2) * h, box.y + (i / 2) * h, h, h};
            fillRect(r, q, (i == 0 || i == 3) ? s.boardTheme().light : s.boardTheme().dark);
        }
    } else if (row == RowPieces) {
        fillRect(r, box, s.boardTheme().light);
        drawPiece(r, art, Piece{PieceType::Knight, Color::White}, box);
    } else if (row == RowInterface) {
        fillRect(r, box, s.uiTheme().background);
        fillRect(r, SDL_Rect{box.x + box.w / 3, box.y, box.w - box.w / 3, box.h}, s.uiTheme().panel);
        fillRect(r, SDL_Rect{box.x + box.w / 2, box.y + box.h / 3, box.w / 3, box.h / 3}, s.uiTheme().button);
    }
}

void drawSettings(SDL_Renderer* r, const State& st, const PieceArt& art) {
    const Layout& L = st.layout;
    const SettingsLayout S = settingsLayout(L);
    const Settings& s = st.settings;
    const UiTheme& ui = s.uiTheme();
    fillRect(r, S.area, ui.panel);

    int fs = L.fontScale;
    while (fs > 1 && 7 * fs > S.rows[0].h - 6) --fs;
    font::draw(r, "THEMES", S.title.x, S.title.y + (S.title.h - 7 * fs) / 2, fs, ui.text);

    const char* labels[SettingRowCount] = {"BOARD", "PIECES", "INTERFACE", "COORDINATES", "MOVE HINTS", "ANIMATIONS"};
    for (int i = 0; i < SettingRowCount; ++i) {
        SDL_Rect rc = S.rows[i];
        fillRect(r, rc, st.hoverRow == i ? ui.buttonHover : ui.button);
        int labelScale = fs;
        int labelX = rc.x + (i <= RowInterface ? rc.h : rc.h / 3);  // cycling rows keep room for the left arrow
        while (labelScale > 1 && font::textWidth(labels[i], labelScale) > rc.x + rc.w * 2 / 5 - labelX) --labelScale;
        font::draw(r, labels[i], labelX, rc.y + (rc.h - 7 * labelScale) / 2, labelScale, ui.buttonText);

        std::string value;
        switch (i) {
            case RowBoard: value = s.boardTheme().name; break;
            case RowPieces: value = s.pieceSet().name; break;
            case RowInterface: value = s.uiTheme().name; break;
            case RowCoords: value = s.coordinates ? "ON" : "OFF"; break;
            case RowHints: value = s.hints ? "ON" : "OFF"; break;
            default: value = s.animations ? "ON" : "OFF"; break;
        }
        int right = rc.x + rc.w;
        if (i <= RowInterface) {
            drawArrow(r, rc, true, fs, ui.buttonText);
            drawArrow(r, rc, false, fs, ui.buttonText);
            int sw = rc.h - 8;
            int swX = rc.x + rc.w * 2 / 5;
            drawSwatch(r, st, art, i, SDL_Rect{swX, rc.y + 4, sw, sw});
            int valueLeft = swX + sw + rc.h / 4;
            int valueRight = right - rc.h;
            int vs = fs;
            while (vs > 1 && font::textWidth(value, vs) > valueRight - valueLeft) --vs;
            font::draw(r, value, valueLeft + (valueRight - valueLeft - font::textWidth(value, vs)) / 2,
                       rc.y + (rc.h - 7 * vs) / 2, vs, ui.buttonText);
        } else {
            int m = rc.h / 5;
            SDL_Rect pill{right - rc.h * 2 - rc.h / 3, rc.y + m, rc.h * 2, rc.h - 2 * m};
            bool on = value == "ON";
            fillRect(r, pill, on ? SDL_Color{80, 170, 100, 255} : ui.buttonDisabled);
            SDL_Rect knob{on ? pill.x + pill.w - pill.h : pill.x, pill.y, pill.h, pill.h};
            fillRect(r, inset(knob, 2), ui.buttonText);
        }
    }

    auto smallButton = [&](SDL_Rect rc, const char* text, bool hover) {
        fillRect(r, rc, hover ? ui.buttonHover : ui.button);
        int sc = fs;
        while (sc > 1 && font::textWidth(text, sc) > rc.w - 8) --sc;
        font::draw(r, text, rc.x + (rc.w - font::textWidth(text, sc)) / 2, rc.y + (rc.h - 7 * sc) / 2, sc, ui.buttonText);
    };
    smallButton(S.reset, "RESET", st.hoverRow == 100);
    smallButton(S.done, "DONE", st.hoverRow == 101);
}

}  // namespace

std::string statusHeadline(const Game& g) {
    switch (g.status()) {
        case GameStatus::Checkmate: return "GAME OVER";
        case GameStatus::Stalemate:
        case GameStatus::DrawFiftyMove:
        case GameStatus::DrawRepetition:
        case GameStatus::DrawInsufficient: return "GAME OVER";
        default: return g.board().sideToMove == Color::White ? "WHITE TO MOVE" : "BLACK TO MOVE";
    }
}

std::string statusDetail(const Game& g) {
    switch (g.status()) {
        case GameStatus::Check: return "CHECK!";
        case GameStatus::Checkmate:
            return g.board().sideToMove == Color::White ? "CHECKMATE - BLACK WINS" : "CHECKMATE - WHITE WINS";
        case GameStatus::Stalemate: return "STALEMATE - DRAW";
        case GameStatus::DrawFiftyMove: return "DRAW BY 50 MOVE RULE";
        case GameStatus::DrawRepetition: return "DRAW BY REPETITION";
        case GameStatus::DrawInsufficient: return "DRAW - INSUFFICIENT MATERIAL";
        default: return "";
    }
}

const char* buttonLabel(int id, const State& st) {
    switch (id) {
        case BtnNewGame: return "NEW GAME";
        case BtnUndo: return "UNDO";
        case BtnSavePgn: return "SAVE PGN";
        case BtnCopyFen: return "COPY FEN";
        case BtnFlip: return st.flipMode ? "FLIP: ON" : "FLIP: OFF";
        case BtnThemes: return "THEMES";
        default: return "";
    }
}

void drawFrame(SDL_Renderer* r, const State& st, const PieceArt& art) {
    setColor(r, st.settings.uiTheme().background);
    SDL_RenderClear(r);
    drawBoard(r, st, art);
    if (isGameOver(st.game.status()) && !st.anim.active) drawBanner(r, st);
    if (st.promotionOpen) drawPromotion(r, st, art);
    drawPanel(r, st);
    if (st.settingsOpen) drawSettings(r, st, art);
}
