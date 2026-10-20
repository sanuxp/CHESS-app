#include "chess/Pgn.hpp"

#include <algorithm>
#include <sstream>
#include <vector>

namespace chess {

namespace {

char pieceLetter(PieceType t) {
    switch (t) {
        case PieceType::Knight: return 'N';
        case PieceType::Bishop: return 'B';
        case PieceType::Rook: return 'R';
        case PieceType::Queen: return 'Q';
        case PieceType::King: return 'K';
        default: return '?';
    }
}

std::string stripDecorations(std::string s) {
    while (!s.empty() && (s.back() == '+' || s.back() == '#' || s.back() == '!' || s.back() == '?')) s.pop_back();
    for (char& c : s) {
        if (c == '0') c = 'O';  // accept 0-0 as castling
    }
    return s;
}

}  // namespace

std::string moveToSAN(const Board& before, const Move& m) {
    Piece p = before.pieceAt(m.from);
    std::string s;

    if (m.flags & FlagCastleKing) {
        s = "O-O";
    } else if (m.flags & FlagCastleQueen) {
        s = "O-O-O";
    } else {
        if (p.type == PieceType::Pawn) {
            if (m.isCapture()) s += static_cast<char>('a' + fileOf(m.from));
        } else {
            s += pieceLetter(p.type);
            bool ambiguous = false, sameFile = false, sameRank = false;
            for (const Move& other : generateLegal(before)) {
                if (other.to != m.to || other.from == m.from) continue;
                if (before.pieceAt(other.from).type != p.type) continue;
                ambiguous = true;
                if (fileOf(other.from) == fileOf(m.from)) sameFile = true;
                if (rankOf(other.from) == rankOf(m.from)) sameRank = true;
            }
            if (ambiguous) {
                if (!sameFile) s += static_cast<char>('a' + fileOf(m.from));
                else if (!sameRank) s += static_cast<char>('1' + rankOf(m.from));
                else s += squareName(m.from);
            }
        }
        if (m.isCapture()) s += 'x';
        s += squareName(m.to);
        if (m.promo != PieceType::None) {
            s += '=';
            s += pieceLetter(m.promo);
        }
    }

    Board after = before;
    after.applyMove(m);
    if (isInCheck(after, after.sideToMove)) {
        s += generateLegal(after).empty() ? '#' : '+';
    }
    return s;
}

std::optional<Move> moveFromSAN(const Board& board, const std::string& san) {
    const std::string text = stripDecorations(san);
    const std::vector<Move> legal = generateLegal(board);

    if (text == "O-O" || text == "O-O-O") {
        const uint8_t flag = text == "O-O" ? FlagCastleKing : FlagCastleQueen;
        for (const Move& m : legal) {
            if (m.flags & flag) return m;
        }
        return std::nullopt;
    }

    // [piece] [file] [rank] [x] destination [=promotion]
    size_t i = 0;
    PieceType type = PieceType::Pawn;
    auto pieceFrom = [](char c) {
        switch (c) {
            case 'N': return PieceType::Knight;
            case 'B': return PieceType::Bishop;
            case 'R': return PieceType::Rook;
            case 'Q': return PieceType::Queen;
            case 'K': return PieceType::King;
            default: return PieceType::None;
        }
    };
    if (!text.empty() && pieceFrom(text[0]) != PieceType::None) {
        type = pieceFrom(text[0]);
        i = 1;
    }
    std::string rest = text.substr(i);
    PieceType promo = PieceType::None;
    size_t eq = rest.find('=');
    if (eq != std::string::npos) {
        if (eq + 2 != rest.size()) return std::nullopt;
        promo = pieceFrom(rest[eq + 1]);
        if (promo == PieceType::None || promo == PieceType::King) return std::nullopt;
        rest.resize(eq);
    } else if (rest.size() >= 3 && pieceFrom(rest.back()) != PieceType::None && rest[rest.size() - 2] >= '1' &&
               rest[rest.size() - 2] <= '8') {
        promo = pieceFrom(rest.back());  // "e8Q"
        if (promo == PieceType::King) return std::nullopt;
        rest.pop_back();
    }
    if (rest.size() < 2) return std::nullopt;
    Square to = squareFromName(rest.substr(rest.size() - 2));
    if (to == NoSquare) return std::nullopt;

    int fileHint = -1, rankHint = -1;
    for (size_t k = 0; k + 2 < rest.size(); ++k) {
        char c = rest[k];
        if (c >= 'a' && c <= 'h') fileHint = c - 'a';
        else if (c >= '1' && c <= '8') rankHint = c - '1';
        else if (c != 'x') return std::nullopt;
    }

    std::optional<Move> found;
    for (const Move& m : legal) {
        if (m.to != to || m.promo != promo || board.pieceAt(m.from).type != type) continue;
        if (fileHint >= 0 && fileOf(m.from) != fileHint) continue;
        if (rankHint >= 0 && rankOf(m.from) != rankHint) continue;
        if (found) return std::nullopt;  // ambiguous
        found = m;
    }
    return found;
}

std::string toPGN(const Game& game, const PgnHeaders& h, const std::vector<PlyAnnotation>* annotations) {
    std::ostringstream out;
    auto tag = [&](const char* name, const std::string& value) {
        out << '[' << name << " \"" << value << "\"]\n";
    };
    std::string result = game.resultToken();
    tag("Event", h.event);
    tag("Site", h.site);
    tag("Date", h.date);
    tag("Round", h.round);
    tag("White", h.white);
    tag("Black", h.black);
    tag("Result", result);

    const Board& start = game.initialBoard();
    if (start.toFEN() != Board().toFEN()) {
        tag("SetUp", "1");
        tag("FEN", start.toFEN());
    }
    out << '\n';

    // Build movetext tokens, then wrap at 80 columns.
    std::vector<std::string> tokens;
    int moveNumber = start.fullmoveNumber;
    Color side = start.sideToMove;
    bool first = true;
    for (size_t index = 0; index < game.history().size(); ++index) {
        const HistoryEntry& e = game.history()[index];
        if (side == Color::White) {
            tokens.push_back(std::to_string(moveNumber) + ".");
        } else if (first) {
            tokens.push_back(std::to_string(moveNumber) + "...");
        }
        tokens.push_back(e.san);
        if (annotations && index < annotations->size()) {
            const PlyAnnotation& note = (*annotations)[index];
            if (note.nag > 0) tokens.push_back("$" + std::to_string(note.nag));
            if (!note.comment.empty()) {
                std::istringstream words(note.comment);
                std::string w;
                std::vector<std::string> list;
                while (words >> w) {
                    w.erase(std::remove_if(w.begin(), w.end(), [](char c) { return c == '{' || c == '}'; }), w.end());
                    if (!w.empty()) list.push_back(w);
                }
                for (size_t k = 0; k < list.size(); ++k) {
                    tokens.push_back((k == 0 ? "{" : "") + list[k] + (k + 1 == list.size() ? "}" : ""));
                }
            }
        }
        if (side == Color::Black) ++moveNumber;
        side = opposite(side);
        first = false;
    }
    tokens.push_back(result);

    size_t column = 0;
    for (size_t i = 0; i < tokens.size(); ++i) {
        if (column > 0 && column + 1 + tokens[i].size() > 80) {
            out << '\n';
            column = 0;
        }
        if (column > 0) {
            out << ' ';
            ++column;
        }
        out << tokens[i];
        column += tokens[i].size();
    }
    out << '\n';
    return out.str();
}

}  // namespace chess
