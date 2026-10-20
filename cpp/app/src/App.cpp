#include "App.hpp"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <sstream>

#include "Platform.hpp"
#include "ReviewUi.hpp"
#include "Renderer.hpp"
#include "chess/Pgn.hpp"

using namespace chess;

App::~App() {
    if (renderer_) SDL_DestroyRenderer(renderer_);
    if (window_) SDL_DestroyWindow(window_);
    SDL_Quit();
}

bool App::init() {
    SDL_SetHint(SDL_HINT_RENDER_SCALE_QUALITY, "1");
    // Touches arrive as emulated left-button mouse events, so one code path serves mouse and finger.
    SDL_SetHint(SDL_HINT_TOUCH_MOUSE_EVENTS, "1");
    SDL_SetHint(SDL_HINT_MOUSE_TOUCH_EVENTS, "0");
    if (SDL_Init(SDL_INIT_VIDEO) != 0) {
        SDL_Log("SDL_Init failed: %s", SDL_GetError());
        return false;
    }
    window_ = SDL_CreateWindow("Chess", SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, 1000, 720,
                               SDL_WINDOW_RESIZABLE | SDL_WINDOW_ALLOW_HIGHDPI);
    if (!window_) {
        SDL_Log("Window failed: %s", SDL_GetError());
        return false;
    }
    SDL_SetWindowMinimumSize(window_, 320, 400);
    renderer_ = SDL_CreateRenderer(window_, -1, SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC);
    if (!renderer_) renderer_ = SDL_CreateRenderer(window_, -1, SDL_RENDERER_SOFTWARE);
    if (!renderer_) {
        SDL_Log("Renderer failed: %s", SDL_GetError());
        return false;
    }
    st_.settings = Settings::load();
    if (!art_.init(renderer_, st_.settings.pieceSet())) {
        SDL_Log("Could not draw pieces: %s", SDL_GetError());
        return false;
    }
    return true;
}

void App::run() {
    Uint64 last = SDL_GetPerformanceCounter();
    while (running_) {
        SDL_Event e;
        while (SDL_PollEvent(&e)) {
            switch (e.type) {
                case SDL_QUIT: running_ = false; break;
                case SDL_MOUSEBUTTONDOWN:
                    if (e.button.button == SDL_BUTTON_LEFT) {
                        int x = e.button.x, y = e.button.y;
                        toWindowPixels(x, y);
                        onPress(x, y);
                    }
                    break;
                case SDL_MOUSEBUTTONUP:
                    if (e.button.button == SDL_BUTTON_LEFT) {
                        int x = e.button.x, y = e.button.y;
                        toWindowPixels(x, y);
                        onRelease(x, y);
                    }
                    break;
                case SDL_MOUSEMOTION: {
                    int x = e.motion.x, y = e.motion.y;
                    toWindowPixels(x, y);
                    onMove(x, y);
                    break;
                }
                case SDL_KEYDOWN: onKey(e.key.keysym.sym); break;
                case SDL_MOUSEWHEEL:
                    if (st_.review) reviewWheel(st_, e.wheel.y);
                    break;
                default: break;
            }
        }

        Uint64 now = SDL_GetPerformanceCounter();
        double dt = static_cast<double>(now - last) / static_cast<double>(SDL_GetPerformanceFrequency());
        last = now;
        update(std::min(dt, 0.1));

        int w = 0, h = 0;
        SDL_GetRendererOutputSize(renderer_, &w, &h);
        st_.layout = computeLayout(w, h);
        drawFrame(renderer_, st_, art_);
        SDL_RenderPresent(renderer_);
    }
}

// Window coordinates (points) to renderer pixels; they differ on high-DPI screens.
void App::toWindowPixels(int& x, int& y) const {
    int ww = 0, wh = 0, rw = 0, rh = 0;
    SDL_GetWindowSize(window_, &ww, &wh);
    SDL_GetRendererOutputSize(renderer_, &rw, &rh);
    if (ww > 0 && wh > 0) {
        x = static_cast<int>(x * static_cast<double>(rw) / ww);
        y = static_cast<int>(y * static_cast<double>(rh) / wh);
    }
}

void App::update(double dt) {
    st_.toastAge += dt;
    if (st_.review) st_.review->poll();
    if (st_.anim.active) {
        st_.anim.elapsed += dt;
        if (st_.anim.elapsed >= st_.anim.duration) {
            st_.anim.active = false;
            updateFlip();
        }
    }
}

void App::say(const std::string& text) {
    st_.toast = text;
    st_.toastAge = 0;
}

// ---------------------------------------------------------------- input

void App::onPress(int x, int y) {
    if (st_.review) {
        handleReview(reviewPress(st_, x, y));
        return;
    }
    if (st_.settingsOpen) {
        onSettingsPress(x, y);
        return;
    }
    if (st_.anim.active) return;

    if (st_.promotionOpen) {
        auto rects = promotionRects(st_.layout);
        const PieceType types[4] = {PieceType::Queen, PieceType::Rook, PieceType::Bishop, PieceType::Knight};
        for (int i = 0; i < 4; ++i) {
            if (contains(rects[i], x, y)) {
                finishPromotion(types[i]);
                return;
            }
        }
        st_.promotionOpen = false;
        clearSelection();
        return;
    }

    for (int i = 0; i < BtnCount; ++i) {
        if (contains(st_.layout.buttons[i], x, y)) {
            onButton(i);
            return;
        }
    }

    Square sq = squareAt(st_.layout, st_.flipped, x, y);
    if (sq == NoSquare) {
        clearSelection();
        return;
    }
    if (isGameOver(st_.game.status())) return;

    // Second click of click-click: move the selected piece here.
    if (st_.selected != NoSquare) {
        for (const Move& m : st_.targets) {
            if (m.to == sq) {
                tryMove(sq);
                return;
            }
        }
    }

    Piece p = st_.game.board().pieceAt(sq);
    if (!p.isNone() && p.color == st_.game.board().sideToMove) {
        selectSquare(sq);
        st_.mouseDown = true;
        st_.dragFrom = sq;
        st_.downX = st_.dragX = x;
        st_.downY = st_.dragY = y;
    } else {
        clearSelection();
    }
}

void App::onSettingsPress(int x, int y) {
    const SettingsLayout S = settingsLayout(st_.layout);
    if (contains(S.done, x, y)) {
        st_.settingsOpen = false;
    } else if (contains(S.reset, x, y)) {
        st_.settings = Settings{};
        applySettings(true);
    } else {
        for (int i = 0; i < SettingRowCount; ++i) {
            if (!contains(S.rows[i], x, y)) continue;
            // Left quarter steps back, anywhere else steps forward; toggles flip on any click.
            bool back = i <= RowInterface && x < S.rows[i].x + S.rows[i].w / 4;
            changeSetting(i, back ? -1 : 1);
            break;
        }
    }
}

void App::changeSetting(int row, int delta) {
    auto cycle = [delta](int v, int n) { return ((v + delta) % n + n) % n; };
    Settings& s = st_.settings;
    switch (row) {
        case RowBoard: s.board = cycle(s.board, kBoardThemeCount); break;
        case RowPieces: s.pieces = cycle(s.pieces, kPieceSetCount); break;
        case RowInterface: s.ui = cycle(s.ui, kUiThemeCount); break;
        case RowCoords: s.coordinates = !s.coordinates; break;
        case RowHints: s.hints = !s.hints; break;
        case RowAnim: s.animations = !s.animations; break;
        default: break;
    }
    applySettings(row == RowPieces);
}

void App::applySettings(bool redrawPieces) {
    if (redrawPieces && renderer_) art_.init(renderer_, st_.settings.pieceSet());
    if (!st_.settings.animations) st_.anim = Animation{};
    if (saveSettings_) st_.settings.save();
    updateFlip();
}

void App::onMove(int x, int y) {
    if (st_.review) {
        reviewHover(st_, x, y);
        return;
    }
    st_.hoverRow = -1;
    if (st_.settingsOpen) {
        const SettingsLayout S = settingsLayout(st_.layout);
        for (int i = 0; i < SettingRowCount; ++i) {
            if (contains(S.rows[i], x, y)) st_.hoverRow = i;
        }
        if (contains(S.reset, x, y)) st_.hoverRow = 100;
        if (contains(S.done, x, y)) st_.hoverRow = 101;
        st_.hoverButton = -1;
        return;
    }
    st_.hoverButton = -1;
    for (int i = 0; i < BtnCount; ++i) {
        if (contains(st_.layout.buttons[i], x, y)) st_.hoverButton = i;
    }
    st_.hoverPromo = -1;
    if (st_.promotionOpen) {
        auto rects = promotionRects(st_.layout);
        for (int i = 0; i < 4; ++i) {
            if (contains(rects[i], x, y)) st_.hoverPromo = i;
        }
    }
    if (st_.mouseDown) {
        st_.dragX = x;
        st_.dragY = y;
        int dx = x - st_.downX, dy = y - st_.downY;
        if (!st_.dragging && dx * dx + dy * dy > 36) st_.dragging = true;
    }
}

void App::onRelease(int x, int y) {
    bool wasDragging = st_.dragging;
    st_.mouseDown = false;
    st_.dragging = false;
    if (!wasDragging || st_.anim.active) return;

    Square sq = squareAt(st_.layout, st_.flipped, x, y);
    if (sq == NoSquare || sq == st_.dragFrom) return;
    for (const Move& m : st_.targets) {
        if (m.to == sq) {
            tryMove(sq, true);  // the piece is already under the pointer: no flight animation
            return;
        }
    }
}

void App::onButton(int id) {
    switch (id) {
        case BtnReview: openReview(); break;
        case BtnNewGame: newGame(); break;
        case BtnUndo: undoMove(); break;
        case BtnSavePgn: savePgn(); break;
        case BtnCopyFen: copyFen(); break;
        case BtnFlip:
            st_.flipMode = !st_.flipMode;
            updateFlip();
            break;
        case BtnThemes:
            clearSelection();
            st_.settingsOpen = true;
            break;
        default: break;
    }
}

void App::onKey(SDL_Keycode key) {
    if (st_.review) {
        handleReview(reviewKey(st_, key));
        return;
    }
    if (st_.settingsOpen) {
        if (key == SDLK_ESCAPE || key == SDLK_t) st_.settingsOpen = false;
        return;
    }
    if (key == SDLK_t) {
        onButton(BtnThemes);
    } else if (key == SDLK_ESCAPE) {
        st_.promotionOpen = false;
        clearSelection();
    } else if (key == SDLK_u || (key == SDLK_z && (SDL_GetModState() & KMOD_CTRL))) {
        undoMove();
    }
}

// ---------------------------------------------------------------- game actions

void App::openReview() {
    ReviewSession::setBaseDirectory(SDL_GetBasePath() ? SDL_GetBasePath() : "");
    st_.review = std::make_unique<ReviewSession>();
    st_.review->setGame(st_.game, "the current game");
    clearSelection();
    st_.promotionOpen = false;
}

void App::handleReview(ReviewAction action) {
    if (!st_.review) return;
    switch (action) {
        case ReviewAction::Close: st_.review.reset(); break;
        case ReviewAction::SavePgn: {
            platform::SaveResult res = platform::savePgn(window_, st_.review->exportPgn());
            if (res.cancelled) say("SAVE CANCELLED");
            else if (res.saved) {
                size_t slash = res.path.find_last_of("/\\");
                say("SAVED " + (slash == std::string::npos ? res.path : res.path.substr(slash + 1)));
            } else say("COULD NOT SAVE: " + res.error);
            break;
        }
        case ReviewAction::OpenPgn: {
            platform::OpenResult res = platform::openPgn(window_);
            if (!res.supported) {
                st_.review->error = "Opening a PGN file is only available on Windows for now.";
                st_.review->phase = ReviewSession::Phase::Error;
            } else if (!res.error.empty()) {
                st_.review->error = res.error;
                st_.review->phase = ReviewSession::Phase::Error;
            } else if (res.opened) {
                st_.review->loadPgn(res.text, res.name);
            }
            break;
        }
        case ReviewAction::None: break;
    }
}

void App::selectSquare(Square sq) {
    st_.selected = sq;
    st_.targets = st_.game.legalMovesFrom(sq);
}

void App::clearSelection() {
    st_.selected = NoSquare;
    st_.targets.clear();
    st_.dragging = false;
    st_.mouseDown = false;
    st_.dragFrom = NoSquare;
}

void App::tryMove(Square to, bool dropped) {
    std::vector<Move> options;
    for (const Move& m : st_.targets) {
        if (m.to == to) options.push_back(m);
    }
    if (options.empty()) return;
    if (options.size() > 1) {     // promotion: ask which piece
        st_.promotionOpen = true;
        st_.promoFrom = options[0].from;
        st_.promoTo = to;
        st_.promoColor = st_.game.board().sideToMove;
        st_.dragging = false;
        st_.mouseDown = false;
        return;
    }
    playMove(options[0], !dropped);
}

void App::finishPromotion(PieceType type) {
    Move chosen;
    bool found = false;
    for (const Move& m : st_.targets) {
        if (m.to == st_.promoTo && m.promo == type) {
            chosen = m;
            found = true;
        }
    }
    st_.promotionOpen = false;
    if (found) playMove(chosen, true);
    else clearSelection();
}

void App::playMove(const Move& m, bool animate) {
    Board before = st_.game.board();
    if (!st_.game.makeMove(m)) return;
    clearSelection();
    if (animate && st_.settings.animations) startAnimation(before, m, false);
    else updateFlip();
}

void App::newGame() {
    st_.game = Game();
    clearSelection();
    st_.promotionOpen = false;
    st_.anim = Animation{};
    updateFlip();
    say("NEW GAME");
}

void App::undoMove() {
    if (st_.anim.active || st_.game.history().empty()) return;
    HistoryEntry entry = st_.game.history().back();
    st_.game.undo();
    clearSelection();
    st_.promotionOpen = false;
    if (st_.settings.animations) startAnimation(entry.before, entry.move, true);
    else updateFlip();
}

void App::savePgn() {
    PgnHeaders headers;
    headers.date = platform::pgnDate();
    platform::SaveResult res = platform::savePgn(window_, toPGN(st_.game, headers));
    if (res.cancelled) {
        say("SAVE CANCELLED");
    } else if (res.saved) {
        size_t slash = res.path.find_last_of("/\\");
        say("SAVED " + (slash == std::string::npos ? res.path : res.path.substr(slash + 1)));
    } else {
        say("COULD NOT SAVE: " + res.error);
    }
}

void App::copyFen() {
    SDL_SetClipboardText(st_.game.board().toFEN().c_str());
    say("FEN COPIED TO CLIPBOARD");
}

void App::updateFlip() {
    if (st_.anim.active) return;
    st_.flipped = st_.flipMode && st_.game.board().sideToMove == Color::Black;
}

void App::startAnimation(const Board& before, const Move& m, bool reverse) {
    Animation a;
    a.active = true;
    a.piece = before.pieceAt(m.from);
    a.from = reverse ? m.to : m.from;
    a.to = reverse ? m.from : m.to;
    a.hiddenA = a.to;

    if (m.isCapture()) {
        a.hasCaptured = true;
        a.capturedFadesIn = reverse;
        a.capturedSquare = m.isEnPassant() ? m.to + (a.piece.color == Color::White ? -8 : 8) : m.to;
        a.captured = before.pieceAt(a.capturedSquare);
    }
    if (m.flags & (FlagCastleKing | FlagCastleQueen)) {
        int rank = rankOf(m.from);
        bool king = (m.flags & FlagCastleKing) != 0;
        Square home = makeSquare(king ? 7 : 0, rank), landed = makeSquare(king ? 5 : 3, rank);
        a.hasRook = true;
        a.rook = Piece{PieceType::Rook, a.piece.color};
        a.rookFrom = reverse ? landed : home;
        a.rookTo = reverse ? home : landed;
        a.hiddenB = a.rookTo;
    }
    st_.anim = a;
}

// Renders one frame into a BMP with the software renderer; used to inspect the UI without a window.
bool App::screenshot(const ShotOptions& o) {
    SDL_SetHint(SDL_HINT_RENDER_SCALE_QUALITY, "1");
    if (SDL_Init(SDL_INIT_VIDEO) != 0) return false;
    SDL_Surface* surf = SDL_CreateRGBSurfaceWithFormat(0, o.width, o.height, 32, SDL_PIXELFORMAT_ARGB8888);
    if (!surf) return false;
    renderer_ = SDL_CreateSoftwareRenderer(surf);
    saveSettings_ = false;  // screenshots must not overwrite saved choices
    if (o.board >= 0) st_.settings.board = o.board % kBoardThemeCount;
    if (o.pieces >= 0) st_.settings.pieces = o.pieces % kPieceSetCount;
    if (o.ui >= 0) st_.settings.ui = o.ui % kUiThemeCount;
    bool ok = renderer_ && art_.init(renderer_, st_.settings.pieceSet());

    if (ok) {
        std::istringstream in(o.moves);
        std::string mv;
        std::vector<std::string> list;
        while (in >> mv) list.push_back(mv);
        for (size_t i = 0; i < list.size(); ++i) {
            bool last = i + 1 == list.size();
            Board before = st_.game.board();
            if (!st_.game.makeMoveUCI(list[i])) {
                SDL_Log("illegal move in --moves: %s", list[i].c_str());
                ok = false;
                break;
            }
            if (last && o.animate >= 0 && !o.undoLast) {
                startAnimation(before, *st_.game.lastMove(), false);
                st_.anim.elapsed = o.animate * st_.anim.duration;
            }
        }
        if (ok && o.undoLast && o.animate >= 0 && !st_.game.history().empty()) {
            HistoryEntry entry = st_.game.history().back();
            st_.game.undo();
            startAnimation(entry.before, entry.move, true);
            st_.anim.elapsed = o.animate * st_.anim.duration;
        }
        st_.flipMode = o.flip;
        updateFlip();
        st_.layout = computeLayout(o.width, o.height);
        if (!o.reviewSan.empty()) {
            Game g;
            std::istringstream tokens(o.reviewSan);
            std::string san;
            while (tokens >> san) {
                if (!g.makeMoveSAN(san)) {
                    SDL_Log("illegal move in --review: %s", san.c_str());
                    ok = false;
                    break;
                }
            }
            ReviewSession::setBaseDirectory(SDL_GetBasePath() ? SDL_GetBasePath() : "");
            st_.review = std::make_unique<ReviewSession>();
            st_.review->setGame(g, "the test game");
            st_.review->depthIndex = std::clamp(o.reviewDepth, 0, 2);
            if (ok && !o.reviewSetup) {
                st_.review->start();
                while (st_.review->phase == ReviewSession::Phase::Running) {
                    SDL_Delay(50);
                    st_.review->poll();
                }
                if (st_.review->phase == ReviewSession::Phase::Error) SDL_Log("review failed: %s", st_.review->error.c_str());
                if (o.cursor >= 0) st_.review->goTo(o.cursor);
                reviewEnsureVisible(st_);
            }
        }
        if (!o.select.empty()) {
            Square sq = squareFromName(o.select);
            if (sq != NoSquare) selectSquare(sq);
        }
        st_.settingsOpen = o.settings;
        if (o.promotion) {
            st_.promotionOpen = true;
            st_.promoColor = st_.game.board().sideToMove;
        }
        drawFrame(renderer_, st_, art_);
        SDL_RenderPresent(renderer_);
        ok = SDL_SaveBMP(surf, o.outPath.c_str()) == 0;
    }
    if (renderer_) SDL_DestroyRenderer(renderer_);
    renderer_ = nullptr;
    SDL_FreeSurface(surf);
    return ok;
}

bool App::selfTest() {
    int failures = 0;
    // The app has no console on Windows, so results also go to selftest.log in the working directory.
    std::FILE* log = std::fopen("selftest.log", "w");
    auto logLine = [&](const char* line) {
        SDL_Log("%s", line);
        if (log) std::fprintf(log, "%s\n", line);
    };
    auto check = [&](bool ok, const char* what) {
        logLine((std::string(ok ? "ok   " : "FAIL ") + what).c_str());
        if (!ok) ++failures;
    };
    auto relayout = [&]() {
        int w = 0, h = 0;
        SDL_GetRendererOutputSize(renderer_, &w, &h);
        st_.layout = computeLayout(w, h);
    };
    auto center = [&](const char* name, int& x, int& y) {
        SDL_Rect r = squareRect(st_.layout, st_.flipped, squareFromName(name));
        x = r.x + r.w / 2;
        y = r.y + r.h / 2;
    };
    auto click = [&](const char* name) {
        int x, y;
        center(name, x, y);
        onPress(x, y);
        onRelease(x, y);
    };
    auto settle = [&]() { update(1.0); };

    saveSettings_ = false;
    st_.settings = Settings{};
    relayout();

    // Click-click move with animation.
    click("e2");
    check(st_.selected == squareFromName("e2") && st_.targets.size() == 2, "selecting e2 shows two targets");
    click("e4");
    check(st_.game.history().size() == 1 && st_.anim.active, "click e2 then e4 plays e4 and animates");
    click("e7");  // ignored while animating
    check(st_.selected == NoSquare, "input is locked during the animation");
    settle();
    check(!st_.anim.active, "animation finishes");

    // Illegal target: select then click a non-target; selection is dropped, nothing moves.
    click("e7");
    click("e4");
    check(st_.game.history().size() == 1, "illegal destination does not move");

    // Drag and drop.
    int x, y, x2, y2;
    center("e7", x, y);
    center("e5", x2, y2);
    onPress(x, y);
    onMove(x + 20, y + 40);
    check(st_.dragging, "moving the pointer starts a drag");
    onMove(x2, y2);
    onRelease(x2, y2);
    check(st_.game.history().size() == 2 && !st_.anim.active, "drag e7 to e5 plays e5 without animation");

    // Wrong colour cannot be picked.
    click("e4");
    check(st_.selected == NoSquare || st_.game.board().sideToMove == Color::White, "turns alternate");

    // Promotion picker.
    Game::fromFEN("8/P6k/8/8/8/8/8/K7 w - - 0 1", st_.game);
    clearSelection();
    click("a7");
    click("a8");
    check(st_.promotionOpen, "reaching the last rank opens the promotion picker");
    auto rects = promotionRects(st_.layout);
    onPress(rects[2].x + rects[2].w / 2, rects[2].y + rects[2].h / 2);  // bishop
    check(!st_.promotionOpen && st_.game.history().size() == 1 && st_.game.history()[0].san == "a8=B",
          "choosing the bishop promotes to a bishop");
    settle();

    // Undo animates back and restores the position.
    onButton(BtnUndo);
    check(st_.game.history().empty() && st_.anim.active, "undo restores the move and animates");
    settle();

    // Castling: rook animates with the king.
    Game::fromFEN("r3k2r/8/8/8/8/8/8/R3K2R w KQkq - 0 1", st_.game);
    clearSelection();
    click("e1");
    click("g1");
    check(st_.anim.active && st_.anim.hasRook && st_.anim.rookTo == squareFromName("f1"), "castling animates the rook");
    settle();

    // New game and flip mode.
    onButton(BtnNewGame);
    check(st_.game.history().empty() && st_.game.board().toFEN() == Board().toFEN(), "new game resets");
    onButton(BtnFlip);
    click("e2");
    click("e4");
    settle();
    check(st_.flipped, "flip mode turns the board to Black after White moves");

    // Settings view: open, cycle, toggle, reset, close.
    onButton(BtnThemes);
    check(st_.settingsOpen, "themes button opens the settings view");
    {
        SettingsLayout S = settingsLayout(st_.layout);
        auto clickRect = [&](SDL_Rect rc, int dx) { onPress(rc.x + dx, rc.y + rc.h / 2); };
        clickRect(S.rows[RowBoard], S.rows[RowBoard].w / 2);
        check(st_.settings.board == 1, "clicking the board row selects the next board");
        clickRect(S.rows[RowBoard], 2);
        check(st_.settings.board == 0, "clicking the left edge selects the previous board");
        clickRect(S.rows[RowPieces], S.rows[RowPieces].w / 2);
        check(st_.settings.pieces == 1 && art_.get(Piece{PieceType::Pawn, Color::White}) != nullptr,
              "next piece set redraws pieces");
        clickRect(S.rows[RowHints], 5);
        check(!st_.settings.hints, "hints toggle off");
        clickRect(S.reset, 5);
        check(st_.settings.pieces == 0 && st_.settings.hints, "reset restores defaults");
        clickRect(S.done, 5);
        check(!st_.settingsOpen, "done closes the settings view");
    }

    // ---- game review screen
    {
        onButton(BtnReview);
        check(st_.review && st_.review->phase == ReviewSession::Phase::Setup, "REVIEW opens the setup card");
        check(st_.review && st_.review->moveCount() > 0, "the current game is loaded for review");
        ReviewLayout R = computeReviewLayout(st_.layout, *st_.review);

        onPress(R.depth[0].x + 2, R.depth[0].y + 2);
        check(st_.review->depthIndex == 0, "the quick depth can be chosen");
        onPress(R.depth[2].x + 2, R.depth[2].y + 2);
        check(st_.review->depthIndex == 2, "the deep depth can be chosen");
        st_.review->depthIndex = 0;

        // A short game to review: fool's mate.
        Game g;
        for (const char* san : {"f3", "e5", "g4", "Qh4#"}) g.makeMoveSAN(san);
        st_.review->setGame(g, "fool's mate");
        st_.review->depthIndex = 0;
        ReviewSession::setBaseDirectory(SDL_GetBasePath() ? SDL_GetBasePath() : "");

        if (findEnginePath().empty()) {
            onPress(R.btnStart.x + 2, R.btnStart.y + 2);
            for (int i = 0; i < 400 && st_.review->phase == ReviewSession::Phase::Running; ++i) {
                SDL_Delay(25);
                st_.review->poll();
            }
            check(st_.review->phase == ReviewSession::Phase::Error && st_.review->error.find("Stockfish") != std::string::npos,
                  "without Stockfish the review explains how to install it");
        } else {
            onPress(R.btnStart.x + 2, R.btnStart.y + 2);
            check(st_.review->phase == ReviewSession::Phase::Running, "START begins the review");
            for (int i = 0; i < 2400 && st_.review->phase == ReviewSession::Phase::Running; ++i) {
                SDL_Delay(25);
                st_.review->poll();
            }
            check(st_.review->phase == ReviewSession::Phase::Done && st_.review->review, "the review finishes");
            if (st_.review->review) {
                const auto& plies = st_.review->review->plies;
                check(plies.size() == 4 && plies[2].label == MoveLabel::Blunder, "g4 is a blunder");
                check(plies.size() == 4 && plies[3].label == MoveLabel::Best, "Qh4# is best");
                R = computeReviewLayout(st_.layout, *st_.review);
                onPress(R.nav[2].x + 2, R.nav[2].y + 2);
                check(st_.review->cursor == 1, "the next button steps forward");
                std::int32_t lastRow = R.list.y + 2;
                onPress(R.list.x + R.fontScale * 6 * 3 + 8 + (R.list.w - R.fontScale * 6 * 3 - 4 - 2 * R.fontScale) / 2 + 4, lastRow + 1 * R.rowHeight);
                check(st_.review->cursor == 4 || st_.review->cursor == 3, "clicking a move in the list selects it");
                reviewKey(st_, SDLK_HOME);
                check(st_.review->cursor == 0, "Home goes to the start");
                reviewKey(st_, SDLK_END);
                check(st_.review->cursor == 4, "End goes to the last move");
                std::string pgn = st_.review->exportPgn();
                check(pgn.find("$4") != std::string::npos && pgn.find("[%eval") != std::string::npos, "the reviewed PGN has glyphs and evaluations");
            }
        }
        handleReview(ReviewAction::Close);
        check(!st_.review, "BACK closes the review");
    }

    logLine(failures == 0 ? "ALL SELF-TESTS PASSED" : "SELF-TESTS FAILED");
    if (log) std::fclose(log);
    return failures == 0;
}
