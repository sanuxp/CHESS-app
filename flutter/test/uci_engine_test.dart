// UCI parsing and protocol handling against recorded Stockfish 19 output and a scripted fake engine, plus an
// optional smoke test against the real Stockfish (skipped unless engine/stockfish.exe exists).
import 'dart:convert';
import 'dart:io';

import 'package:chess_app/core/game.dart';
import 'package:chess_app/review/analyzer.dart';
import 'package:chess_app/review/classify.dart';
import 'package:chess_app/review/engine_factory.dart';
import 'package:chess_app/review/openings.dart';
import 'package:chess_app/review/uci_engine.dart';
import 'package:chess_app/review/uci_parser.dart';
import 'package:flutter_test/flutter_test.dart';

import 'support/fake_transport.dart';

void main() {
  group('uci parser (recorded Stockfish 19 lines)', () {
    test('mate score', () {
      final i = parseInfo(
          'info depth 10 seldepth 2 multipv 1 score mate 1 nodes 4064 nps 145142 hashfull 1 tbhits 0 time 28 pv d8h4')!;
      expect(i.depth, 10);
      expect(i.multipv, 1);
      expect(i.score.mate, 1);
      expect(i.pv, ['d8h4']);
    });

    test('centipawn score with a long variation', () {
      final i = parseInfo(
          'info depth 10 seldepth 15 multipv 2 score cp 374 nodes 4064 nps 145142 hashfull 1 tbhits 0 time 28 pv h7h5 g4g5 d8g5 g1h3')!;
      expect(i.multipv, 2);
      expect(i.score.cp, 374);
      expect(i.pv, ['h7h5', 'g4g5', 'd8g5', 'g1h3']);
    });

    test('negative scores and missing multipv', () {
      final i = parseInfo('info depth 5 score cp -35 nodes 10 pv e7e5 g1f3')!;
      expect(i.multipv, 1);
      expect(i.score.cp, -35);
    });

    test('ignores lines that are not results', () {
      expect(parseInfo('info depth 1 currmove e2e4 currmovenumber 1'), isNull);
      expect(parseInfo('info string NNUE evaluation using nn-1234.nnue'), isNull);
      expect(parseInfo('info depth 8 multipv 1 score cp 12 lowerbound nodes 5 pv e2e4'), isNull);
      expect(parseInfo('readyok'), isNull);
    });

    test('bestmove', () {
      expect(parseBestMove('bestmove d8h4 ponder a2a3'), 'd8h4');
      expect(parseBestMove('bestmove (none)'), '(none)');
      expect(parseBestMove('info depth 1'), isNull);
    });
  });

  group('engine protocol with a scripted engine', () {
    test('handshake, options and multi-line analysis', () async {
      const fen = 'rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1';
      final t = FakeTransport({
        fen: [
          'info depth 5 multipv 1 score cp 20 pv e2e4 e7e5',
          'info depth 5 multipv 2 score cp 15 pv d2d4 d7d5',
          'info depth 6 multipv 1 score cp 30 pv e2e4 c7c5',
          'info depth 6 multipv 2 score cp 18 pv d2d4 g8f6',
        ],
      });
      final engine = UciEngine(t);
      await engine.start();
      expect(engine.name, 'FakeFish 1');
      final lines = await engine.analyze(fen, depth: 6, multiPv: 2);
      expect(lines.length, 2);
      expect(lines[0].move, 'e2e4');
      expect(lines[0].score.cp, 30, reason: 'the deepest result per line wins');
      expect(lines[1].move, 'd2d4');
      expect(t.sent, contains('setoption name MultiPV value 2'));
      expect(t.sent, contains('position fen $fen'));
      expect(t.sent, contains('go depth 6'));
      await engine.dispose();
    });

    test('a finished game returns no lines', () async {
      final t = FakeTransport({});
      final engine = UciEngine(t);
      await engine.start();
      expect(await engine.analyze('7k/5Q2/6K1/8/8/8/8/8 b - - 0 1'), isEmpty);
      await engine.dispose();
    });

    test('an engine that never answers times out with a clear message', () async {
      final engine = UciEngine(SilentTransport());
      await expectLater(engine.start(timeout: const Duration(milliseconds: 100)), throwsA(isA<EngineException>()));
    });
  });

  group('analyzer with a scripted engine', () {
    test("reviews fool's mate and caches positions", () async {
      final g = Game();
      for (final san in ['f3', 'e5', 'g4', 'Qh4#']) {
        g.makeMoveSan(san);
      }
      final fens = [for (final h in g.history) h.before.toFen()];
      final t = FakeTransport({
        fens[0]: ['info depth 6 multipv 1 score cp 30 pv e2e4', 'info depth 6 multipv 2 score cp 28 pv d2d4'],
        fens[1]: ['info depth 6 multipv 1 score cp 120 pv e7e5', 'info depth 6 multipv 2 score cp 100 pv d7d5'],
        fens[2]: ['info depth 6 multipv 1 score cp -100 pv e2e4', 'info depth 6 multipv 2 score cp -105 pv d2d4'],
        fens[3]: ['info depth 6 multipv 1 score mate 1 pv d8h4', 'info depth 6 multipv 2 score cp 374 pv h7h5'],
      });
      final engine = UciEngine(t);
      await engine.start();
      final analyzer = EngineAnalyzer(engine, depth: 6, multiPv: 2);
      final progress = <int>[];
      final review = await analyzer.review(g, onProgress: (d, n) => progress.add(d));
      expect([for (final p in review.plies) p.label], [MoveLabel.mistake, MoveLabel.best, MoveLabel.blunder, MoveLabel.best]);
      expect(progress, [1, 2, 3, 4, 5]);
      final goCount = t.sent.where((s) => s.startsWith('go ')).length;
      expect(goCount, 4, reason: 'the finished position is not searched');
      await analyzer.review(g); // second run comes from the cache
      expect(t.sent.where((s) => s.startsWith('go ')).length, 4);
      await engine.dispose();
    });

    test('can be cancelled between positions', () async {
      final g = Game()..makeMoveSan('e4');
      final engine = UciEngine(FakeTransport({}));
      await engine.start();
      await expectLater(EngineAnalyzer(engine).review(g, isCancelled: () => true), throwsA(isA<ReviewCancelled>()));
      await engine.dispose();
    });
  });

  group('real Stockfish (skipped unless engine/stockfish.exe is installed)', () {
    Future<String?> path() => findEnginePath();

    Future<UciEngine> open() async {
      final transport = await openEngineTransport();
      final engine = UciEngine(transport!);
      await engine.start();
      return engine;
    }

    test('finds mate in one', () async {
      if (await path() == null) return markTestSkipped('Stockfish not installed (tools/get_stockfish.ps1)');
      final engine = await open();
      final lines = await engine.analyze('6k1/5ppp/8/8/8/8/5PPP/R5K1 w - - 0 1', depth: 8, multiPv: 1);
      expect(lines.first.move, 'a1a8');
      expect(lines.first.score.mate, 1);
      expect(engine.name, startsWith('Stockfish'));
      await engine.dispose();
    });

    final games = (jsonDecode(File('../docs/testdata/review.json').readAsStringSync()) as Map)['engineGames'] as List;
    final book = OpeningBook.parse(File('../docs/data/openings.tsv').readAsStringSync());
    for (final g in games) {
      test('reviews ${g['name']}', () async {
        if (await path() == null) return markTestSkipped('Stockfish not installed (tools/get_stockfish.ps1)');
        final engine = await open();
        final game = Game();
        for (final san in g['san'] as List) {
          expect(game.makeMoveSan(san as String), isTrue, reason: san);
        }
        final analyzer = EngineAnalyzer(engine); // default depth 16
        analyzer.configure(threads: defaultEngineThreads());
        final review = await analyzer.review(game, book: book);
        for (final e in g['expect'] as List) {
          final ply = review.plies[(e['ply'] as int) - 1];
          expect(e['oneOf'], contains(ply.label.name), reason: 'ply ${ply.ply} ${ply.san} was ${ply.label.name} (loss ${ply.loss.toStringAsFixed(1)})');
        }
        await engine.dispose();
      }, timeout: const Timeout(Duration(minutes: 5)));
    }
  });
}
