import 'board.dart';
import 'game.dart';
import 'types.dart';

class PgnHeaders {
  final String event, site, date, round, white, black;
  const PgnHeaders({
    this.event = 'Casual Game',
    this.site = 'Local',
    this.date = '????.??.??', // YYYY.MM.DD
    this.round = '-',
    this.white = 'White',
    this.black = 'Black',
  });
}

/// Extra information written after a move: a numeric annotation glyph ($1 !, $2 ?, ...) and a comment.
class PlyAnnotation {
  final int? nag;
  final String? comment;
  const PlyAnnotation({this.nag, this.comment});
}

/// Full PGN text: seven tag roster, SetUp/FEN for custom starts, movetext wrapped at 80 columns, result.
/// Importable by Lichess, Chess.com, Arena, Scid and other engine front ends.
/// [annotations], if given, has one entry per move (for example a game review's labels and evaluations).
String toPgn(Game game, [PgnHeaders headers = const PgnHeaders(), List<PlyAnnotation?>? annotations]) {
  final out = StringBuffer();
  void tag(String name, String value) => out.write('[$name "$value"]\n');
  final result = game.resultToken;
  tag('Event', headers.event);
  tag('Site', headers.site);
  tag('Date', headers.date);
  tag('Round', headers.round);
  tag('White', headers.white);
  tag('Black', headers.black);
  tag('Result', result);

  final start = game.initial;
  if (start.toFen() != Board.start().toFen()) {
    tag('SetUp', '1');
    tag('FEN', start.toFen());
  }
  out.write('\n');

  final tokens = <String>[];
  var moveNumber = start.fullmoveNumber;
  var side = start.sideToMove;
  var first = true;
  for (var i = 0; i < game.history.length; i++) {
    final e = game.history[i];
    if (side == PieceColor.white) {
      tokens.add('$moveNumber.');
    } else if (first) {
      tokens.add('$moveNumber...');
    }
    tokens.add(e.san);
    final note = annotations != null && i < annotations.length ? annotations[i] : null;
    if (note?.nag != null) tokens.add('\$${note!.nag}');
    final words = note?.comment?.replaceAll(RegExp(r'[{}]'), '').split(RegExp(r'\s+')).where((w) => w.isNotEmpty).toList();
    if (words != null && words.isNotEmpty) {
      tokens.addAll([
        for (var w = 0; w < words.length; w++) '${w == 0 ? '{' : ''}${words[w]}${w == words.length - 1 ? '}' : ''}',
      ]);
    }
    if (side == PieceColor.black) moveNumber++;
    side = side.opposite;
    first = false;
  }
  tokens.add(result);

  var column = 0;
  for (final t in tokens) {
    if (column > 0 && column + 1 + t.length > 80) {
      out.write('\n');
      column = 0;
    }
    if (column > 0) {
      out.write(' ');
      column++;
    }
    out.write(t);
    column += t.length;
  }
  out.write('\n');
  return out.toString();
}
