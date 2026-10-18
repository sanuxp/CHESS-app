import 'types.dart';

const String startFen = 'rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1';

/// A position: piece placement plus the state FEN records. No history.
class Board {
  final List<Piece> _sq;
  PieceColor sideToMove;
  int castling;
  int epSquare;
  int halfmoveClock;
  int fullmoveNumber;

  Board._(this._sq, this.sideToMove, this.castling, this.epSquare, this.halfmoveClock, this.fullmoveNumber);

  factory Board.start() => Board.fromFen(startFen)!;

  Board copy() => Board._(List<Piece>.of(_sq), sideToMove, castling, epSquare, halfmoveClock, fullmoveNumber);

  Piece pieceAt(int s) => _sq[s];
  void setPiece(int s, Piece p) => _sq[s] = p;

  int kingSquare(PieceColor c) {
    for (var s = 0; s < 64; s++) {
      if (_sq[s].type == PieceType.king && _sq[s].color == c) return s;
    }
    return noSquare;
  }

  /// Parses a FEN string; returns null if it is malformed.
  static Board? fromFen(String fen) {
    final parts = fen.trim().split(RegExp(r'\s+'));
    if (parts.length < 2) return null;
    final placement = parts[0], side = parts[1];
    final castleField = parts.length > 2 ? parts[2] : '-';
    final epField = parts.length > 3 ? parts[3] : '-';
    var half = 0, full = 1;
    if (parts.length > 4) {
      final h = int.tryParse(parts[4]);
      if (h == null) return null;
      half = h;
      if (parts.length > 5) full = int.tryParse(parts[5]) ?? 1;
    }
    if (half < 0 || full < 1) return null;

    final sq = List<Piece>.filled(64, Piece.empty);
    var file = 0, rank = 7, whiteKings = 0, blackKings = 0;
    const letters = {
      'p': PieceType.pawn,
      'n': PieceType.knight,
      'b': PieceType.bishop,
      'r': PieceType.rook,
      'q': PieceType.queen,
      'k': PieceType.king,
    };
    for (final ch in placement.split('')) {
      final code = ch.codeUnitAt(0);
      if (ch == '/') {
        if (file != 8 || rank == 0) return null;
        rank--;
        file = 0;
      } else if (code >= 49 && code <= 56) {
        file += code - 48;
        if (file > 8) return null;
      } else {
        final t = letters[ch.toLowerCase()];
        if (t == null || file >= 8) return null;
        final color = ch == ch.toUpperCase() ? PieceColor.white : PieceColor.black;
        if (t == PieceType.pawn && (rank == 0 || rank == 7)) return null;
        if (t == PieceType.king) {
          color == PieceColor.white ? whiteKings++ : blackKings++;
        }
        sq[makeSquare(file, rank)] = Piece(t, color);
        file++;
      }
    }
    if (rank != 0 || file != 8 || whiteKings != 1 || blackKings != 1) return null;

    final PieceColor sideToMove;
    if (side == 'w') {
      sideToMove = PieceColor.white;
    } else if (side == 'b') {
      sideToMove = PieceColor.black;
    } else {
      return null;
    }

    var castling = 0;
    if (castleField != '-') {
      for (final c in castleField.split('')) {
        switch (c) {
          case 'K':
            castling |= castleWhiteKing;
          case 'Q':
            castling |= castleWhiteQueen;
          case 'k':
            castling |= castleBlackKing;
          case 'q':
            castling |= castleBlackQueen;
          default:
            return null;
        }
      }
    }

    var ep = noSquare;
    if (epField != '-') {
      ep = squareFromName(epField);
      if (ep == noSquare) return null;
    }
    return Board._(sq, sideToMove, castling, ep, half, full);
  }

  static String _pieceChar(Piece p) {
    const chars = {
      PieceType.pawn: 'p',
      PieceType.knight: 'n',
      PieceType.bishop: 'b',
      PieceType.rook: 'r',
      PieceType.queen: 'q',
      PieceType.king: 'k',
    };
    final c = chars[p.type] ?? ' ';
    return p.color == PieceColor.white ? c.toUpperCase() : c;
  }

  String toFen() {
    final b = StringBuffer();
    for (var rank = 7; rank >= 0; rank--) {
      var empty = 0;
      for (var file = 0; file < 8; file++) {
        final p = _sq[makeSquare(file, rank)];
        if (p.isNone) {
          empty++;
          continue;
        }
        if (empty > 0) {
          b.write(empty);
          empty = 0;
        }
        b.write(_pieceChar(p));
      }
      if (empty > 0) b.write(empty);
      if (rank > 0) b.write('/');
    }
    b.write(sideToMove == PieceColor.white ? ' w ' : ' b ');
    if (castling == 0) {
      b.write('-');
    } else {
      if (castling & castleWhiteKing != 0) b.write('K');
      if (castling & castleWhiteQueen != 0) b.write('Q');
      if (castling & castleBlackKing != 0) b.write('k');
      if (castling & castleBlackQueen != 0) b.write('q');
    }
    b.write(' ${epSquare == noSquare ? '-' : squareName(epSquare)} $halfmoveClock $fullmoveNumber');
    return b.toString();
  }

  /// Applies a move that is known to be legal (flags as produced by move generation).
  void applyMove(Move m) {
    final moving = _sq[m.from];
    final resetsClock = moving.type == PieceType.pawn || m.isCapture;

    if (m.isEnPassant) {
      _sq[m.to + (moving.color == PieceColor.white ? -8 : 8)] = Piece.empty;
    }
    _sq[m.to] = m.promo != PieceType.none ? Piece(m.promo, moving.color) : moving;
    _sq[m.from] = Piece.empty;

    if (m.flags & flagCastleKing != 0) {
      final rank = rankOf(m.from);
      _sq[makeSquare(5, rank)] = _sq[makeSquare(7, rank)];
      _sq[makeSquare(7, rank)] = Piece.empty;
    } else if (m.flags & flagCastleQueen != 0) {
      final rank = rankOf(m.from);
      _sq[makeSquare(3, rank)] = _sq[makeSquare(0, rank)];
      _sq[makeSquare(0, rank)] = Piece.empty;
    }

    // A right is lost when its king or rook leaves home, or the rook is captured there.
    void strip(int s) {
      switch (s) {
        case 0:
          castling &= ~castleWhiteQueen;
        case 7:
          castling &= ~castleWhiteKing;
        case 56:
          castling &= ~castleBlackQueen;
        case 63:
          castling &= ~castleBlackKing;
        case 4:
          castling &= ~(castleWhiteKing | castleWhiteQueen);
        case 60:
          castling &= ~(castleBlackKing | castleBlackQueen);
      }
    }

    strip(m.from);
    strip(m.to);

    epSquare = (m.flags & flagDoublePush != 0) ? (m.from + m.to) ~/ 2 : noSquare;
    halfmoveClock = resetsClock ? 0 : halfmoveClock + 1;
    if (sideToMove == PieceColor.black) fullmoveNumber++;
    sideToMove = sideToMove.opposite;
  }
}
