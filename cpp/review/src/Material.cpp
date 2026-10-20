#include "chess/Material.hpp"

#include "chess/MoveGen.hpp"

namespace chess {

namespace {

int pieceValue(PieceType t) {
    switch (t) {
        case PieceType::Pawn: return 1;
        case PieceType::Knight: return 3;
        case PieceType::Bishop: return 3;
        case PieceType::Rook: return 5;
        case PieceType::Queen: return 9;
        default: return 0;
    }
}

int material(const Board& b, Color c) {
    int total = 0;
    for (Square s = 0; s < 64; ++s) {
        Piece p = b.pieceAt(s);
        if (!p.isNone() && p.color == c) total += pieceValue(p.type);
    }
    return total;
}

}  // namespace

bool applyUci(Board& board, const std::string& uci) {
    if (uci.size() < 4) return false;
    Square from = squareFromName(uci.substr(0, 2)), to = squareFromName(uci.substr(2, 2));
    PieceType promo = PieceType::None;
    if (uci.size() == 5) {
        switch (uci[4]) {
            case 'q': promo = PieceType::Queen; break;
            case 'r': promo = PieceType::Rook; break;
            case 'b': promo = PieceType::Bishop; break;
            case 'n': promo = PieceType::Knight; break;
            default: return false;
        }
    }
    for (const Move& m : generateLegal(board)) {
        if (m.from == from && m.to == to && m.promo == promo) {
            board.applyMove(m);
            return true;
        }
    }
    return false;
}

double sacrificedValue(const Board& before, const std::string& played, const std::vector<std::string>& pv) {
    const Color mover = before.sideToMove;
    const Color other = opposite(mover);
    const int moverBefore = material(before, mover), otherBefore = material(before, other);

    Board b = before;
    if (!applyUci(b, played)) return 0;
    for (size_t i = 0; i < pv.size() && i < 3; ++i) {
        if (!applyUci(b, pv[i])) break;
    }
    const int delta = (material(b, mover) - moverBefore) - (material(b, other) - otherBefore);
    return delta < 0 ? -delta : 0;
}

}  // namespace chess
