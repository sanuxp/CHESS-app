/// Basic chess types. Squares are ints 0..63 with a1 = 0, h1 = 7, a8 = 56.
library;

enum PieceColor {
  white,
  black;

  PieceColor get opposite => this == white ? black : white;
}

enum PieceType { none, pawn, knight, bishop, rook, queen, king }

class Piece {
  final PieceType type;
  final PieceColor color;
  const Piece(this.type, this.color);

  static const empty = Piece(PieceType.none, PieceColor.white);

  bool get isNone => type == PieceType.none;

  @override
  bool operator ==(Object other) =>
      other is Piece && type == other.type && (type == PieceType.none || color == other.color);

  @override
  int get hashCode => type == PieceType.none ? 0 : type.index * 2 + color.index + 1;
}

const int noSquare = -1;

int fileOf(int s) => s & 7;
int rankOf(int s) => s >> 3;
int makeSquare(int file, int rank) => rank * 8 + file;

String squareName(int s) =>
    (s < 0 || s > 63) ? '-' : String.fromCharCode(97 + fileOf(s)) + String.fromCharCode(49 + rankOf(s));

/// Returns [noSquare] when [name] is not a square like "e4".
int squareFromName(String name) {
  if (name.length != 2) return noSquare;
  final f = name.codeUnitAt(0) - 97, r = name.codeUnitAt(1) - 49;
  if (f < 0 || f > 7 || r < 0 || r > 7) return noSquare;
  return makeSquare(f, r);
}

const int flagCapture = 1;
const int flagDoublePush = 2;
const int flagEnPassant = 4;
const int flagCastleKing = 8;
const int flagCastleQueen = 16;

const int castleWhiteKing = 1;
const int castleWhiteQueen = 2;
const int castleBlackKing = 4;
const int castleBlackQueen = 8;

class Move {
  final int from;
  final int to;
  final PieceType promo;
  final int flags;
  const Move(this.from, this.to, [this.promo = PieceType.none, this.flags = 0]);

  bool get isCapture => flags & flagCapture != 0;
  bool get isEnPassant => flags & flagEnPassant != 0;
  bool get isCastle => flags & (flagCastleKing | flagCastleQueen) != 0;

  /// Move written like "e2e4" or "e7e8q".
  String get uci {
    const letters = {
      PieceType.queen: 'q',
      PieceType.rook: 'r',
      PieceType.bishop: 'b',
      PieceType.knight: 'n',
    };
    return squareName(from) + squareName(to) + (letters[promo] ?? '');
  }

  // Same move = same origin, target and promotion (flags are derived).
  @override
  bool operator ==(Object other) =>
      other is Move && from == other.from && to == other.to && promo == other.promo;

  @override
  int get hashCode => Object.hash(from, to, promo);

  @override
  String toString() => uci;
}
