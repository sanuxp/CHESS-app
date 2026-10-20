#pragma once

#include <SDL.h>

#include <string>

#include "PieceArt.hpp"
#include "ReviewUi.hpp"
#include "State.hpp"

// Options for the hidden "--screenshot" mode used to check the UI without opening a window.
struct ShotOptions {
    std::string outPath;
    int width = 1000, height = 720;
    std::string moves;      // space separated UCI moves to play first
    std::string select;     // square to show as selected, e.g. "e2"
    double animate = -1;    // 0..1 to freeze the last move mid-animation
    bool promotion = false; // show the promotion picker
    bool flip = false;
    int board = -1, pieces = -1, ui = -1;  // theme indices; -1 keeps the default
    bool settings = false;                 // show the settings view
    bool undoLast = false;  // freeze an undo animation instead
    std::string reviewSan;  // show the game review of these SAN moves (runs Stockfish and waits for it)
    bool reviewSetup = false;  // ... but stop at the setup card
    int reviewDepth = 0;       // 0 quick, 1 standard, 2 deep
    int cursor = -1;           // move to show in the review
};

class App {
public:
    ~App();
    bool init();
    void run();
    bool screenshot(const ShotOptions& opt);
    // Drives the real input handlers with scripted clicks and drags; returns true if all checks pass.
    bool selfTest();

private:
    // Input
    void onPress(int x, int y);
    void onMove(int x, int y);
    void onRelease(int x, int y);
    void onButton(int id);
    void onKey(SDL_Keycode key);
    void onSettingsPress(int x, int y);
    void changeSetting(int row, int delta);
    void applySettings(bool redrawPieces);
    void toWindowPixels(int& x, int& y) const;
    void openReview();
    void handleReview(ReviewAction action);

    // Game actions
    void selectSquare(chess::Square sq);
    void clearSelection();
    void tryMove(chess::Square to, bool dropped = false);
    void playMove(const chess::Move& m, bool animate);
    void finishPromotion(chess::PieceType type);
    void newGame();
    void undoMove();
    void savePgn();
    void copyFen();
    void updateFlip();
    void startAnimation(const chess::Board& before, const chess::Move& m, bool reverse);
    void update(double dt);
    void say(const std::string& text);

    SDL_Window* window_ = nullptr;
    SDL_Renderer* renderer_ = nullptr;
    PieceArt art_;
    State st_;
    bool running_ = true;
    bool saveSettings_ = true;
};
