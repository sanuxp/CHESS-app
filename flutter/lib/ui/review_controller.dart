import 'package:flutter/foundation.dart';
import 'package:flutter/services.dart' show rootBundle;

import '../core/board.dart';
import '../core/game.dart';
import '../core/pgn.dart';
import '../core/pgn_import.dart';
import '../review/analyzer.dart';
import '../review/engine_factory.dart';
import '../review/openings.dart';
import '../review/review.dart';
import '../review/uci_engine.dart';

enum ReviewPhase { setup, running, done, error }

/// How hard Stockfish thinks about each position (see docs/REVIEW.md section 6).
enum ReviewDepth {
  quick(12, 'Quick'),
  standard(16, 'Standard'),
  deep(20, 'Deep');

  final int depth;
  final String title;
  const ReviewDepth(this.depth, this.title);
}

/// State of one game review: which game, setup, progress, results and which move is being looked at.
class ReviewController extends ChangeNotifier {
  /// Opens the engine connection. Replaced in tests.
  final Future<UciTransport?> Function() openTransport;

  /// Loads the opening database. Replaced in tests.
  final Future<OpeningBook> Function() loadBook;

  ReviewController({
    Future<UciTransport?> Function()? openTransport,
    Future<OpeningBook> Function()? loadBook,
    Game? game,
    String? source,
  })  : openTransport = openTransport ?? openEngineTransport,
        loadBook = loadBook ?? _loadBundledBook {
    if (game != null) setGame(game, source: source);
  }

  ReviewPhase phase = ReviewPhase.setup;
  ReviewDepth depth = ReviewDepth.standard;

  /// The game being reviewed (a private copy, so playing on does not change the review).
  Game? game;
  PgnHeaders headers = const PgnHeaders();
  String source = 'the current game';
  String? importWarning;

  int done = 0, total = 0;
  String? error;
  GameReview? review;

  /// Position being shown: 0 = start, n = after n moves.
  int cursor = 0;
  bool flipped = false;
  bool showBestMove = true;

  bool _cancel = false;
  UciEngine? _engine;
  bool _disposed = false;

  static OpeningBook? _bundled;
  static Future<OpeningBook> _loadBundledBook() async =>
      _bundled ??= OpeningBook.parse(await rootBundle.loadString('assets/data/openings.tsv'));

  int get moveCount => game?.moveCount ?? 0;

  /// Positions 0..n of the reviewed game.
  Board positionAt(int index) {
    final g = game!;
    return index < g.moveCount ? g.history[index].before : g.board;
  }

  /// Replaces the game under review with a copy of [source] and returns to the setup screen.
  void setGame(Game from, {String? source, PgnHeaders? headers, String? warning}) {
    final copy = Game(from.initial);
    for (final h in from.history) {
      copy.makeMove(h.move.from, h.move.to, h.move.promo);
    }
    game = copy;
    if (source != null) this.source = source;
    if (headers != null) this.headers = headers;
    importWarning = warning;
    review = null;
    error = null;
    cursor = 0;
    phase = ReviewPhase.setup;
    notifyListeners();
  }

  /// Reads PGN text (from a file) as the game to review.
  bool loadPgn(String text, {String name = 'the opened file'}) {
    final r = importPgn(text);
    if (r.game.moveCount == 0) {
      error = r.error ?? 'No moves were found in that PGN.';
      phase = ReviewPhase.error;
      notifyListeners();
      return false;
    }
    final h = r.headers;
    setGame(
      r.game,
      source: name,
      headers: PgnHeaders(
        event: h['Event'] ?? 'Casual Game',
        site: h['Site'] ?? 'Local',
        date: h['Date'] ?? '????.??.??',
        round: h['Round'] ?? '-',
        white: h['White'] ?? 'White',
        black: h['Black'] ?? 'Black',
      ),
      warning: r.error == null ? null : 'Reading stopped early: ${r.error}',
    );
    return true;
  }

  void setDepth(ReviewDepth d) {
    depth = d;
    notifyListeners();
  }

  /// Runs the review. Progress and the result arrive through [notifyListeners].
  Future<void> start() async {
    final g = game;
    if (g == null || g.moveCount == 0 || phase == ReviewPhase.running) return;
    phase = ReviewPhase.running;
    done = 0;
    total = g.moveCount + 1;
    error = null;
    _cancel = false;
    notifyListeners();

    try {
      final transport = await openTransport();
      if (transport == null) throw EngineException(engineSearchHint());
      final engine = _engine = UciEngine(transport);
      await engine.start();
      final analyzer = EngineAnalyzer(engine, depth: depth.depth)..configure(threads: defaultEngineThreads());
      final book = await loadBook();
      final result = await analyzer.review(
        g,
        book: book,
        onProgress: (d, t) {
          done = d;
          total = t;
          if (!_disposed) notifyListeners();
        },
        isCancelled: () => _cancel,
      );
      review = result;
      cursor = 0;
      phase = ReviewPhase.done;
    } on ReviewCancelled {
      phase = ReviewPhase.setup;
    } catch (e) {
      error = e.toString();
      phase = ReviewPhase.error;
    } finally {
      final engine = _engine;
      _engine = null;
      try {
        await engine?.dispose();
      } catch (_) {}
      if (!_disposed) notifyListeners();
    }
  }

  /// Stops a running review after the current position.
  void cancel() {
    _cancel = true;
    _engine?.stop();
  }

  void backToSetup() {
    phase = ReviewPhase.setup;
    notifyListeners();
  }

  void goTo(int index) {
    final n = moveCount;
    cursor = index.clamp(0, n);
    notifyListeners();
  }

  void next() => goTo(cursor + 1);
  void previous() => goTo(cursor - 1);
  void first() => goTo(0);
  void last() => goTo(moveCount);

  void toggleFlip() {
    flipped = !flipped;
    notifyListeners();
  }

  void toggleBestMove() {
    showBestMove = !showBestMove;
    notifyListeners();
  }

  /// The reviewed game as PGN with the review's glyphs and evaluations in the comments.
  String exportPgn() => toPgn(game!, headers, review == null ? null : reviewAnnotations(review!));

  @override
  void dispose() {
    _disposed = true;
    _cancel = true;
    _engine?.dispose();
    super.dispose();
  }
}
