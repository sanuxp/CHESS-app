#include "chess/Pgn.hpp"

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
    std::string wanted = stripDecorations(san);
    for (const Move& m : generateLegal(board)) {
        if (stripDecorations(moveToSAN(board, m)) == wanted) return m;
    }
    return std::nullopt;
}

std::string toPGN(const Game& game, const PgnHeaders& h) {
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
    for (const HistoryEntry& e : game.history()) {
        if (side == Color::White) {
            tokens.push_back(std::to_string(moveNumber) + ".");
        } else if (first) {
            tokens.push_back(std::to_string(moveNumber) + "...");
        }
        tokens.push_back(e.san);
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
