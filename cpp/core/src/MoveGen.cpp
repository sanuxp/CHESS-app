#include "chess/MoveGen.hpp"

namespace chess {

namespace {

const int kKnightDeltas[8][2] = {{1, 2}, {2, 1}, {2, -1}, {1, -2}, {-1, -2}, {-2, -1}, {-2, 1}, {-1, 2}};
const int kKingDeltas[8][2] = {{1, 0}, {1, 1}, {0, 1}, {-1, 1}, {-1, 0}, {-1, -1}, {0, -1}, {1, -1}};
const int kDiagonals[4][2] = {{1, 1}, {-1, 1}, {-1, -1}, {1, -1}};
const int kOrthogonals[4][2] = {{1, 0}, {-1, 0}, {0, 1}, {0, -1}};

bool onBoard(int file, int rank) { return file >= 0 && file < 8 && rank >= 0 && rank < 8; }

bool slidingAttack(const Board& board, int file, int rank, const int (*dirs)[2], PieceType a, PieceType b, Color by) {
    for (int i = 0; i < 4; ++i) {
        int f = file + dirs[i][0], r = rank + dirs[i][1];
        while (onBoard(f, r)) {
            Piece p = board.pieceAt(makeSquare(f, r));
            if (!p.isNone()) {
                if (p.color == by && (p.type == a || p.type == b)) return true;
                break;
            }
            f += dirs[i][0];
            r += dirs[i][1];
        }
    }
    return false;
}

void addPawnMove(std::vector<Move>& out, Square from, Square to, uint8_t flags, bool promotes) {
    if (!promotes) {
        out.push_back(Move{from, to, PieceType::None, flags});
        return;
    }
    for (PieceType t : {PieceType::Queen, PieceType::Rook, PieceType::Bishop, PieceType::Knight}) {
        out.push_back(Move{from, to, t, flags});
    }
}

void generatePawn(const Board& board, Square from, Color us, std::vector<Move>& out) {
    int file = fileOf(from), rank = rankOf(from);
    int dir = us == Color::White ? 1 : -1;
    int startRank = us == Color::White ? 1 : 6;
    int lastRank = us == Color::White ? 7 : 0;

    int nextRank = rank + dir;
    if (!onBoard(file, nextRank)) return;
    bool promotes = nextRank == lastRank;

    Square one = makeSquare(file, nextRank);
    if (board.pieceAt(one).isNone()) {
        addPawnMove(out, from, one, FlagNone, promotes);
        if (rank == startRank) {
            Square two = makeSquare(file, rank + 2 * dir);
            if (board.pieceAt(two).isNone()) out.push_back(Move{from, two, PieceType::None, FlagDoublePush});
        }
    }

    for (int df = -1; df <= 1; df += 2) {
        int f = file + df;
        if (!onBoard(f, nextRank)) continue;
        Square target = makeSquare(f, nextRank);
        Piece victim = board.pieceAt(target);
        if (!victim.isNone() && victim.color != us) {
            addPawnMove(out, from, target, FlagCapture, promotes);
        } else if (victim.isNone() && target == board.epSquare) {
            out.push_back(Move{from, target, PieceType::None, static_cast<uint8_t>(FlagCapture | FlagEnPassant)});
        }
    }
}

void generateStepper(const Board& board, Square from, Color us, const int (*deltas)[2], std::vector<Move>& out) {
    int file = fileOf(from), rank = rankOf(from);
    for (int i = 0; i < 8; ++i) {
        int f = file + deltas[i][0], r = rank + deltas[i][1];
        if (!onBoard(f, r)) continue;
        Square to = makeSquare(f, r);
        Piece target = board.pieceAt(to);
        if (target.isNone()) out.push_back(Move{from, to, PieceType::None, FlagNone});
        else if (target.color != us) out.push_back(Move{from, to, PieceType::None, FlagCapture});
    }
}

void generateSlider(const Board& board, Square from, Color us, const int (*dirs)[2], std::vector<Move>& out) {
    int file = fileOf(from), rank = rankOf(from);
    for (int i = 0; i < 4; ++i) {
        int f = file + dirs[i][0], r = rank + dirs[i][1];
        while (onBoard(f, r)) {
            Square to = makeSquare(f, r);
            Piece target = board.pieceAt(to);
            if (target.isNone()) {
                out.push_back(Move{from, to, PieceType::None, FlagNone});
            } else {
                if (target.color != us) out.push_back(Move{from, to, PieceType::None, FlagCapture});
                break;
            }
            f += dirs[i][0];
            r += dirs[i][1];
        }
    }
}

void generateCastling(const Board& board, Color us, std::vector<Move>& out) {
    int rank = us == Color::White ? 0 : 7;
    Square king = makeSquare(4, rank);
    Piece kp = board.pieceAt(king);
    if (kp.type != PieceType::King || kp.color != us) return;
    Color them = opposite(us);
    uint8_t kingRight = us == Color::White ? CastleWhiteKing : CastleBlackKing;
    uint8_t queenRight = us == Color::White ? CastleWhiteQueen : CastleBlackQueen;
    if (!(board.castling & (kingRight | queenRight))) return;
    if (isSquareAttacked(board, king, them)) return;

    auto rookAt = [&](int file) {
        Piece p = board.pieceAt(makeSquare(file, rank));
        return p.type == PieceType::Rook && p.color == us;
    };
    auto empty = [&](int file) { return board.pieceAt(makeSquare(file, rank)).isNone(); };

    if ((board.castling & kingRight) && rookAt(7) && empty(5) && empty(6) &&
        !isSquareAttacked(board, makeSquare(5, rank), them) && !isSquareAttacked(board, makeSquare(6, rank), them)) {
        out.push_back(Move{king, makeSquare(6, rank), PieceType::None, FlagCastleKing});
    }
    if ((board.castling & queenRight) && rookAt(0) && empty(1) && empty(2) && empty(3) &&
        !isSquareAttacked(board, makeSquare(3, rank), them) && !isSquareAttacked(board, makeSquare(2, rank), them)) {
        out.push_back(Move{king, makeSquare(2, rank), PieceType::None, FlagCastleQueen});
    }
}

}  // namespace

bool isSquareAttacked(const Board& board, Square sq, Color by) {
    int file = fileOf(sq), rank = rankOf(sq);

    // A pawn of colour `by` attacks diagonally forward, so look one rank behind the target.
    int pawnRank = by == Color::White ? rank - 1 : rank + 1;
    for (int df = -1; df <= 1; df += 2) {
        if (!onBoard(file + df, pawnRank)) continue;
        Piece p = board.pieceAt(makeSquare(file + df, pawnRank));
        if (p.type == PieceType::Pawn && p.color == by) return true;
    }
    for (int i = 0; i < 8; ++i) {
        int f = file + kKnightDeltas[i][0], r = rank + kKnightDeltas[i][1];
        if (!onBoard(f, r)) continue;
        Piece p = board.pieceAt(makeSquare(f, r));
        if (p.type == PieceType::Knight && p.color == by) return true;
    }
    for (int i = 0; i < 8; ++i) {
        int f = file + kKingDeltas[i][0], r = rank + kKingDeltas[i][1];
        if (!onBoard(f, r)) continue;
        Piece p = board.pieceAt(makeSquare(f, r));
        if (p.type == PieceType::King && p.color == by) return true;
    }
    return slidingAttack(board, file, rank, kDiagonals, PieceType::Bishop, PieceType::Queen, by) ||
           slidingAttack(board, file, rank, kOrthogonals, PieceType::Rook, PieceType::Queen, by);
}

bool isInCheck(const Board& board, Color c) {
    Square k = board.kingSquare(c);
    return k != NoSquare && isSquareAttacked(board, k, opposite(c));
}

void generatePseudoLegal(const Board& board, std::vector<Move>& out) {
    Color us = board.sideToMove;
    for (Square s = 0; s < 64; ++s) {
        Piece p = board.pieceAt(s);
        if (p.isNone() || p.color != us) continue;
        switch (p.type) {
            case PieceType::Pawn: generatePawn(board, s, us, out); break;
            case PieceType::Knight: generateStepper(board, s, us, kKnightDeltas, out); break;
            case PieceType::Bishop: generateSlider(board, s, us, kDiagonals, out); break;
            case PieceType::Rook: generateSlider(board, s, us, kOrthogonals, out); break;
            case PieceType::Queen:
                generateSlider(board, s, us, kDiagonals, out);
                generateSlider(board, s, us, kOrthogonals, out);
                break;
            case PieceType::King: generateStepper(board, s, us, kKingDeltas, out); break;
            default: break;
        }
    }
    generateCastling(board, us, out);
}

std::vector<Move> generateLegal(const Board& board) {
    std::vector<Move> pseudo;
    pseudo.reserve(64);
    generatePseudoLegal(board, pseudo);

    Color us = board.sideToMove;
    Square king = board.kingSquare(us);
    std::vector<Move> legal;
    legal.reserve(pseudo.size());
    for (const Move& m : pseudo) {
        Board next = board;
        next.applyMove(m);
        Square k = board.pieceAt(m.from).type == PieceType::King ? m.to : king;
        if (!isSquareAttacked(next, k, opposite(us))) legal.push_back(m);
    }
    return legal;
}

uint64_t perft(const Board& board, int depth) {
    if (depth == 0) return 1;
    std::vector<Move> moves = generateLegal(board);
    if (depth == 1) return moves.size();
    uint64_t nodes = 0;
    for (const Move& m : moves) {
        Board next = board;
        next.applyMove(m);
        nodes += perft(next, depth - 1);
    }
    return nodes;
}

}  // namespace chess
