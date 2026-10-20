#pragma once

#include <atomic>
#include <optional>
#include <string>
#include <thread>

#include "chess/Analyzer.hpp"
#include "chess/Pgn.hpp"

// State of one game review: the game, setup choices, a background worker that runs Stockfish, the result and the
// move being looked at. No graphics here; see ReviewUi for the screen.
class ReviewSession {
public:
    enum class Phase { Setup, Running, Done, Error };

    ReviewSession() = default;
    ~ReviewSession() { stopAndJoin(); }
    ReviewSession(const ReviewSession&) = delete;
    ReviewSession& operator=(const ReviewSession&) = delete;

    // Folder the program lives in; the opening database is looked up relative to it.
    static void setBaseDirectory(const std::string& dir);

    // Search depths: quick, standard, deep (docs/REVIEW.md section 6).
    static constexpr int kDepths[3] = {12, 16, 20};
    int depthIndex = 1;
    int depth() const { return kDepths[depthIndex]; }

    Phase phase = Phase::Setup;
    std::string source = "the current game";
    std::string warning;  // e.g. a PGN that stopped at an illegal move
    std::string error;
    chess::PgnHeaders headers;
    chess::Game game;
    std::optional<chess::GameReview> review;

    int cursor = 0;  // 0 = start, n = after n moves
    bool flipped = false;
    bool showBest = true;
    int listScroll = 0;  // first visible row of the move list
    int hover = -1;      // button under the pointer (ids are private to ReviewUi)

    std::atomic<int> done{0}, total{0};

    int moveCount() const { return static_cast<int>(game.history().size()); }
    chess::Board positionAt(int index) const;

    // Replaces the game under review with a copy of `from` and returns to the setup screen.
    void setGame(const chess::Game& from, const std::string& sourceName);
    // Reads PGN text as the game to review. False (and the Error phase) when it has no moves.
    bool loadPgn(const std::string& text, const std::string& name);

    void start();      // runs the review on a worker thread
    void cancel();     // asks the worker to stop; poll() then returns to Setup
    void poll();       // call every frame: collects the worker's result
    void stopAndJoin();

    void goTo(int index);
    std::string exportPgn() const;

private:
    std::thread worker_;
    std::atomic<bool> cancel_{false}, finished_{false};
    bool cancelled_ = false;
    std::string workerError_;
    std::optional<chess::GameReview> workerResult_;
};
