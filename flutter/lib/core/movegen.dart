import 'board.dart';
import 'types.dart';

const _knight = [
  [1, 2], [2, 1], [2, -1], [1, -2], [-1, -2], [-2, -1], [-2, 1], [-1, 2], //
];
const _king = [
  [1, 0], [1, 1], [0, 1], [-1, 1], [-1, 0], [-1, -1], [0, -1], [1, -1], //
];
const _diagonals = [
  [1, 1], [-1, 1], [-1, -1], [1, -1], //
];
const _orthogonals = [
  [1, 0], [-1, 0], [0, 1], [0, -1], //
];

bool _on(int f, int r) => f >= 0 && f < 8 && r >= 0 && r < 8;

bool _slides(Board b, int file, int rank, List<List<int>> dirs, PieceType a, PieceType c, PieceColor by) {
  for (final d in dirs) {
    var f = file + d[0], r = rank + d[1];
    while (_on(f, r)) {
      final p = b.pieceAt(makeSquare(f, r));
      if (!p.isNone) {
        if (p.color == by && (p.type == a || p.type == c)) return true;
        break;
      }
      f += d[0];
      r += d[1];
    }
  }
  return false;
}

/// True if any piece of colour [by] attacks [sq].
bool isSquareAttacked(Board b, int sq, PieceColor by) {
  final file = fileOf(sq), rank = rankOf(sq);
  final pawnRank = by == PieceColor.white ? rank - 1 : rank + 1;
  for (final df in const [-1, 1]) {
    if (!_on(file + df, pawnRank)) continue;
    final p = b.pieceAt(makeSquare(file + df, pawnRank));
    if (p.type == PieceType.pawn && p.color == by) return true;
  }
  for (final d in _knight) {
    if (!_on(file + d[0], rank + d[1])) continue;
    final p = b.pieceAt(makeSquare(file + d[0], rank + d[1]));
    if (p.type == PieceType.knight && p.color == by) return true;
  }
  for (final d in _king) {
    if (!_on(file + d[0], rank + d[1])) continue;
    final p = b.pieceAt(makeSquare(file + d[0], rank + d[1]));
    if (p.type == PieceType.king && p.color == by) return true;
  }
  return _slides(b, file, rank, _diagonals, PieceType.bishop, PieceType.queen, by) ||
      _slides(b, file, rank, _orthogonals, PieceType.rook, PieceType.queen, by);
}

bool isInCheck(Board b, PieceColor c) {
  final k = b.kingSquare(c);
  return k != noSquare && isSquareAttacked(b, k, c.opposite);
}

void _pawnMove(List<Move> out, int from, int to, int flags, bool promotes) {
  if (!promotes) {
    out.add(Move(from, to, PieceType.none, flags));
    return;
  }
  for (final t in const [PieceType.queen, PieceType.rook, PieceType.bishop, PieceType.knight]) {
    out.add(Move(from, to, t, flags));
  }
}

void _pawn(Board b, int from, PieceColor us, List<Move> out) {
  final file = fileOf(from), rank = rankOf(from);
  final dir = us == PieceColor.white ? 1 : -1;
  final startRank = us == PieceColor.white ? 1 : 6;
  final lastRank = us == PieceColor.white ? 7 : 0;
  final next = rank + dir;
  if (!_on(file, next)) return;
  final promotes = next == lastRank;

  final one = makeSquare(file, next);
  if (b.pieceAt(one).isNone) {
    _pawnMove(out, from, one, 0, promotes);
    if (rank == startRank) {
      final two = makeSquare(file, rank + 2 * dir);
      if (b.pieceAt(two).isNone) out.add(Move(from, two, PieceType.none, flagDoublePush));
    }
  }
  for (final df in const [-1, 1]) {
    final f = file + df;
    if (!_on(f, next)) continue;
    final target = makeSquare(f, next);
    final victim = b.pieceAt(target);
    if (!victim.isNone && victim.color != us) {
      _pawnMove(out, from, target, flagCapture, promotes);
    } else if (victim.isNone && target == b.epSquare) {
      out.add(Move(from, target, PieceType.none, flagCapture | flagEnPassant));
    }
  }
}

void _step(Board b, int from, PieceColor us, List<List<int>> deltas, List<Move> out) {
  final file = fileOf(from), rank = rankOf(from);
  for (final d in deltas) {
    final f = file + d[0], r = rank + d[1];
    if (!_on(f, r)) continue;
    final to = makeSquare(f, r);
    final t = b.pieceAt(to);
    if (t.isNone) {
      out.add(Move(from, to));
    } else if (t.color != us) {
      out.add(Move(from, to, PieceType.none, flagCapture));
    }
  }
}

void _slide(Board b, int from, PieceColor us, List<List<int>> dirs, List<Move> out) {
  final file = fileOf(from), rank = rankOf(from);
  for (final d in dirs) {
    var f = file + d[0], r = rank + d[1];
    while (_on(f, r)) {
      final to = makeSquare(f, r);
      final t = b.pieceAt(to);
      if (t.isNone) {
        out.add(Move(from, to));
      } else {
        if (t.color != us) out.add(Move(from, to, PieceType.none, flagCapture));
        break;
      }
      f += d[0];
      r += d[1];
    }
  }
}

void _castling(Board b, PieceColor us, List<Move> out) {
  final rank = us == PieceColor.white ? 0 : 7;
  final king = makeSquare(4, rank);
  final kp = b.pieceAt(king);
  if (kp.type != PieceType.king || kp.color != us) return;
  final them = us.opposite;
  final kingRight = us == PieceColor.white ? castleWhiteKing : castleBlackKing;
  final queenRight = us == PieceColor.white ? castleWhiteQueen : castleBlackQueen;
  if (b.castling & (kingRight | queenRight) == 0) return;
  if (isSquareAttacked(b, king, them)) return;

  bool rookAt(int file) {
    final p = b.pieceAt(makeSquare(file, rank));
    return p.type == PieceType.rook && p.color == us;
  }

  bool empty(int file) => b.pieceAt(makeSquare(file, rank)).isNone;
  bool safe(int file) => !isSquareAttacked(b, makeSquare(file, rank), them);

  if (b.castling & kingRight != 0 && rookAt(7) && empty(5) && empty(6) && safe(5) && safe(6)) {
    out.add(Move(king, makeSquare(6, rank), PieceType.none, flagCastleKing));
  }
  if (b.castling & queenRight != 0 && rookAt(0) && empty(1) && empty(2) && empty(3) && safe(3) && safe(2)) {
    out.add(Move(king, makeSquare(2, rank), PieceType.none, flagCastleQueen));
  }
}

/// Moves that obey piece movement but may leave the own king in check.
List<Move> generatePseudoLegal(Board b) {
  final out = <Move>[];
  final us = b.sideToMove;
  for (var s = 0; s < 64; s++) {
    final p = b.pieceAt(s);
    if (p.isNone || p.color != us) continue;
    switch (p.type) {
      case PieceType.pawn:
        _pawn(b, s, us, out);
      case PieceType.knight:
        _step(b, s, us, _knight, out);
      case PieceType.bishop:
        _slide(b, s, us, _diagonals, out);
      case PieceType.rook:
        _slide(b, s, us, _orthogonals, out);
      case PieceType.queen:
        _slide(b, s, us, _diagonals, out);
        _slide(b, s, us, _orthogonals, out);
      case PieceType.king:
        _step(b, s, us, _king, out);
      case PieceType.none:
        break;
    }
  }
  _castling(b, us, out);
  return out;
}

/// Fully legal moves for the side to move. Promotions appear as four separate moves.
List<Move> generateLegal(Board b) {
  final us = b.sideToMove;
  final king = b.kingSquare(us);
  final legal = <Move>[];
  for (final m in generatePseudoLegal(b)) {
    final next = b.copy()..applyMove(m);
    final k = b.pieceAt(m.from).type == PieceType.king ? m.to : king;
    if (!isSquareAttacked(next, k, us.opposite)) legal.add(m);
  }
  return legal;
}

/// Leaf-node count of the legal move tree: the standard move-generation benchmark.
int perft(Board b, int depth) {
  if (depth == 0) return 1;
  final moves = generateLegal(b);
  if (depth == 1) return moves.length;
  var nodes = 0;
  for (final m in moves) {
    nodes += perft(b.copy()..applyMove(m), depth - 1);
  }
  return nodes;
}
