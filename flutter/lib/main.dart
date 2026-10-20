import 'package:flutter/material.dart';
import 'package:flutter/services.dart';

import 'core/pgn.dart';
import 'ui/app_settings.dart';
import 'ui/board_view.dart';
import 'ui/game_controller.dart';
import 'ui/review_page.dart';
import 'ui/save_pgn.dart';
import 'ui/side_panel.dart';

void main() => runApp(const ChessApp());

class ChessApp extends StatefulWidget {
  const ChessApp({super.key});

  @override
  State<ChessApp> createState() => _ChessAppState();
}

class _ChessAppState extends State<ChessApp> {
  final AppSettings settings = AppSettings();
  late final GameController controller = GameController(settings: settings);

  @override
  void initState() {
    super.initState();
    settings.load();
  }

  @override
  void dispose() {
    controller.dispose();
    settings.dispose();
    super.dispose();
  }

  ThemeData _theme(UiPalette p) {
    final scheme = ColorScheme.fromSeed(seedColor: p.button, brightness: p.brightness).copyWith(
      surface: p.panel,
      onSurface: p.text,
      primary: p.button,
      onPrimary: p.buttonText,
    );
    return ThemeData(
      brightness: p.brightness,
      scaffoldBackgroundColor: p.background,
      colorScheme: scheme,
      dialogTheme: DialogThemeData(backgroundColor: p.panel),
      useMaterial3: true,
    );
  }

  @override
  Widget build(BuildContext context) {
    return ListenableBuilder(
      listenable: settings,
      builder: (context, _) => MaterialApp(
        title: 'Chess',
        debugShowCheckedModeBanner: false,
        theme: _theme(settings.palette),
        home: GamePage(controller: controller),
      ),
    );
  }
}

class GamePage extends StatefulWidget {
  /// Optional so tests can drive the game; the app creates its own.
  final GameController? controller;
  const GamePage({super.key, this.controller});

  @override
  State<GamePage> createState() => _GamePageState();
}

class _GamePageState extends State<GamePage> {
  late final GameController controller = widget.controller ?? GameController();

  @override
  void dispose() {
    if (widget.controller == null) controller.dispose();
    super.dispose();
  }

  Future<void> _savePgn() async {
    final text = toPgn(controller.game, PgnHeaders(date: pgnDate()));
    controller.say(await savePgnFile(text));
  }

  Future<void> _copyFen() async {
    await Clipboard.setData(ClipboardData(text: controller.game.board.toFen()));
    controller.say('FEN COPIED TO CLIPBOARD');
  }

  void _openReview() {
    Navigator.of(context).push(MaterialPageRoute<void>(
      builder: (_) => ReviewPage(settings: controller.settings, game: controller.game),
    ));
  }

  KeyEventResult _onKey(FocusNode node, KeyEvent event) {
    if (event is! KeyDownEvent) return KeyEventResult.ignored;
    final ctrl = HardwareKeyboard.instance.isControlPressed || HardwareKeyboard.instance.isMetaPressed;
    if (event.logicalKey == LogicalKeyboardKey.escape) {
      controller.cancelPromotion();
      return KeyEventResult.handled;
    }
    if (event.logicalKey == LogicalKeyboardKey.keyU || (ctrl && event.logicalKey == LogicalKeyboardKey.keyZ)) {
      controller.undo();
      return KeyEventResult.handled;
    }
    return KeyEventResult.ignored;
  }

  @override
  Widget build(BuildContext context) {
    return Focus(
      autofocus: true,
      onKeyEvent: _onKey,
      child: ListenableBuilder(
        listenable: controller.settings,
        builder: (context, _) => Scaffold(
        backgroundColor: controller.settings.palette.background,
        body: SafeArea(
          child: LayoutBuilder(builder: (context, box) {
            final landscape = box.maxWidth >= box.maxHeight * 1.1;
            const m = 16.0;
            final board = Center(child: Padding(padding: const EdgeInsets.all(m), child: AspectRatio(aspectRatio: 1, child: BoardView(controller: controller))));
            final panel = SidePanel(controller: controller, landscape: landscape, onSavePgn: _savePgn, onCopyFen: _copyFen, onReview: _openReview);
            if (landscape) {
              final panelWidth = (box.maxWidth * 0.26).clamp(230.0, 360.0);
              return Row(children: [
                Expanded(child: board),
                Padding(padding: const EdgeInsets.fromLTRB(0, m, m, m), child: SizedBox(width: panelWidth, child: panel)),
              ]);
            }
            return Column(children: [
              Expanded(child: board),
              Padding(padding: const EdgeInsets.fromLTRB(m, 0, m, m), child: SizedBox(height: 200, child: panel)),
            ]);
          }),
        ),
        ),
      ),
    );
  }
}
