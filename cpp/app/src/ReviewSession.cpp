#include "ReviewSession.hpp"

#include <algorithm>
#include <filesystem>
#include <fstream>
#include <sstream>

#include "chess/Openings.hpp"
#include "chess/PgnImport.hpp"
#include "chess/Uci.hpp"

using namespace chess;
namespace fs = std::filesystem;

namespace {

std::string g_baseDir;

std::string readFile(const fs::path& p) {
    std::ifstream in(p, std::ios::binary);
    std::stringstream ss;
    ss << in.rdbuf();
    return ss.str();
}

// <program folder>/data/openings.tsv (installed layout), or docs/data/openings.tsv in a parent folder
// (development builds run from build/app inside the repository).
std::string findOpeningsFile() {
    std::error_code ec;
    fs::path dir = g_baseDir.empty() ? fs::current_path(ec) : fs::path(g_baseDir);
    for (int i = 0; i < 10 && !dir.empty(); ++i) {
        for (const char* rel : {"data/openings.tsv", "docs/data/openings.tsv"}) {
            fs::path candidate = dir / rel;
            if (fs::exists(candidate, ec)) return candidate.string();
        }
        fs::path parent = dir.parent_path();
        if (parent == dir) break;
        dir = parent;
    }
    return {};
}

}  // namespace

constexpr int ReviewSession::kDepths[3];

void ReviewSession::setBaseDirectory(const std::string& dir) {
    g_baseDir = dir;
    while (g_baseDir.size() > 1 && (g_baseDir.back() == '/' || g_baseDir.back() == '\\')) g_baseDir.pop_back();
}

Board ReviewSession::positionAt(int index) const {
    const auto& h = game.history();
    return index < static_cast<int>(h.size()) ? h[static_cast<size_t>(index)].before : game.board();
}

void ReviewSession::setGame(const Game& from, const std::string& sourceName) {
    stopAndJoin();
    Game copy(from.initialBoard());
    for (const HistoryEntry& h : from.history()) copy.makeMove(h.move);
    game = std::move(copy);
    source = sourceName;
    warning.clear();
    error.clear();
    review.reset();
    cursor = 0;
    listScroll = 0;
    headers = PgnHeaders{};
    phase = Phase::Setup;
}

bool ReviewSession::loadPgn(const std::string& text, const std::string& name) {
    stopAndJoin();
    PgnImport r = importPGN(text);
    if (r.game.history().empty()) {
        error = r.error.empty() ? "No moves were found in that PGN." : r.error;
        phase = Phase::Error;
        return false;
    }
    setGame(r.game, name);
    auto tag = [&](const char* key, std::string& out) {
        auto it = r.headers.find(key);
        if (it != r.headers.end()) out = it->second;
    };
    tag("Event", headers.event);
    tag("Site", headers.site);
    tag("Date", headers.date);
    tag("Round", headers.round);
    tag("White", headers.white);
    tag("Black", headers.black);
    if (!r.error.empty()) warning = "Reading stopped early: " + r.error;
    return true;
}

void ReviewSession::start() {
    if (phase == Phase::Running || game.history().empty()) return;
    stopAndJoin();
    phase = Phase::Running;
    error.clear();
    done = 0;
    total = moveCount() + 1;
    cancel_ = false;
    finished_ = false;
    cancelled_ = false;
    workerError_.clear();
    workerResult_.reset();
    const int searchDepth = depth();

    worker_ = std::thread([this, searchDepth] {
        try {
            std::string path = findEnginePath();
            if (path.empty()) throw EngineError(engineSearchHint());
            auto transport = openProcessTransport(path);
            if (!transport) throw EngineError("Could not start Stockfish at " + path);
            UciEngine engine(std::move(transport));
            engine.start();
            EngineAnalyzer analyzer(engine, searchDepth);
            analyzer.configure();

            std::string bookPath = findOpeningsFile();
            OpeningBook book = bookPath.empty() ? OpeningBook{} : OpeningBook::parse(readFile(bookPath));
            workerResult_ = analyzer.review(
                game, bookPath.empty() ? nullptr : &book, ReviewConfig{},
                [this](int d, int t) {
                    done = d;
                    total = t;
                },
                &cancel_);
        } catch (const ReviewCancelled&) {
            cancelled_ = true;
        } catch (const std::exception& e) {
            workerError_ = e.what();
        }
        finished_ = true;
    });
}

void ReviewSession::cancel() { cancel_ = true; }

void ReviewSession::poll() {
    if (phase != Phase::Running || !finished_) return;
    if (worker_.joinable()) worker_.join();
    if (cancelled_) {
        phase = Phase::Setup;
    } else if (!workerError_.empty()) {
        error = workerError_;
        phase = Phase::Error;
    } else if (workerResult_) {
        review = std::move(workerResult_);
        workerResult_.reset();
        cursor = 0;
        listScroll = 0;
        phase = Phase::Done;
    } else {
        error = "The review produced no result.";
        phase = Phase::Error;
    }
}

void ReviewSession::stopAndJoin() {
    cancel_ = true;
    if (worker_.joinable()) worker_.join();
    if (phase == Phase::Running) phase = Phase::Setup;
}

void ReviewSession::goTo(int index) { cursor = std::clamp(index, 0, moveCount()); }

std::string ReviewSession::exportPgn() const {
    if (!review) return toPGN(game, headers);
    std::vector<PlyAnnotation> notes = reviewAnnotations(*review);
    return toPGN(game, headers, &notes);
}
