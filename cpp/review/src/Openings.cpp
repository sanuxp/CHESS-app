#include "chess/Openings.hpp"

#include <sstream>

#include "chess/Pgn.hpp"

namespace chess {

std::string OpeningBook::key(const Board& b) {
    const std::string fen = b.toFEN();
    int spaces = 0;
    for (size_t i = 0; i < fen.size(); ++i) {
        if (fen[i] == ' ' && ++spaces == 4) return fen.substr(0, i);  // drop the move counters
    }
    return fen;
}

const Opening* OpeningBook::nameOf(const Board& b) const {
    auto it = names_.find(key(b));
    return it == names_.end() ? nullptr : &it->second;
}

OpeningBook OpeningBook::parse(const std::string& tsv) {
    OpeningBook book;
    std::istringstream lines(tsv);
    std::string line;
    bool header = true;
    while (std::getline(lines, line)) {
        if (header) {
            header = false;
            continue;
        }
        if (!line.empty() && line.back() == '\r') line.pop_back();
        size_t t1 = line.find('\t');
        if (t1 == std::string::npos) continue;
        size_t t2 = line.find('\t', t1 + 1);
        if (t2 == std::string::npos) continue;
        std::string eco = line.substr(0, t1), name = line.substr(t1 + 1, t2 - t1 - 1), pgn = line.substr(t2 + 1);

        Board board;
        bool ok = true;
        std::istringstream tokens(pgn);
        std::string token;
        while (ok && tokens >> token) {
            size_t dot = token.rfind('.');
            std::string san = dot == std::string::npos ? token : token.substr(dot + 1);  // "1." or "1.e4"
            if (san.empty()) continue;
            auto m = moveFromSAN(board, san);
            if (!m) {
                ok = false;
                break;
            }
            board.applyMove(*m);
            book.positions_.insert(key(board));
        }
        if (ok) book.names_[key(board)] = Opening{eco, name};
    }
    return book;
}

}  // namespace chess
