// The review screen driven end to end with a scripted engine: setup, progress, results, navigation,
// errors, cancelling and exporting.
import 'package:chess_app/core/game.dart';
import 'package:chess_app/core/pgn_import.dart';
import 'package:chess_app/main.dart';
import 'package:chess_app/review/classify.dart';
import 'package:chess_app/review/openings.dart';
import 'package:chess_app/review/uci_engine.dart';
import 'package:chess_app/ui/app_settings.dart';
import 'package:chess_app/ui/game_controller.dart';
import 'package:chess_app/ui/review_controller.dart';
import 'package:chess_app/ui/review_page.dart';
import 'package:flutter/material.dart';
import 'package:flutter/services.dart';
import 'package:flutter_test/flutter_test.dart';

import 'support/fake_transport.dart';

Game foolsMate() {
  final g = Game();
  for (final san in ['f3', 'e5', 'g4', 'Qh4#']) {
    g.makeMoveSan(san);
  }
  return g;
}

/// Engine output for fool's mate, position by position.
Future<UciTransport?> Function() scriptedEngine(Game g) {
  final fens = [for (final h in g.history) h.before.toFen()];
  return () async => FakeTransport({
        fens[0]: ['info depth 6 multipv 1 score cp 30 pv e2e4', 'info depth 6 multipv 2 score cp 28 pv d2d4'],
        fens[1]: ['info depth 6 multipv 1 score cp 120 pv e7e5', 'info depth 6 multipv 2 score cp 100 pv d7d5'],
        fens[2]: ['info depth 6 multipv 1 score cp -100 pv e2e4', 'info depth 6 multipv 2 score cp -105 pv d2d4'],
        fens[3]: ['info depth 6 multipv 1 score mate 1 pv d8h4', 'info depth 6 multipv 2 score cp 374 pv h7h5'],
      });
}

Future<OpeningBook> tinyBook() async => OpeningBook.parse('eco\tname\tpgn\nA00\tBarnes Opening\t1. f3\n');

void main() {
  late AppSettings settings;

  setUp(() => settings = AppSettings(persist: false));
  tearDown(() => settings.dispose());

  Future<ReviewController> openReview(WidgetTester tester, {Game? game, Size size = const Size(1100, 760)}) async {
    tester.view.physicalSize = size;
    tester.view.devicePixelRatio = 1;
    addTearDown(tester.view.reset);
    final g = game ?? foolsMate();
    final c = ReviewController(game: g, openTransport: scriptedEngine(foolsMate()), loadBook: tinyBook);
    addTearDown(c.dispose);
    await tester.pumpWidget(MaterialApp(home: ReviewPage(settings: settings, controller: c)));
    return c;
  }

  testWidgets('setup shows the game and starts disabled when there are no moves', (tester) async {
    final c = await openReview(tester);
    expect(find.text('Reviewing the current game: 4 moves.'), findsOneWidget);
    expect(find.byKey(const ValueKey('review-depth')), findsOneWidget);
    expect(tester.widget<FilledButton>(find.byKey(const ValueKey('review-start'))).onPressed, isNotNull);

    c.setGame(Game());
    await tester.pump();
    expect(find.textContaining('no moves yet'), findsOneWidget);
    expect(tester.widget<FilledButton>(find.byKey(const ValueKey('review-start'))).onPressed, isNull);
  });

  testWidgets('depth can be chosen', (tester) async {
    final c = await openReview(tester);
    expect(c.depth, ReviewDepth.standard);
    await tester.tap(find.text('Deep (20)'));
    await tester.pump();
    expect(c.depth, ReviewDepth.deep);
    expect(ReviewDepth.quick.depth, 12);
  });

  testWidgets('review runs and shows labels, accuracy, detail and navigation', (tester) async {
    final c = await openReview(tester);
    await tester.tap(find.byKey(const ValueKey('review-start')));
    await tester.pumpAndSettle(const Duration(milliseconds: 50));

    expect(c.phase, ReviewPhase.done);
    expect([for (final p in c.review!.plies) p.label], [MoveLabel.mistake, MoveLabel.best, MoveLabel.blunder, MoveLabel.best]);

    expect(find.byKey(const ValueKey('accuracy-white')), findsOneWidget);
    expect(find.byKey(const ValueKey('accuracy-black')), findsOneWidget);
    expect(find.byKey(const ValueKey('eval-graph')), findsOneWidget);
    expect(find.byKey(const ValueKey('move-list')), findsOneWidget);
    expect(find.text('A00 Barnes Opening'), findsOneWidget, reason: 'the opening is named even though f3 itself is rated a mistake');

    // Start of the game, then step through.
    expect(c.cursor, 0);
    await tester.tap(find.byKey(const ValueKey('nav-next')));
    await tester.pump();
    expect(c.cursor, 1);
    expect(find.byKey(const ValueKey('detail-title')), findsOneWidget);

    await tester.tap(find.byKey(const ValueKey('move-3'))); // g4
    await tester.pump();
    expect(c.cursor, 3);
    expect(find.textContaining('g4'), findsWidgets);
    expect(find.textContaining('a blunder'), findsOneWidget);
    expect(find.textContaining('Best was e4'), findsOneWidget);

    await tester.tap(find.byKey(const ValueKey('nav-last')));
    await tester.pump();
    expect(c.cursor, 4);
    expect(tester.widget<FilledButton>(find.byKey(const ValueKey('nav-next'))).onPressed, isNull);

    await tester.tap(find.byKey(const ValueKey('nav-first')));
    await tester.pump();
    expect(c.cursor, 0);
  });

  testWidgets('arrow keys step through the game and the graph can be tapped', (tester) async {
    final c = await openReview(tester);
    await tester.tap(find.byKey(const ValueKey('review-start')));
    await tester.pumpAndSettle(const Duration(milliseconds: 50));

    await tester.sendKeyEvent(LogicalKeyboardKey.arrowRight);
    await tester.sendKeyEvent(LogicalKeyboardKey.arrowRight);
    await tester.pump();
    expect(c.cursor, 2);
    await tester.sendKeyEvent(LogicalKeyboardKey.arrowLeft);
    await tester.pump();
    expect(c.cursor, 1);
    await tester.sendKeyEvent(LogicalKeyboardKey.end);
    await tester.pump();
    expect(c.cursor, 4);

    final graph = tester.getRect(find.byKey(const ValueKey('eval-graph')));
    await tester.tapAt(Offset(graph.left + graph.width * 0.5, graph.center.dy)); // middle = after move 2
    await tester.pump();
    expect(c.cursor, 2);
  });

  testWidgets('board can be flipped and the best-move arrow toggled', (tester) async {
    final c = await openReview(tester);
    await tester.tap(find.byKey(const ValueKey('review-start')));
    await tester.pumpAndSettle(const Duration(milliseconds: 50));
    await tester.tap(find.byKey(const ValueKey('review-flip')));
    await tester.tap(find.byKey(const ValueKey('review-arrow')));
    await tester.pump();
    expect(c.flipped, isTrue);
    expect(c.showBestMove, isFalse);
    expect(tester.takeException(), isNull);
  });

  testWidgets('a missing engine explains how to install Stockfish', (tester) async {
    tester.view.physicalSize = const Size(1100, 760);
    tester.view.devicePixelRatio = 1;
    addTearDown(tester.view.reset);
    final c = ReviewController(game: foolsMate(), openTransport: () async => null, loadBook: tinyBook);
    addTearDown(c.dispose);
    await tester.pumpWidget(MaterialApp(home: ReviewPage(settings: settings, controller: c)));
    await tester.tap(find.byKey(const ValueKey('review-start')));
    await tester.pumpAndSettle();
    expect(c.phase, ReviewPhase.error);
    expect(find.byKey(const ValueKey('review-error')), findsOneWidget);
    expect(find.textContaining('Stockfish'), findsWidgets);
    expect(find.text('Try again'), findsOneWidget);
  });

  testWidgets('an engine that stops answering ends in a clear error', (tester) async {
    tester.view.physicalSize = const Size(1100, 760);
    tester.view.devicePixelRatio = 1;
    addTearDown(tester.view.reset);
    final c = ReviewController(game: foolsMate(), openTransport: () async => SilentTransport(), loadBook: tinyBook);
    addTearDown(c.dispose);
    await tester.pumpWidget(MaterialApp(home: ReviewPage(settings: settings, controller: c)));
    await tester.tap(find.byKey(const ValueKey('review-start')));
    await tester.pump();
    await tester.pump(const Duration(seconds: 25)); // the handshake gives up after 20 s
    await tester.pumpAndSettle();
    expect(c.phase, ReviewPhase.error);
    expect(c.error, contains('did not answer'));
  });

  testWidgets('a review can be cancelled', (tester) async {
    final c = await openReview(tester);
    await tester.runAsync(() async {
      final running = c.start();
      c.cancel();
      await running;
    });
    expect(c.phase, ReviewPhase.setup);
    expect(c.review, isNull);
  });

  testWidgets('reviewed PGN carries glyphs and evaluations and can be read back', (tester) async {
    final c = await openReview(tester);
    await tester.tap(find.byKey(const ValueKey('review-start')));
    await tester.pumpAndSettle(const Duration(milliseconds: 50));
    final pgn = c.exportPgn();
    expect(pgn, contains(r'$2')); // f3 mistake
    expect(pgn, contains(r'$4')); // g4 blunder
    expect(pgn, contains('[%eval'));
    expect(pgn, contains('Blunder. Best: e4'));
    final back = importPgn(pgn);
    expect(back.error, isNull);
    expect([for (final h in back.game.history) h.san], ['f3', 'e5', 'g4', 'Qh4#']);
  });

  testWidgets('opening PGN text sets the game to review', (tester) async {
    final c = await openReview(tester);
    expect(c.loadPgn('[White "Alice"]\n[Black "Bob"]\n\n1. e4 e5 2. Nf3 *', name: 'x.pgn'), isTrue);
    await tester.pump();
    expect(c.moveCount, 3);
    expect(c.headers.white, 'Alice');
    expect(find.text('Reviewing x.pgn: 3 moves.'), findsOneWidget);

    expect(c.loadPgn('1. e4 e5 2. Ke4', name: 'bad.pgn'), isTrue);
    expect(c.importWarning, contains('Reading stopped early'));
    expect(c.loadPgn('no moves here', name: 'empty.pgn'), isFalse);
    expect(c.phase, ReviewPhase.error);
  });

  testWidgets('phone-sized portrait layout has no overflow', (tester) async {
    final c = await openReview(tester, size: const Size(390, 844));
    await tester.tap(find.byKey(const ValueKey('review-start')));
    await tester.pumpAndSettle(const Duration(milliseconds: 50));
    expect(c.phase, ReviewPhase.done);
    expect(tester.takeException(), isNull);
    await tester.drag(find.byType(ListView).first, const Offset(0, -300));
    await tester.pump();
    expect(tester.takeException(), isNull);
  });

  testWidgets('the REVIEW button on the main screen opens the review of the current game', (tester) async {
    tester.view.physicalSize = const Size(1100, 760);
    tester.view.devicePixelRatio = 1;
    addTearDown(tester.view.reset);
    final game = GameController(settings: settings);
    addTearDown(game.dispose);
    game.game.makeMoveSan('e4');
    game.game.makeMoveSan('e5');
    await tester.pumpWidget(MaterialApp(home: GamePage(controller: game)));
    await tester.tap(find.byKey(const ValueKey('btn-review')));
    await tester.pumpAndSettle();
    expect(find.text('Game review'), findsOneWidget);
    expect(find.text('Reviewing the current game: 2 moves.'), findsOneWidget);
    expect(find.byKey(const ValueKey('review-start')), findsOneWidget);
  });
}
