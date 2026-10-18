#include <iostream>

#include "chess/Pgn.hpp"
#include "json_mini.hpp"
#include "tests.hpp"

using namespace chess;

int runPgnTests(const std::string& dataDir) {
    json::Value data = json::parseFile(dataDir + "/pgn.json");
    int failures = 0;

    for (const json::Value& tc : data.at("san_cases").items) {
        const std::string& name = tc.at("name").asString();
        std::vector<std::string> problems;
        Game game;
        if (!gameFromSpec(tc.at("fen").asString(), game)) {
            problems.push_back("bad FEN");
        } else {
            const auto& moves = tc.at("moves").items;
            const auto& sans = tc.at("san").items;
            for (size_t i = 0; i < moves.size(); ++i) {
                Board before = game.board();
                if (!game.makeMoveUCI(moves[i].asString())) {
                    problems.push_back("move rejected: " + moves[i].asString());
                    break;
                }
                const std::string& got = game.history().back().san;
                if (got != sans[i].asString()) {
                    problems.push_back("move " + std::to_string(i + 1) + ": got " + got + ", expected " + sans[i].asString());
                }
                // The SAN must resolve back to exactly the move that was played.
                std::optional<Move> parsed = moveFromSAN(before, sans[i].asString());
                if (!parsed || *parsed != game.history().back().move) {
                    problems.push_back("SAN does not parse back: " + sans[i].asString());
                }
            }
        }
        if (problems.empty()) {
            std::cout << "ok   " << name << "\n";
        } else {
            ++failures;
            std::cout << "FAIL " << name << "\n";
            for (const std::string& p : problems) std::cout << "       " << p << "\n";
        }
    }

    for (const json::Value& tc : data.at("pgn_cases").items) {
        const std::string& name = tc.at("name").asString();
        Game game;
        bool ok = gameFromSpec(tc.at("fen").asString(), game);
        for (const json::Value& mv : tc.at("moves").items) ok = ok && game.makeMoveUCI(mv.asString());
        std::string pgn = ok ? toPGN(game) : "";
        if (pgn == tc.at("pgn").asString()) {
            std::cout << "ok   " << name << "\n";
        } else {
            ++failures;
            std::cout << "FAIL " << name << "\n--- got ---\n" << pgn << "--- expected ---\n" << tc.at("pgn").asString() << "\n";
        }
    }
    return failures;
}
