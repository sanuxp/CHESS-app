#pragma once

// Talking to a UCI chess engine (Stockfish): parsing its output, a transport to reach it, and a small client
// that does the handshake and fixed-depth multi-line analysis. Mirrors flutter/lib/review/uci_*.dart.

#include <atomic>
#include <chrono>
#include <memory>
#include <optional>
#include <stdexcept>
#include <string>
#include <vector>

#include "chess/Classify.hpp"

namespace chess {

// One `info ... pv ...` line; scores are from the side to move's point of view.
struct UciInfo {
    int depth = 0;
    int multipv = 1;  // 1 = best line
    EngineScore score;
    std::vector<std::string> pv;
};

// Returns nothing for lines without a score and a variation (currmove, string, ...) and for aspiration-window
// bound lines (lowerbound/upperbound), which are not real results.
std::optional<UciInfo> parseInfo(const std::string& line);

// The move in a `bestmove` line ("(none)" when there is no legal move), or nullopt for other lines.
std::optional<std::string> parseBestMove(const std::string& line);

class EngineError : public std::runtime_error {
public:
    using std::runtime_error::runtime_error;
};

// A line-based connection to an engine: a child process on desktop, a script in tests.
class UciTransport {
public:
    virtual ~UciTransport() = default;
    virtual void send(const std::string& line) = 0;
    // Waits up to `timeout` for the next output line. Returns false on timeout or when the engine has gone.
    virtual bool readLine(std::string& line, std::chrono::milliseconds timeout) = 0;
    virtual void close() = 0;
};

// Where the engine program is expected, for error messages.
std::string engineSearchHint();

// Looks for Stockfish: the given path, then `engine/` next to the program and in its parent folders (so a development
// build finds the repository's `engine/`), then the CHESS_STOCKFISH variable. Empty when not found.
std::string findEnginePath(const std::string& customPath = "");

// Starts the engine program as a child process. Null when `path` is empty or it cannot be started.
std::unique_ptr<UciTransport> openProcessTransport(const std::string& path);

// Handshake, options and analysis over a transport.
class UciEngine {
public:
    explicit UciEngine(std::unique_ptr<UciTransport> transport) : transport_(std::move(transport)) {}
    ~UciEngine();

    // Performs the `uci` / `isready` handshake. Throws EngineError if the engine does not answer in time.
    void start(std::chrono::milliseconds timeout = std::chrono::seconds(20));

    void setOption(const std::string& name, const std::string& value);

    // Best lines for `fen`, best first, scores from the side to move's view. Empty if there is no legal move.
    // Throws EngineError on timeout. `cancel`, if set, stops the search early (the lines found so far are returned).
    std::vector<EngineLine> analyze(const std::string& fen, int depth = 14, int multiPv = 3,
                                    std::chrono::milliseconds timeout = std::chrono::minutes(2),
                                    const std::atomic<bool>* cancel = nullptr);

    void stop() { transport_->send("stop"); }
    const std::string& name() const { return name_; }

private:
    void waitFor(const std::string& command, const std::string& answer, std::chrono::milliseconds timeout);

    std::unique_ptr<UciTransport> transport_;
    std::string name_;
    int multiPv_ = 1;
};

}  // namespace chess
