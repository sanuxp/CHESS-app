// Runs the shared test data in ../docs/testdata against the Dart rules.
import 'dart:convert';
import 'dart:io';

import 'package:chess_app/core/board.dart';
import 'package:chess_app/core/game.dart';
import 'package:chess_app/core/movegen.dart';
import 'package:chess_app/core/pgn.dart';
import 'package:chess_app/core/san.dart';
import 'package:flutter_test/flutter_test.dart';

dynamic loadJson(String name) => jsonDecode(File('../docs/testdata/$name').readAsStringSync());

Game gameFromSpec(String spec) => spec == 'startpos' ? Game() : Game.fromFen(spec)!;

void main() {
  group('basic', () {
    test('start FEN round trip', () {
      expect(Board.start().toFen(), startFen);
    });

    test('FEN round trip for other positions', () {
      for (final f in [
        'r3k2r/p1ppqpb1/bn2pnp1/3PN3/1p2P3/2N2Q1p/PPPBBPPP/R3K2R w KQkq - 0 1',
        '8/2p5/3p4/KP5r/1R3p1k/8/4P1P1/8 w - - 0 1',
        'rnbqkbnr/pppp1ppp/8/4p3/4P3/8/PPPP1PPP/RNBQKBNR w KQkq e6 0 2',
      ]) {
        expect(Board.fromFen(f)?.toFen(), f);
      }
    });

    test('bad FEN is rejected', () {
      expect(Board.fromFen(''), isNull);
      expect(Board.fromFen('8/8/8/8/8/8/8/8 w - - 0 1'), isNull);
      expect(Board.fromFen('rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBN w KQkq - 0 1'), isNull);
      expect(Board.fromFen('rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR x KQkq - 0 1'), isNull);
    });

    test('undo restores everything', () {
      final g = Game();
      expect(g.legalMoves.length, 20);
      expect(g.makeMoveUci('e2e5'), isFalse);
      expect(g.makeMoveUci('e2e4'), isTrue);
      expect(g.history.single.san, 'e4');
      expect(g.undo(), isTrue);
      expect(g.board.toFen(), startFen);
      expect(g.undo(), isFalse);

      final c = Game.fromFen('r3k2r/8/8/8/8/8/8/R3K2R w KQkq - 0 1')!;
      final before = c.board.toFen();
      expect(c.makeMoveUci('e1g1'), isTrue);
      expect(c.undo(), isTrue);
      expect(c.board.toFen(), before);
    });

    test('undo leaves a repetition draw', () {
      final g = Game();
      for (final m in ['g1f3', 'g8f6', 'f3g1', 'f6g8', 'g1f3', 'g8f6', 'f3g1', 'f6g8']) {
        g.makeMoveUci(m);
      }
      expect(g.status, GameStatus.drawRepetition);
      g.undo();
      expect(g.status, GameStatus.playing);
    });
  });

  group('perft', () {
    final data = loadJson('perft.json');
    for (final pos in data['positions']) {
      final board = Board.fromFen(pos['fen'] as String)!;
      (pos['nodes'] as Map<String, dynamic>).forEach((depth, expected) {
        test('${pos['name']} depth $depth = $expected', () {
          expect(perft(board, int.parse(depth)), expected);
        });
      });
    }
  });

  group('scenarios', () {
    final data = loadJson('scenarios.json');
    for (final sc in data['scenarios']) {
      test(sc['name'] as String, () {
        final game = gameFromSpec(sc['fen'] as String);
        for (final mv in sc['moves']) {
          expect(game.makeMoveUci(mv as String), isTrue, reason: 'move rejected: $mv');
        }
        final ex = sc['expect'] as Map<String, dynamic>;
        final legal = game.legalMoves.map((m) => m.uci).toSet();
        if (ex.containsKey('status')) expect(game.status.dataName, ex['status']);
        if (ex.containsKey('result')) expect(game.resultToken, ex['result']);
        if (ex.containsKey('fen')) expect(game.board.toFen(), ex['fen']);
        if (ex.containsKey('legalCount')) expect(legal.length, ex['legalCount']);
        for (final m in ex['legalContains'] ?? []) {
          expect(legal, contains(m));
        }
        for (final m in ex['legalNotContains'] ?? []) {
          expect(legal, isNot(contains(m)));
        }
      });
    }
  });

  group('pgn', () {
    final data = loadJson('pgn.json');
    for (final tc in data['san_cases']) {
      test(tc['name'] as String, () {
        final game = gameFromSpec(tc['fen'] as String);
        final moves = tc['moves'] as List, sans = tc['san'] as List;
        for (var i = 0; i < moves.length; i++) {
          final before = game.board.copy();
          expect(game.makeMoveUci(moves[i] as String), isTrue);
          expect(game.history.last.san, sans[i]);
          expect(moveFromSan(before, sans[i] as String), game.history.last.move, reason: 'SAN does not parse back');
        }
      });
    }
    for (final tc in data['pgn_cases']) {
      test(tc['name'] as String, () {
        final game = gameFromSpec(tc['fen'] as String);
        for (final mv in tc['moves']) {
          expect(game.makeMoveUci(mv as String), isTrue);
        }
        expect(toPgn(game), tc['pgn']);
      });
    }
  });
}
