import '../core/game.dart';
import 'classify.dart';
import 'openings.dart';
import 'review.dart';
import 'uci_engine.dart';

/// Quick = 12, standard = 16, deep = 20. Shallow searches miss deep tactics (see docs/REVIEW.md section 6).
const int defaultReviewDepth = 16;

class ReviewCancelled implements Exception {
  const ReviewCancelled();
  @override
  String toString() => 'Review cancelled';
}

/// Runs a whole game through an engine and turns the numbers into a [GameReview].
/// One search per position is enough (docs/REVIEW.md section 1); results are cached by FEN.
class EngineAnalyzer {
  final UciEngine engine;
  final int depth;
  final int multiPv;
  final Map<String, List<EngineLine>> _cache = {};

  EngineAnalyzer(this.engine, {this.depth = defaultReviewDepth, this.multiPv = 3});

  /// Gives the engine its working options; call once after [UciEngine.start].
  void configure({int threads = 1, int hashMb = 128}) {
    // One thread by default: multi-threaded Stockfish was much slower on the development machine.
    engine.setOption('Threads', threads);
    engine.setOption('Hash', hashMb);
  }

  Future<List<EngineLine>> evaluate(String fen) async {
    final key = '$depth/$multiPv/$fen';
    final hit = _cache[key];
    if (hit != null) return hit;
    final lines = await engine.analyze(fen, depth: depth, multiPv: multiPv);
    return _cache[key] = lines;
  }

  /// Analyses every position of [game] and scores each move.
  /// [onProgress] gets (positions done, total). [isCancelled] is polled between positions.
  Future<GameReview> review(
    Game game, {
    OpeningBook? book,
    ReviewConfig config = const ReviewConfig(),
    void Function(int done, int total)? onProgress,
    bool Function()? isCancelled,
  }) async {
    final history = game.history;
    final fens = [for (final h in history) h.before.toFen(), game.board.toFen()];
    final evals = <PositionEval>[];
    for (var i = 0; i < fens.length; i++) {
      if (isCancelled?.call() ?? false) throw const ReviewCancelled();
      final last = i == fens.length - 1;
      if (last && game.status.isOver) {
        evals.add(const PositionEval([])); // nothing left to play, nothing to search
      } else {
        evals.add(PositionEval(await evaluate(fens[i])));
      }
      onProgress?.call(i + 1, fens.length);
    }
    return reviewGame(game, evals, book: book, config: config);
  }
}
