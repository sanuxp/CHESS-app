#pragma once

#include <array>
#include <string>

#include "chess/Types.hpp"

namespace chess {

// Plain position: piece placement plus the state FEN records. No history here.
class Board {
public:
    Board();  // standard starting position

    static Board startPosition();
    // Parses a FEN string. Returns false (and leaves `out` untouched) if it is malformed.
    static bool fromFEN(const std::string& fen, Board& out);
    std::string toFEN() const;

    Piece pieceAt(Square s) const { return squares_[s]; }
    void setPiece(Square s, Piece p) { squares_[s] = p; }
    Square kingSquare(Color c) const;

    // Applies a move that is known to be legal (flags must be set as MoveGen produces them).
    void applyMove(const Move& m);

    Color sideToMove = Color::White;
    uint8_t castling = 0;          // CastlingRight bits
    Square epSquare = NoSquare;    // square a pawn may capture onto en passant
    int halfmoveClock = 0;         // plies since last capture or pawn move
    int fullmoveNumber = 1;

private:
    struct EmptyTag {};
    explicit Board(EmptyTag) {}  // empty board, nothing set up

    std::array<Piece, 64> squares_{};
};

char pieceChar(Piece p);  // FEN letter: 'P' white, 'p' black, ' ' for none

}  // namespace chess
