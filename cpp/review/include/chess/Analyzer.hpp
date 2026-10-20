#pragma once

#include <atomic>
#include <exception>
#include <functional>
#include <map>
#include <string>
#include <vector>

#include "chess/Review.hpp"
#include "chess/Uci.hpp"

namespace chess {

// Quick = 12, standard = 16, deep = 20. Shallow searches miss deep tactics (see docs/REVIEW.md section 6).
constexpr int kDefaultReviewDepth = 16;

struct ReviewCancelled : std::exception {
    const char* what() const noexcept override { return "Review cancelled"; }
};

// Runs a whole game through an engine and turns the numbers into a GameReview. One search per position is
// enough (docs/REVIEW.md section 1); results are cached by FEN.
class EngineAnalyzer {
public:
    explicit EngineAnalyzer(UciEngine& engine, int depth = kDefaultReviewDepth, int multiPv = 3)
        : engine_(engine), depth_(depth), multiPv_(multiPv) {}

    // Gives the engine its working options; call once after UciEngine::start. One thread by default: multi-threaded
    // Stockfish was much slower on the development machine.
    void configure(int threads = 1, int hashMb = 128);

    const std::vector<EngineLine>& evaluate(const std::string& fen, const std::atomic<bool>* cancel = nullptr);

    // `progress(done, total)` is called after each position. `cancel` is polled between positions and inside
    // searches; throws ReviewCancelled when set.
    GameReview review(const Game& game, const OpeningBook* book = nullptr, const ReviewConfig& config = ReviewConfig{},
                      const std::function<void(int, int)>& progress = {}, const std::atomic<bool>* cancel = nullptr);

private:
    UciEngine& engine_;
    int depth_, multiPv_;
    std::map<std::string, std::vector<EngineLine>> cache_;
};

}  // namespace chess
