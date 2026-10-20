#pragma once

#include <map>
#include <string>

#include "chess/Game.hpp"

namespace chess {

// The result of reading PGN text: the game, its tags, the result token and an error message if reading
// stopped early (an illegal move, a bad FEN). The moves before the problem are still in `game`.
struct PgnImport {
    Game game;
    std::map<std::string, std::string> headers;
    std::string result = "*";  // "1-0", "0-1", "1/2-1/2" or "*"
    std::string error;         // empty when everything was read
};

// Reads the first game in `text`. Comments, variations, NAGs and engine annotations are ignored.
PgnImport importPGN(const std::string& text);

}  // namespace chess
