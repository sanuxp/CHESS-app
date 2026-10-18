// Drives the real widgets with taps and drags (the Flutter counterpart of the C++ --selftest).
import 'package:chess_app/core/board.dart';
import 'package:chess_app/core/game.dart';
import 'package:chess_app/core/types.dart';
import 'package:chess_app/main.dart';
import 'package:chess_app/ui/board_view.dart';
import 'package:chess_app/ui/game_controller.dart';
import 'package:flutter/material.dart';
import 'package:flutter_test/flutter_test.dart';

void main() {
  late GameController c;

  Future<void> start(WidgetTester tester, {Size size = const Size(1000, 720)}) async {
    tester.view.physicalSize = size;
    tester.view.devicePixelRatio = 1;
    addTearDown(tester.view.reset);
    c = GameController();
    addTearDown(c.dispose);
    await tester.pumpWidget(MaterialApp(home: GamePage(controller: c)));
  }

  Offset center(WidgetTester tester, String name) {
    final rect = tester.getRect(find.byType(BoardView));
    final cell = rect.width / 8;
    final sq = squareFromName(name);
    final col = c.flipped ? 7 - fileOf(sq) : fileOf(sq);
    final row = c.flipped ? rankOf(sq) : 7 - rankOf(sq);
    return Offset(rect.left + (col + 0.5) * cell, rect.top + (row + 0.5) * cell);
  }

  Future<void> tapSquare(WidgetTester tester, String name) async {
    await tester.tapAt(center(tester, name));
    await tester.pump();
  }

  Future<void> settle(WidgetTester tester) => tester.pumpAndSettle(const Duration(milliseconds: 50));

  testWidgets('click-click move animates and input is locked meanwhile', (tester) async {
    await start(tester);
    await tapSquare(tester, 'e2');
    expect(c.selected, squareFromName('e2'));
    expect(c.targets.length, 2);
    await tapSquare(tester, 'e4');
    expect(c.game.moveCount, 1);
    expect(c.busy, isTrue);
    await tester.pump(const Duration(milliseconds: 100));
    expect(c.busy, isTrue, reason: 'still mid-animation');
    await tapSquare(tester, 'e7');
    expect(c.selected, noSquare, reason: 'input ignored while animating');
    await settle(tester);
    expect(c.busy, isFalse);
  });

  testWidgets('illegal destination does not move; drag and drop plays without animation', (tester) async {
    await start(tester);
    await tapSquare(tester, 'e2');
    await tapSquare(tester, 'e4');
    await settle(tester);

    await tapSquare(tester, 'e7');
    await tapSquare(tester, 'e4'); // white pawn, not a target
    expect(c.game.moveCount, 1);

    final gesture = await tester.startGesture(center(tester, 'e7'));
    await gesture.moveTo(center(tester, 'e7') + const Offset(0, 30));
    await tester.pump();
    expect(c.dragging, isTrue);
    await gesture.moveTo(center(tester, 'e5'));
    await gesture.up();
    await tester.pump();
    expect(c.game.moveCount, 2);
    expect(c.busy, isFalse, reason: 'dropped piece is already in place');
  });

  testWidgets('promotion picker, undo, castling, new game, flip', (tester) async {
    await start(tester);
    c.game = Game.fromFen('8/P6k/8/8/8/8/8/K7 w - - 0 1')!;
    c.notifyListeners();
    await tester.pump();
    await tapSquare(tester, 'a7');
    await tapSquare(tester, 'a8');
    expect(c.promotionOpen, isTrue);
    await tester.pump();
    await tester.tap(find.byKey(const ValueKey('promo-bishop')));
    await tester.pump();
    expect(c.promotionOpen, isFalse);
    expect(c.game.history.single.san, 'a8=B');
    await settle(tester);

    await tester.tap(find.byKey(const ValueKey('btn-undo')));
    await tester.pump();
    expect(c.game.moveCount, 0);
    expect(c.busy, isTrue);
    await settle(tester);

    c.game = Game.fromFen('r3k2r/8/8/8/8/8/8/R3K2R w KQkq - 0 1')!;
    c.notifyListeners();
    await tester.pump();
    await tapSquare(tester, 'e1');
    await tapSquare(tester, 'g1');
    expect(c.anim?.rook, isNotNull);
    expect(c.anim?.rookTo, squareFromName('f1'));
    await settle(tester);

    await tester.tap(find.byKey(const ValueKey('btn-new')));
    await tester.pump();
    expect(c.game.board.toFen(), startFen);

    await tester.tap(find.byKey(const ValueKey('btn-flip')));
    await tester.pump();
    await tapSquare(tester, 'e2');
    await tapSquare(tester, 'e4');
    await settle(tester);
    expect(c.flipped, isTrue);
    await tester.pump(const Duration(seconds: 5)); // let the toast timer finish
  });

  testWidgets('checkmate shows the result and phone-sized portrait layout works', (tester) async {
    await start(tester, size: const Size(400, 800));
    for (final m in ['f2f3', 'e7e5', 'g2g4', 'd8h4']) {
      c.game.makeMoveUci(m);
    }
    c.notifyListeners();
    await tester.pump();
    expect(find.byKey(const ValueKey('detail')), findsOneWidget);
    expect(find.text('CHECKMATE - BLACK WINS'), findsWidgets);
    expect(tester.takeException(), isNull);
  });
}
