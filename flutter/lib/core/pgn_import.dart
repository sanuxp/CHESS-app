import 'board.dart';
import 'game.dart';

/// The result of reading PGN text: the game, its tags, the result token and an error if a move was illegal.
class PgnImport {
  final Game game;
  final Map<String, String> headers;
  final String result; // "1-0", "0-1", "1/2-1/2" or "*"
  final String? error; // set when reading stopped early (the moves before it are still in [game])
  const PgnImport(this.game, this.headers, this.result, this.error);
}

final _tag = RegExp(r'^\s*\[\s*(\w+)\s+"((?:[^"\\]|\\.)*)"\s*\]\s*$');
final _moveNumber = RegExp(r'^\d+\.+');
const _results = {'1-0', '0-1', '1/2-1/2', '*'};

/// Reads the first game in [text]. Comments, variations, NAGs and engine annotations are ignored.
PgnImport importPgn(String text) {
  final headers = <String, String>{};
  final body = StringBuffer();
  var inHeaders = true;
  for (final line in text.split(RegExp(r'\r?\n'))) {
    if (inHeaders) {
      final m = _tag.firstMatch(line);
      if (m != null) {
        headers[m.group(1)!] = m.group(2)!.replaceAll(r'\"', '"').replaceAll(r'\\', r'\');
        continue;
      }
      if (line.trim().isEmpty) continue;
      inHeaders = false;
    }
    body.writeln(line);
  }

  var start = Board.start();
  final fen = headers['FEN'];
  if (fen != null) {
    final b = Board.fromFen(fen);
    if (b == null) return PgnImport(Game(), headers, '*', 'Invalid FEN in the PGN tags');
    start = b;
  }
  final game = Game(start);

  var result = headers['Result'] ?? '*';
  if (!_results.contains(result)) result = '*';
  String? error;

  final s = body.toString();
  var i = 0, variationDepth = 0;
  var moveNo = start.fullmoveNumber;
  final token = StringBuffer();

  // Returns false when reading must stop (result token or illegal move).
  bool flush() {
    if (token.isEmpty) return true;
    var t = token.toString();
    token.clear();
    if (variationDepth > 0) return true;
    if (_results.contains(t)) {
      result = t;
      return false;
    }
    if (t.startsWith(r'$')) return true; // NAG
    t = t.replaceFirst(_moveNumber, '');
    if (t.isEmpty) return true;
    if (!game.makeMoveSan(t)) {
      error = 'Illegal or unreadable move "$t" (move $moveNo)';
      return false;
    }
    if (game.board.sideToMove.index == 0) moveNo++;
    return true;
  }

  loop:
  while (i < s.length) {
    final ch = s[i];
    switch (ch) {
      case '{':
        if (!flush()) break loop;
        final end = s.indexOf('}', i);
        i = end < 0 ? s.length : end + 1;
      case ';':
        if (!flush()) break loop;
        final end = s.indexOf('\n', i);
        i = end < 0 ? s.length : end + 1;
      case '(':
        if (!flush()) break loop;
        variationDepth++;
        i++;
      case ')':
        if (!flush()) break loop;
        if (variationDepth > 0) variationDepth--;
        i++;
      case ' ' || '\t' || '\n' || '\r':
        if (!flush()) break loop;
        i++;
      default:
        token.write(ch);
        i++;
    }
  }
  if (error == null) flush();

  return PgnImport(game, headers, result, error);
}
