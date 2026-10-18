#pragma once

#include <string>
#include <vector>

#include "Layout.hpp"
#include "Theme.hpp"
#include "chess/Game.hpp"

// A piece sliding between two squares. The game has already applied the move (or undone it),
// so the renderer hides the real piece on `hidden` squares and draws this one in flight.
struct Animation {
    bool active = false;
    double elapsed = 0;
    double duration = 0.22;  // seconds, see docs/UI_SPEC.md

    chess::Piece piece;
    chess::Square from = chess::NoSquare, to = chess::NoSquare;

    bool hasRook = false;
    chess::Piece rook;
    chess::Square rookFrom = chess::NoSquare, rookTo = chess::NoSquare;

    bool hasCaptured = false;
    bool capturedFadesIn = false;  // true when undoing a capture
    chess::Piece captured;
    chess::Square capturedSquare = chess::NoSquare;

    chess::Square hiddenA = chess::NoSquare;  // board squares not drawn while animating
    chess::Square hiddenB = chess::NoSquare;

    double progress() const { return duration > 0 ? std::min(1.0, elapsed / duration) : 1.0; }
    double eased() const {  // ease-in-out (smoothstep)
        double t = progress();
        return t * t * (3 - 2 * t);
    }
};

// Everything the renderer needs. The app mutates it, the renderer only reads it.
struct State {
    chess::Game game;
    Layout layout;
    Settings settings;
    bool settingsOpen = false;
    int hoverRow = -1;  // settings row or button under the pointer: 0..5 rows, 100 reset, 101 done

    bool flipMode = false;  // turn the board towards whoever is to move
    bool flipped = false;

    chess::Square selected = chess::NoSquare;
    std::vector<chess::Move> targets;

    bool mouseDown = false;
    bool dragging = false;
    chess::Square dragFrom = chess::NoSquare;
    int downX = 0, downY = 0, dragX = 0, dragY = 0;

    Animation anim;

    bool promotionOpen = false;
    chess::Square promoFrom = chess::NoSquare, promoTo = chess::NoSquare;
    chess::Color promoColor = chess::Color::White;
    int hoverPromo = -1;

    int hoverButton = -1;
    std::string toast;
    double toastAge = 0;
};
