import '../core/board.dart';
import '../core/san.dart';

class Opening {
  final String eco;
  final String name;
  const Opening(this.eco, this.name);
}

/// Opening database (docs/data/openings.tsv, Lichess chess-openings, CC0). Positions are keyed by the
/// first four FEN fields so transpositions match.
class OpeningBook {
  final Set<String> _positions = {};
  final Map<String, Opening> _names = {};

  OpeningBook._();

  /// Parses the TSV (`eco<TAB>name<TAB>pgn`, first line is a header).
  factory OpeningBook.parse(String tsv) {
    final book = OpeningBook._();
    final lines = tsv.split('\n');
    for (var i = 1; i < lines.length; i++) {
      final cols = lines[i].trimRight().split('\t');
      if (cols.length < 3) continue;
      final board = Board.start();
      var ok = true;
      for (final token in cols[2].split(' ')) {
        if (token.isEmpty || token.contains('.')) {
          // "1." or "1.e4": keep what follows the dot
          final san = token.contains('.') ? token.substring(token.lastIndexOf('.') + 1) : '';
          if (san.isEmpty) continue;
          ok = book._play(board, san);
        } else {
          ok = book._play(board, token);
        }
        if (!ok) break;
      }
      if (ok) book._names[key(board)] = Opening(cols[0], cols[1]);
    }
    return book;
  }

  bool _play(Board board, String san) {
    final m = moveFromSan(board, san);
    if (m == null) return false;
    board.applyMove(m);
    _positions.add(key(board));
    return true;
  }

  /// Placement, side, castling and en passant of [b].
  static String key(Board b) => b.toFen().split(' ').take(4).join(' ');

  int get size => _names.length;

  bool contains(Board b) => _positions.contains(key(b));

  /// The opening that ends exactly at [b], if any.
  Opening? nameOf(Board b) => _names[key(b)];
}
