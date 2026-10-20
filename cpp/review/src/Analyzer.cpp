#include "chess/Analyzer.hpp"

namespace chess {

void EngineAnalyzer::configure(int threads, int hashMb) {
    engine_.setOption("Threads", std::to_string(threads));
    engine_.setOption("Hash", std::to_string(hashMb));
}

const std::vector<EngineLine>& EngineAnalyzer::evaluate(const std::string& fen, const std::atomic<bool>* cancel) {
    const std::string key = std::to_string(depth_) + "/" + std::to_string(multiPv_) + "/" + fen;
    auto it = cache_.find(key);
    if (it != cache_.end()) return it->second;
    std::vector<EngineLine> lines = engine_.analyze(fen, depth_, multiPv_, std::chrono::minutes(2), cancel);
    if (cancel && cancel->load()) throw ReviewCancelled();  // a stopped search is incomplete: do not cache it
    return cache_[key] = std::move(lines);
}

GameReview EngineAnalyzer::review(const Game& game, const OpeningBook* book, const ReviewConfig& config,
                                  const std::function<void(int, int)>& progress, const std::atomic<bool>* cancel) {
    const auto& history = game.history();
    std::vector<std::string> fens;
    for (const HistoryEntry& h : history) fens.push_back(h.before.toFEN());
    fens.push_back(game.board().toFEN());

    std::vector<PositionEval> evals;
    for (size_t i = 0; i < fens.size(); ++i) {
        if (cancel && cancel->load()) throw ReviewCancelled();
        const bool last = i + 1 == fens.size();
        if (last && isGameOver(game.status())) {
            evals.push_back(PositionEval{});  // nothing left to play, nothing to search
        } else {
            evals.push_back(PositionEval{evaluate(fens[i], cancel)});
        }
        if (progress) progress(static_cast<int>(i) + 1, static_cast<int>(fens.size()));
    }
    return reviewGame(game, evals, book, config);
}

}  // namespace chess
