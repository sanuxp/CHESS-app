#pragma once

#include <cstdint>
#include <string>

namespace chess {

// Squares are numbered 0..63 with a1 = 0, b1 = 1, ... h1 = 7, a2 = 8, ... h8 = 63.
using Square = int;
constexpr Square NoSquare = -1;

inline int fileOf(Square s) { return s & 7; }
inline int rankOf(Square s) { return s >> 3; }
inline Square makeSquare(int file, int rank) { return rank * 8 + file; }

enum class Color : uint8_t { White, Black };

inline Color opposite(Color c) { return c == Color::White ? Color::Black : Color::White; }

enum class PieceType : uint8_t { None, Pawn, Knight, Bishop, Rook, Queen, King };

struct Piece {
    PieceType type = PieceType::None;
    Color color = Color::White;

    bool isNone() const { return type == PieceType::None; }
    bool operator==(const Piece& o) const { return type == o.type && (type == PieceType::None || color == o.color); }
    bool operator!=(const Piece& o) const { return !(*this == o); }
};

enum MoveFlag : uint8_t {
    FlagNone = 0,
    FlagCapture = 1,
    FlagDoublePush = 2,
    FlagEnPassant = 4,
    FlagCastleKing = 8,
    FlagCastleQueen = 16,
};

struct Move {
    Square from = 0;
    Square to = 0;
    PieceType promo = PieceType::None;
    uint8_t flags = FlagNone;

    bool isCapture() const { return (flags & FlagCapture) != 0; }
    bool isEnPassant() const { return (flags & FlagEnPassant) != 0; }
    bool isCastle() const { return (flags & (FlagCastleKing | FlagCastleQueen)) != 0; }

    // Two moves are the same move when origin, target and promotion agree.
    bool operator==(const Move& o) const { return from == o.from && to == o.to && promo == o.promo; }
    bool operator!=(const Move& o) const { return !(*this == o); }
};

enum CastlingRight : uint8_t {
    CastleWhiteKing = 1,
    CastleWhiteQueen = 2,
    CastleBlackKing = 4,
    CastleBlackQueen = 8,
};

inline std::string squareName(Square s) {
    if (s < 0 || s > 63) return "-";
    std::string r;
    r += static_cast<char>('a' + fileOf(s));
    r += static_cast<char>('1' + rankOf(s));
    return r;
}

// Returns NoSquare when the text is not a valid square such as "e4".
inline Square squareFromName(const std::string& n) {
    if (n.size() != 2 || n[0] < 'a' || n[0] > 'h' || n[1] < '1' || n[1] > '8') return NoSquare;
    return makeSquare(n[0] - 'a', n[1] - '1');
}

}  // namespace chess
