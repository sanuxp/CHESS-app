// Themes: defaults, cycling, persistence and the settings dialog.
import 'package:chess_app/main.dart';
import 'package:chess_app/ui/app_settings.dart';
import 'package:chess_app/ui/game_controller.dart';
import 'package:flutter/material.dart';
import 'package:flutter_test/flutter_test.dart';
import 'package:shared_preferences/shared_preferences.dart';

void main() {
  test('defaults follow docs/THEMES.md and cycling wraps around', () {
    final s = AppSettings(persist: false);
    expect(s.board.name, 'Brown');
    expect(s.pieces.name, 'Engraved');
    expect(s.palette.name, 'Dark');
    expect(s.coordinates && s.hints && s.animations, isTrue);
    s.cycleBoard(-1);
    expect(s.board.name, 'Walnut');
    s.cycleBoard(1);
    expect(s.board.name, 'Brown');
    s.cyclePieces(1);
    s.cyclePalette(1);
    s.setHints(false);
    s.reset();
    expect([s.board.name, s.pieces.name, s.palette.name, s.hints], ['Brown', 'Engraved', 'Dark', true]);
  });

  test('settings are saved by name and unknown names fall back to the default', () async {
    SharedPreferences.setMockInitialValues({'board': 'Ice', 'pieces': 'Nope', 'interface': 'Sepia', 'animations': false});
    final s = AppSettings();
    await s.load();
    expect(s.board.name, 'Ice');
    expect(s.pieces.name, 'Engraved');
    expect(s.palette.name, 'Sepia');
    expect(s.animations, isFalse);

    s.cyclePieces(1);
    await Future<void>.delayed(Duration.zero);
    final prefs = await SharedPreferences.getInstance();
    expect(prefs.getString('pieces'), 'Classic');
  });

  testWidgets('Themes dialog changes the settings live', (tester) async {
    tester.view.physicalSize = const Size(400, 800);
    tester.view.devicePixelRatio = 1;
    addTearDown(tester.view.reset);
    final settings = AppSettings(persist: false);
    final c = GameController(settings: settings);
    addTearDown(c.dispose);
    await tester.pumpWidget(MaterialApp(home: GamePage(controller: c)));

    await tester.tap(find.byKey(const ValueKey('btn-themes')));
    await tester.pumpAndSettle();
    await tester.tap(find.byKey(const ValueKey('next-board')));
    await tester.pump();
    expect(settings.board.name, 'Green');
    await tester.tap(find.byKey(const ValueKey('row-interface')));
    await tester.pump();
    expect(settings.palette.name, 'Midnight');
    await tester.tap(find.byKey(const ValueKey('row-hints')));
    await tester.pump();
    expect(settings.hints, isFalse);
    expect(find.text('OFF'), findsOneWidget);

    await tester.tap(find.byKey(const ValueKey('themes-reset')));
    await tester.pump();
    expect(settings.board.name, 'Brown');
    await tester.tap(find.byKey(const ValueKey('themes-done')));
    await tester.pumpAndSettle();
    expect(find.byKey(const ValueKey('themes-done')), findsNothing);
    expect(tester.takeException(), isNull);
  });
}
