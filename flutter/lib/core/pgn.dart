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

/// Full PGN text: seven tag roster, SetUp/FEN for custom starts, movetext wrapped at 80 columns, result.
/// Importable by Lichess, Chess.com, Arena, Scid and other engine front ends.
String toPgn(Game game, [PgnHeaders headers = const PgnHeaders()]) {
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
  for (final e in game.history) {
    if (side == PieceColor.white) {
      tokens.add('$moveNumber.');
    } else if (first) {
      tokens.add('$moveNumber...');
    }
    tokens.add(e.san);
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
