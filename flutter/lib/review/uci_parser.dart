import 'classify.dart';

/// One `info ... pv ...` line from a UCI engine; scores are from the side to move's point of view.
class UciInfo {
  final int depth;
  final int multipv; // 1 = best line
  final EngineScore score;
  final List<String> pv; // UCI moves
  const UciInfo(this.depth, this.multipv, this.score, this.pv);
}

/// Parses an `info` line. Returns null for lines without a score and a variation (currmove, string, ...)
/// and for aspiration-window bound lines (lowerbound/upperbound), which are not real results.
UciInfo? parseInfo(String line) {
  if (!line.startsWith('info ')) return null;
  final t = line.trim().split(RegExp(r'\s+'));
  var depth = 0, multipv = 1;
  EngineScore? score;
  var pv = <String>[];
  for (var i = 1; i < t.length; i++) {
    switch (t[i]) {
      case 'depth':
        depth = int.tryParse(t[++i]) ?? depth;
      case 'multipv':
        multipv = int.tryParse(t[++i]) ?? multipv;
      case 'score':
        final kind = t[++i];
        final value = int.tryParse(t[++i]);
        if (value == null) return null;
        score = kind == 'mate' ? EngineScore.mate(value) : EngineScore.cp(value);
        if (i + 1 < t.length && (t[i + 1] == 'lowerbound' || t[i + 1] == 'upperbound')) return null;
      case 'pv':
        pv = t.sublist(i + 1);
        i = t.length;
    }
  }
  if (score == null || pv.isEmpty) return null;
  return UciInfo(depth, multipv, score, pv);
}

/// The move in a `bestmove` line ("(none)" when there is no legal move), or null for other lines.
String? parseBestMove(String line) {
  if (!line.startsWith('bestmove')) return null;
  final t = line.trim().split(RegExp(r'\s+'));
  return t.length > 1 ? t[1] : '(none)';
}
