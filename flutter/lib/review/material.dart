import '../core/board.dart';
import '../core/movegen.dart';
import '../core/types.dart';

const _value = {
  PieceType.pawn: 1,
  PieceType.knight: 3,
  PieceType.bishop: 3,
  PieceType.rook: 5,
  PieceType.queen: 9,
};

int _material(Board b, PieceColor c) {
  var total = 0;
  for (var s = 0; s < 64; s++) {
    final p = b.pieceAt(s);
    if (!p.isNone && p.color == c) total += _value[p.type] ?? 0;
  }
  return total;
}

/// Applies a UCI move if it is legal; returns false otherwise.
bool applyUci(Board board, String uci) {
  if (uci.length < 4) return false;
  final from = squareFromName(uci.substring(0, 2)), to = squareFromName(uci.substring(2, 4));
  const promos = {'q': PieceType.queen, 'r': PieceType.rook, 'b': PieceType.bishop, 'n': PieceType.knight};
  final promo = uci.length == 5 ? (promos[uci[4]] ?? PieceType.none) : PieceType.none;
  for (final m in generateLegal(board)) {
    if (m.from == from && m.to == to && m.promo == promo) {
      board.applyMove(m);
      return true;
    }
  }
  return false;
}

/// Material in pawns the mover gives up by playing [played] from [before], judged over the move and the
/// first three moves of the engine's variation [pv] that follows it. Trades score 0 (docs/REVIEW.md section 4).
double sacrificedValue(Board before, String played, List<String> pv) {
  final mover = before.sideToMove;
  final other = mover.opposite;
  final moverBefore = _material(before, mover), otherBefore = _material(before, other);

  final b = before.copy();
  if (!applyUci(b, played)) return 0;
  for (final m in pv.take(3)) {
    if (!applyUci(b, m)) break;
  }
  final delta = (_material(b, mover) - moverBefore) - (_material(b, other) - otherBefore);
  return delta < 0 ? -delta.toDouble() : 0;
}
