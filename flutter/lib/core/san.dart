import 'board.dart';
import 'movegen.dart';
import 'types.dart';

String _letter(PieceType t) => switch (t) {
      PieceType.knight => 'N',
      PieceType.bishop => 'B',
      PieceType.rook => 'R',
      PieceType.queen => 'Q',
      PieceType.king => 'K',
      _ => '?',
    };

String _strip(String s) {
  var end = s.length;
  while (end > 0 && '+#!?'.contains(s[end - 1])) {
    end--;
  }
  return s.substring(0, end).replaceAll('0', 'O'); // accept 0-0 as castling
}

String _fileChar(int sq) => String.fromCharCode(97 + fileOf(sq));

/// Standard algebraic notation of [m] played from [before], including the +/# suffix.
String moveToSan(Board before, Move m) {
  final p = before.pieceAt(m.from);
  final s = StringBuffer();

  if (m.flags & flagCastleKing != 0) {
    s.write('O-O');
  } else if (m.flags & flagCastleQueen != 0) {
    s.write('O-O-O');
  } else {
    if (p.type == PieceType.pawn) {
      if (m.isCapture) s.write(_fileChar(m.from));
    } else {
      s.write(_letter(p.type));
      var ambiguous = false, sameFile = false, sameRank = false;
      for (final other in generateLegal(before)) {
        if (other.to != m.to || other.from == m.from) continue;
        if (before.pieceAt(other.from).type != p.type) continue;
        ambiguous = true;
        if (fileOf(other.from) == fileOf(m.from)) sameFile = true;
        if (rankOf(other.from) == rankOf(m.from)) sameRank = true;
      }
      if (ambiguous) {
        if (!sameFile) {
          s.write(_fileChar(m.from));
        } else if (!sameRank) {
          s.write(String.fromCharCode(49 + rankOf(m.from)));
        } else {
          s.write(squareName(m.from));
        }
      }
    }
    if (m.isCapture) s.write('x');
    s.write(squareName(m.to));
    if (m.promo != PieceType.none) s.write('=${_letter(m.promo)}');
  }

  final after = before.copy()..applyMove(m);
  if (isInCheck(after, after.sideToMove)) {
    s.write(generateLegal(after).isEmpty ? '#' : '+');
  }
  return s.toString();
}

final _sanPattern = RegExp(r'^([NBRQK])?([a-h])?([1-8])?x?([a-h][1-8])(?:=?([NBRQ]))?$');

/// Finds the legal move in [board] that [san] denotes (check/mate marks and !? are ignored).
/// Returns null when the text matches no legal move, or more than one.
Move? moveFromSan(Board board, String san) {
  final text = _strip(san);
  final legal = generateLegal(board);

  if (text == 'O-O' || text == 'O-O-O') {
    final flag = text == 'O-O' ? flagCastleKing : flagCastleQueen;
    for (final m in legal) {
      if (m.flags & flag != 0) return m;
    }
    return null;
  }

  final match = _sanPattern.firstMatch(text);
  if (match == null) return null;
  final pieceLetter = match.group(1);
  final fileHint = match.group(2), rankHint = match.group(3);
  final to = squareFromName(match.group(4)!);
  final promoLetter = match.group(5);

  const types = {
    'N': PieceType.knight,
    'B': PieceType.bishop,
    'R': PieceType.rook,
    'Q': PieceType.queen,
    'K': PieceType.king,
  };
  final type = pieceLetter == null ? PieceType.pawn : types[pieceLetter]!;
  final promo = promoLetter == null ? PieceType.none : types[promoLetter]!;

  Move? found;
  for (final m in legal) {
    if (m.to != to || m.promo != promo || board.pieceAt(m.from).type != type) continue;
    if (fileHint != null && fileOf(m.from) != fileHint.codeUnitAt(0) - 97) continue;
    if (rankHint != null && rankOf(m.from) != rankHint.codeUnitAt(0) - 49) continue;
    if (found != null) return null; // ambiguous
    found = m;
  }
  return found;
}
