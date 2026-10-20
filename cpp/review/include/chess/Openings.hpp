#pragma once

#include <string>
#include <unordered_map>
#include <unordered_set>

#include "chess/Board.hpp"

namespace chess {

struct Opening {
    std::string eco;
    std::string name;
};

// Opening database (docs/data/openings.tsv, Lichess chess-openings, CC0). Positions are keyed by the first four
// FEN fields (placement, side, castling, en passant) so transpositions match.
class OpeningBook {
public:
    // Parses the TSV: `eco<TAB>name<TAB>pgn`, first line is a header.
    static OpeningBook parse(const std::string& tsv);

    static std::string key(const Board& b);

    size_t size() const { return names_.size(); }
    bool contains(const Board& b) const { return positions_.count(key(b)) != 0; }
    // The opening that ends exactly at `b`, or nullptr.
    const Opening* nameOf(const Board& b) const;

private:
    std::unordered_set<std::string> positions_;
    std::unordered_map<std::string, Opening> names_;
};

}  // namespace chess
