#include <algorithm>
#include <iostream>

#include "json_mini.hpp"
#include "tests.hpp"

using namespace chess;

namespace {

std::string uci(const Move& m) {
    std::string s = squareName(m.from) + squareName(m.to);
    switch (m.promo) {
        case PieceType::Queen: s += 'q'; break;
        case PieceType::Rook: s += 'r'; break;
        case PieceType::Bishop: s += 'b'; break;
        case PieceType::Knight: s += 'n'; break;
        default: break;
    }
    return s;
}

bool hasMove(const Game& g, const std::string& text) {
    const auto& moves = g.legalMoves();
    return std::any_of(moves.begin(), moves.end(), [&](const Move& m) { return uci(m) == text; });
}

}  // namespace

int runScenarioTests(const std::string& dataDir) {
    json::Value data = json::parseFile(dataDir + "/scenarios.json");
    int failures = 0;

    for (const json::Value& sc : data.at("scenarios").items) {
        const std::string& name = sc.at("name").asString();
        std::vector<std::string> problems;

        Game game;
        if (!gameFromSpec(sc.at("fen").asString(), game)) {
            problems.push_back("bad FEN");
        } else {
            for (const json::Value& mv : sc.at("moves").items) {
                if (!game.makeMoveUCI(mv.asString())) {
                    problems.push_back("move rejected: " + mv.asString());
                    break;
                }
            }
        }

        if (problems.empty()) {
            const json::Value& ex = sc.at("expect");
            if (const json::Value* v = ex.find("status")) {
                if (v->asString() != statusName(game.status())) {
                    problems.push_back("status " + std::string(statusName(game.status())) + ", expected " + v->asString());
                }
            }
            if (const json::Value* v = ex.find("result")) {
                if (v->asString() != game.resultToken()) {
                    problems.push_back("result " + game.resultToken() + ", expected " + v->asString());
                }
            }
            if (const json::Value* v = ex.find("fen")) {
                if (v->asString() != game.board().toFEN()) {
                    problems.push_back("fen " + game.board().toFEN() + ", expected " + v->asString());
                }
            }
            if (const json::Value* v = ex.find("legalCount")) {
                if (static_cast<long long>(game.legalMoves().size()) != v->asInt()) {
                    problems.push_back("legalCount " + std::to_string(game.legalMoves().size()) + ", expected " +
                                       std::to_string(v->asInt()));
                }
            }
            if (const json::Value* v = ex.find("legalContains")) {
                for (const json::Value& m : v->items) {
                    if (!hasMove(game, m.asString())) problems.push_back("missing legal move " + m.asString());
                }
            }
            if (const json::Value* v = ex.find("legalNotContains")) {
                for (const json::Value& m : v->items) {
                    if (hasMove(game, m.asString())) problems.push_back("illegal move allowed: " + m.asString());
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
    return failures;
}
