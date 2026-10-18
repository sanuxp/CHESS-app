#include <chrono>
#include <iostream>

#include "chess/MoveGen.hpp"
#include "json_mini.hpp"
#include "tests.hpp"

using namespace chess;

int runPerftTests(const std::string& dataDir, int maxDepth) {
    json::Value data = json::parseFile(dataDir + "/perft.json");
    int failures = 0;

    for (const json::Value& pos : data.at("positions").items) {
        const std::string& name = pos.at("name").asString();
        Board board;
        if (!Board::fromFEN(pos.at("fen").asString(), board)) {
            std::cout << "FAIL " << name << ": bad FEN\n";
            ++failures;
            continue;
        }
        for (const auto& entry : pos.at("nodes").members) {
            int depth = std::atoi(entry.first.c_str());
            if (depth > maxDepth) continue;
            long long expected = entry.second.asInt();
            auto t0 = std::chrono::steady_clock::now();
            long long got = static_cast<long long>(perft(board, depth));
            auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now() - t0).count();
            bool ok = got == expected;
            std::cout << (ok ? "ok   " : "FAIL ") << name << " depth " << depth << ": " << got;
            if (!ok) std::cout << " (expected " << expected << ")";
            std::cout << "  [" << ms << " ms]\n";
            if (!ok) ++failures;
        }
    }
    return failures;
}
