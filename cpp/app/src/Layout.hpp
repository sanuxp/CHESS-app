#pragma once

#include <SDL.h>

#include <algorithm>
#include <array>

enum ButtonId { BtnNewGame, BtnUndo, BtnSavePgn, BtnCopyFen, BtnFlip, BtnThemes, BtnReview, BtnCount };

// Screen geometry in renderer pixels. Recomputed every frame, so resizing, rotation and
// high-DPI changes need no special handling. Wide windows put the panel beside the board,
// tall windows (phones in portrait) put it underneath.
struct Layout {
    int width = 0, height = 0;
    float scale = 1.0f;     // UI scale relative to a 720 px window
    int fontScale = 2;      // integer scale for the pixel font
    bool landscape = true;
    int square = 0;
    SDL_Rect board{};
    SDL_Rect panel{};
    std::array<SDL_Rect, BtnCount> buttons{};
};

inline bool contains(const SDL_Rect& r, int x, int y) {
    return x >= r.x && x < r.x + r.w && y >= r.y && y < r.y + r.h;
}

inline Layout computeLayout(int W, int H) {
    Layout L;
    L.width = W;
    L.height = H;
    L.scale = std::clamp(std::min(W, H) / 720.0f, 0.6f, 3.0f);
    L.fontScale = std::max(1, static_cast<int>(2 * L.scale + 0.5f));
    L.landscape = W >= static_cast<int>(H * 1.1f);

    const int m = static_cast<int>(16 * L.scale);
    const int pad = static_cast<int>(14 * L.scale);
    const int gap = static_cast<int>(10 * L.scale);

    if (L.landscape) {
        int panelW = std::max(static_cast<int>(230 * L.scale), static_cast<int>(W * 0.26f));
        int side = std::min(H - 2 * m, W - 3 * m - panelW);
        L.square = std::max(1, side / 8);
        int bs = L.square * 8;
        int areaW = W - 3 * m - panelW;
        L.board = {m + (areaW - bs) / 2, (H - bs) / 2, bs, bs};
        L.panel = {L.board.x + bs + m, L.board.y, W - (L.board.x + bs + m) - m, bs};
        int bh = static_cast<int>(42 * L.scale);
        for (int i = 0; i < BtnCount; ++i) {
            int fromBottom = BtnCount - i;
            L.buttons[i] = {L.panel.x + pad, L.panel.y + L.panel.h - pad - fromBottom * bh - (fromBottom - 1) * gap,
                            L.panel.w - 2 * pad, bh};
        }
    } else {
        int panelH = static_cast<int>(180 * L.scale);
        int side = std::min(W - 2 * m, H - 3 * m - panelH);
        L.square = std::max(1, side / 8);
        int bs = L.square * 8;
        L.board = {(W - bs) / 2, m, bs, bs};
        L.panel = {m, L.board.y + bs + m, W - 2 * m, std::max(0, H - (L.board.y + bs + m) - m)};
        int bh = static_cast<int>(38 * L.scale);
        int bw = (L.panel.w - 2 * pad - 3 * gap) / 4;
        for (int i = 0; i < BtnCount; ++i) {
            int col = i % 4, row = i / 4;
            L.buttons[i] = {L.panel.x + pad + col * (bw + gap), L.panel.y + L.panel.h - pad - (2 - row) * bh - (1 - row) * gap, bw, bh};
        }
    }
    return L;
}

// Promotion picker: four squares (Q, R, B, N) centred on the board.
inline std::array<SDL_Rect, 4> promotionRects(const Layout& L) {
    std::array<SDL_Rect, 4> r{};
    int size = std::min(static_cast<int>(L.square * 1.2f), L.board.w / 4 - 4);
    int gap = size / 8;
    int total = 4 * size + 3 * gap;
    int x = L.board.x + (L.board.w - total) / 2;
    int y = L.board.y + (L.board.h - size) / 2;
    for (int i = 0; i < 4; ++i) r[i] = {x + i * (size + gap), y, size, size};
    return r;
}

// Settings view: replaces the side panel in wide windows, and covers the lower part of the board plus the
// panel in tall windows so the top ranks stay visible as a live preview.
enum SettingRow { RowBoard, RowPieces, RowInterface, RowCoords, RowHints, RowAnim, SettingRowCount };

struct SettingsLayout {
    SDL_Rect area{};
    SDL_Rect title{};
    std::array<SDL_Rect, SettingRowCount> rows{};
    SDL_Rect done{}, reset{};
};

inline SettingsLayout settingsLayout(const Layout& L) {
    SettingsLayout S;
    const int pad = static_cast<int>(14 * L.scale);
    const int gap = static_cast<int>(6 * L.scale);
    if (L.landscape) {
        S.area = L.panel;
    } else {
        int top = L.board.y + L.board.h * 4 / 10;
        S.area = {L.panel.x, top, L.panel.w, L.panel.y + L.panel.h - top};
    }
    int units = SettingRowCount + 2;  // title + rows + done/reset
    int rh = std::min(static_cast<int>(42 * L.scale), (S.area.h - 2 * pad - units * gap) / units);
    rh = std::max(rh, 8);
    int x = S.area.x + pad, w = S.area.w - 2 * pad, y = S.area.y + pad;
    S.title = {x, y, w, rh};
    y += rh + gap;
    for (int i = 0; i < SettingRowCount; ++i) {
        S.rows[i] = {x, y, w, rh};
        y += rh + gap;
    }
    int bw = (w - gap) / 2;
    S.reset = {x, y, bw, rh};
    S.done = {x + bw + gap, y, w - bw - gap, rh};
    return S;
}
