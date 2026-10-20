#include <cstdlib>
#include <exception>
#include <iostream>
#include <string>

#include "chess/Pgn.hpp"
#include "tests.hpp"

using namespace chess;

bool gameFromSpec(const std::string& spec, Game& out) {
    if (spec == "startpos") {
        out = Game();
        return true;
    }
    return Game::fromFEN(spec, out);
}

const char* statusName(GameStatus s) {
    switch (s) {
        case GameStatus::Playing: return "Playing";
        case GameStatus::Check: return "Check";
        case GameStatus::Checkmate: return "Checkmate";
        case GameStatus::Stalemate: return "Stalemate";
        case GameStatus::DrawFiftyMove: return "DrawFiftyMove";
        case GameStatus::DrawRepetition: return "DrawRepetition";
        case GameStatus::DrawInsufficient: return "DrawInsufficient";
    }
    return "?";
}

namespace {

int failures = 0;

void check(bool ok, const std::string& what) {
    if (!ok) {
        ++failures;
        std::cout << "  FAIL: " << what << "\n";
    }
}

}  // namespace

// Sanity checks that do not need the shared data files.
int runBasicTests() {
    failures = 0;

    Board start;
    check(start.toFEN() == "rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1", "start FEN round trip");

    const char* fens[] = {
        "r3k2r/p1ppqpb1/bn2pnp1/3PN3/1p2P3/2N2Q1p/PPPBBPPP/R3K2R w KQkq - 0 1",
        "8/2p5/3p4/KP5r/1R3p1k/8/4P1P1/8 w - - 0 1",
        "rnbqkbnr/pppp1ppp/8/4p3/4P3/8/PPPP1PPP/RNBQKBNR w KQkq e6 0 2",
    };
    for (const char* f : fens) {
        Board b;
        check(Board::fromFEN(f, b) && b.toFEN() == f, std::string("FEN round trip: ") + f);
    }

    Board junk;
    check(!Board::fromFEN("", junk), "empty FEN rejected");
    check(!Board::fromFEN("8/8/8/8/8/8/8/8 w - - 0 1", junk), "FEN without kings rejected");
    check(!Board::fromFEN("rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBN w KQkq - 0 1", junk), "short rank rejected");
    check(!Board::fromFEN("rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR x KQkq - 0 1", junk), "bad side rejected");

    Game g;
    check(g.legalMoves().size() == 20, "20 moves from start");
    check(!g.makeMoveUCI("e2e5"), "illegal move rejected");
    check(g.makeMoveUCI("e2e4"), "e2e4 accepted");
    check(g.history().size() == 1 && g.history()[0].san == "e4", "SAN recorded");
    check(g.undo(), "undo works");
    check(g.board().toFEN() == start.toFEN(), "undo restores the position");
    check(!g.undo(), "undo with no history fails");

    // Undo across a castle must restore rights and rook.
    Game c;
    check(Game::fromFEN("r3k2r/8/8/8/8/8/8/R3K2R w KQkq - 0 1", c), "castle FEN loads");
    std::string before = c.board().toFEN();
    check(c.makeMoveUCI("e1g1"), "castle played");
    check(c.undo() && c.board().toFEN() == before, "undo castle");

    // Undo must also drop a repetition count.
    Game r;
    for (const char* m : {"g1f3", "g8f6", "f3g1", "f6g8", "g1f3", "g8f6", "f3g1", "f6g8"}) r.makeMoveUCI(m);
    check(r.status() == GameStatus::DrawRepetition, "repetition reached");
    r.undo();
    check(r.status() == GameStatus::Playing, "undo leaves repetition");

    std::cout << "basic: " << (failures == 0 ? "ok" : "FAILED") << "\n";
    return failures;
}

int main(int argc, char** argv) {
    if (argc < 2) {
        std::cerr << "usage: chess_tests <basic|perft|scenarios|pgn|review|import|openings|engine> [dataDir] [maxDepth]\n";
        return 2;
    }
    std::string which = argv[1];
    std::string dataDir = argc > 2 ? argv[2] : "";
    int maxDepth = argc > 3 ? std::atoi(argv[3]) : 99;
    try {
        int bad = 0;
        if (which == "basic") bad = runBasicTests();
        else if (which == "perft") bad = runPerftTests(dataDir, maxDepth);
        else if (which == "scenarios") bad = runScenarioTests(dataDir);
        else if (which == "pgn") bad = runPgnTests(dataDir);
        else if (which == "review") bad = runReviewTests(dataDir);
        else if (which == "import") bad = runImportTests(dataDir);
        else if (which == "openings") bad = runOpeningTests(dataDir);
        else if (which == "engine") bad = runEngineTests(dataDir);
        else {
            std::cerr << "unknown test group: " << which << "\n";
            return 2;
        }
        return bad == 0 ? 0 : 1;
    } catch (const std::exception& e) {
        std::cerr << "error: " << e.what() << "\n";
        return 2;
    }
}
