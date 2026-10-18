#include "chess/Board.hpp"

#include <cctype>
#include <sstream>

namespace chess {

namespace {

const char* const kStartFEN = "rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1";

PieceType typeFromLetter(char lower) {
    switch (lower) {
        case 'p': return PieceType::Pawn;
        case 'n': return PieceType::Knight;
        case 'b': return PieceType::Bishop;
        case 'r': return PieceType::Rook;
        case 'q': return PieceType::Queen;
        case 'k': return PieceType::King;
        default: return PieceType::None;
    }
}

}  // namespace

char pieceChar(Piece p) {
    char c;
    switch (p.type) {
        case PieceType::Pawn: c = 'p'; break;
        case PieceType::Knight: c = 'n'; break;
        case PieceType::Bishop: c = 'b'; break;
        case PieceType::Rook: c = 'r'; break;
        case PieceType::Queen: c = 'q'; break;
        case PieceType::King: c = 'k'; break;
        default: return ' ';
    }
    return p.color == Color::White ? static_cast<char>(std::toupper(c)) : c;
}

Board::Board() : Board(EmptyTag{}) { fromFEN(kStartFEN, *this); }

Board Board::startPosition() { return Board(); }

bool Board::fromFEN(const std::string& fen, Board& out) {
    std::istringstream in(fen);
    std::string placement, side, castleField = "-", epField = "-";
    if (!(in >> placement >> side)) return false;
    if (in >> castleField) in >> epField;
    int half = 0, full = 1;
    if (in >> half) {
        if (!(in >> full)) full = 1;
    }
    if (half < 0 || full < 1) return false;

    Board b{EmptyTag{}};

    int file = 0, rank = 7;
    int whiteKings = 0, blackKings = 0;
    for (char c : placement) {
        if (c == '/') {
            if (file != 8 || rank == 0) return false;
            --rank;
            file = 0;
        } else if (c >= '1' && c <= '8') {
            file += c - '0';
            if (file > 8) return false;
        } else {
            PieceType t = typeFromLetter(static_cast<char>(std::tolower(static_cast<unsigned char>(c))));
            if (t == PieceType::None || file >= 8) return false;
            Color col = std::isupper(static_cast<unsigned char>(c)) ? Color::White : Color::Black;
            if (t == PieceType::Pawn && (rank == 0 || rank == 7)) return false;
            if (t == PieceType::King) (col == Color::White ? whiteKings : blackKings)++;
            b.squares_[makeSquare(file, rank)] = Piece{t, col};
            ++file;
        }
    }
    if (rank != 0 || file != 8 || whiteKings != 1 || blackKings != 1) return false;

    if (side == "w") b.sideToMove = Color::White;
    else if (side == "b") b.sideToMove = Color::Black;
    else return false;

    b.castling = 0;
    if (castleField != "-") {
        for (char c : castleField) {
            switch (c) {
                case 'K': b.castling |= CastleWhiteKing; break;
                case 'Q': b.castling |= CastleWhiteQueen; break;
                case 'k': b.castling |= CastleBlackKing; break;
                case 'q': b.castling |= CastleBlackQueen; break;
                default: return false;
            }
        }
    }

    if (epField == "-") {
        b.epSquare = NoSquare;
    } else {
        b.epSquare = squareFromName(epField);
        if (b.epSquare == NoSquare) return false;
    }

    b.halfmoveClock = half;
    b.fullmoveNumber = full;
    out = b;
    return true;
}

std::string Board::toFEN() const {
    std::string s;
    for (int rank = 7; rank >= 0; --rank) {
        int empty = 0;
        for (int file = 0; file < 8; ++file) {
            Piece p = squares_[makeSquare(file, rank)];
            if (p.isNone()) {
                ++empty;
                continue;
            }
            if (empty) {
                s += static_cast<char>('0' + empty);
                empty = 0;
            }
            s += pieceChar(p);
        }
        if (empty) s += static_cast<char>('0' + empty);
        if (rank > 0) s += '/';
    }
    s += sideToMove == Color::White ? " w " : " b ";
    if (castling == 0) {
        s += '-';
    } else {
        if (castling & CastleWhiteKing) s += 'K';
        if (castling & CastleWhiteQueen) s += 'Q';
        if (castling & CastleBlackKing) s += 'k';
        if (castling & CastleBlackQueen) s += 'q';
    }
    s += ' ';
    s += epSquare == NoSquare ? "-" : squareName(epSquare);
    s += ' ' + std::to_string(halfmoveClock) + ' ' + std::to_string(fullmoveNumber);
    return s;
}

Square Board::kingSquare(Color c) const {
    for (Square s = 0; s < 64; ++s) {
        if (squares_[s].type == PieceType::King && squares_[s].color == c) return s;
    }
    return NoSquare;
}

void Board::applyMove(const Move& m) {
    Piece moving = squares_[m.from];
    bool resetsClock = moving.type == PieceType::Pawn || m.isCapture();

    if (m.isEnPassant()) {
        squares_[m.to + (moving.color == Color::White ? -8 : 8)] = Piece{};
    }

    squares_[m.to] = moving;
    squares_[m.from] = Piece{};
    if (m.promo != PieceType::None) squares_[m.to].type = m.promo;

    if (m.flags & FlagCastleKing) {
        int rank = rankOf(m.from);
        squares_[makeSquare(5, rank)] = squares_[makeSquare(7, rank)];
        squares_[makeSquare(7, rank)] = Piece{};
    } else if (m.flags & FlagCastleQueen) {
        int rank = rankOf(m.from);
        squares_[makeSquare(3, rank)] = squares_[makeSquare(0, rank)];
        squares_[makeSquare(0, rank)] = Piece{};
    }

    // A right is lost when its king or rook leaves its home square, or the rook is captured there.
    auto strip = [this](Square s) {
        switch (s) {
            case 0: castling &= static_cast<uint8_t>(~CastleWhiteQueen); break;
            case 7: castling &= static_cast<uint8_t>(~CastleWhiteKing); break;
            case 56: castling &= static_cast<uint8_t>(~CastleBlackQueen); break;
            case 63: castling &= static_cast<uint8_t>(~CastleBlackKing); break;
            case 4: castling &= static_cast<uint8_t>(~(CastleWhiteKing | CastleWhiteQueen)); break;
            case 60: castling &= static_cast<uint8_t>(~(CastleBlackKing | CastleBlackQueen)); break;
            default: break;
        }
    };
    strip(m.from);
    strip(m.to);

    epSquare = (m.flags & FlagDoublePush) ? (m.from + m.to) / 2 : NoSquare;
    halfmoveClock = resetsClock ? 0 : halfmoveClock + 1;
    if (sideToMove == Color::Black) ++fullmoveNumber;
    sideToMove = opposite(sideToMove);
}

}  // namespace chess
