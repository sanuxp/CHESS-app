import 'package:file_selector/file_selector.dart';
import 'package:flutter/material.dart';
import 'package:flutter/services.dart';

import '../core/game.dart';
import '../core/types.dart';
import '../review/classify.dart';
import '../review/review.dart';
import 'app_settings.dart';
import 'label_style.dart';
import 'review_board.dart';
import 'review_controller.dart';
import 'save_pgn.dart';

/// Full-screen game review: choose depth, run Stockfish, then step through the game with labels, accuracy,
/// an evaluation graph and the engine's preferred moves.
class ReviewPage extends StatefulWidget {
  final AppSettings settings;
  final Game? game; // the game to review; ignored when [controller] is given
  final ReviewController? controller; // tests inject their own

  const ReviewPage({super.key, required this.settings, this.game, this.controller});

  @override
  State<ReviewPage> createState() => _ReviewPageState();
}

class _ReviewPageState extends State<ReviewPage> {
  late final ReviewController c = widget.controller ?? ReviewController(game: widget.game);
  final FocusNode _focus = FocusNode();
  String toast = '';

  AppSettings get settings => widget.settings;

  @override
  void dispose() {
    _focus.dispose();
    if (widget.controller == null) c.dispose();
    super.dispose();
  }

  void _say(String text) => setState(() => toast = text);

  Future<void> _openPgn() async {
    try {
      final file = await openFile(acceptedTypeGroups: const [XTypeGroup(label: 'PGN game', extensions: ['pgn', 'txt'])]);
      if (file == null) return;
      c.loadPgn(await file.readAsString(), name: file.name);
    } catch (e) {
      _say('Could not open the file: $e');
    }
  }

  Future<void> _export() async => _say(await savePgnFile(c.exportPgn()));

  KeyEventResult _onKey(FocusNode node, KeyEvent e) {
    if (e is! KeyDownEvent || c.phase != ReviewPhase.done) return KeyEventResult.ignored;
    final k = e.logicalKey;
    if (k == LogicalKeyboardKey.arrowRight) {
      c.next();
    } else if (k == LogicalKeyboardKey.arrowLeft) {
      c.previous();
    } else if (k == LogicalKeyboardKey.home) {
      c.first();
    } else if (k == LogicalKeyboardKey.end) {
      c.last();
    } else {
      return KeyEventResult.ignored;
    }
    return KeyEventResult.handled;
  }

  @override
  Widget build(BuildContext context) {
    return Focus(
      focusNode: _focus,
      autofocus: true,
      onKeyEvent: _onKey,
      child: ListenableBuilder(
        listenable: Listenable.merge([c, settings]),
        builder: (context, _) {
          final pal = settings.palette;
          return Scaffold(
            backgroundColor: pal.background,
            appBar: AppBar(
              backgroundColor: pal.panel,
              foregroundColor: pal.text,
              title: const Text('Game review'),
              actions: [
                if (c.phase == ReviewPhase.done) ...[
                  IconButton(
                    key: const ValueKey('review-flip'),
                    tooltip: 'Flip board',
                    icon: const Icon(Icons.swap_vert),
                    onPressed: c.toggleFlip,
                  ),
                  IconButton(
                    key: const ValueKey('review-arrow'),
                    tooltip: c.showBestMove ? 'Hide best-move arrow' : 'Show best-move arrow',
                    icon: Icon(c.showBestMove ? Icons.arrow_outward : Icons.arrow_outward_outlined),
                    onPressed: c.toggleBestMove,
                  ),
                  IconButton(
                    key: const ValueKey('review-export'),
                    tooltip: 'Save reviewed PGN',
                    icon: const Icon(Icons.save_alt),
                    onPressed: _export,
                  ),
                ],
              ],
            ),
            body: SafeArea(
              child: switch (c.phase) {
                ReviewPhase.done => _Results(controller: c, settings: settings, toast: toast),
                _ => _Setup(controller: c, settings: settings, onOpen: _openPgn, toast: toast),
              },
            ),
          );
        },
      ),
    );
  }
}

// ---------------------------------------------------------------- setup / progress / error

class _Setup extends StatelessWidget {
  final ReviewController controller;
  final AppSettings settings;
  final VoidCallback onOpen;
  final String toast;
  const _Setup({required this.controller, required this.settings, required this.onOpen, required this.toast});

  @override
  Widget build(BuildContext context) {
    final c = controller;
    final pal = settings.palette;
    final g = c.game;
    final running = c.phase == ReviewPhase.running;
    final TextStyle body = TextStyle(color: pal.text, fontSize: 15);
    final TextStyle muted = TextStyle(color: pal.muted, fontSize: 13);

    return Center(
      child: SingleChildScrollView(
        padding: const EdgeInsets.all(20),
        child: ConstrainedBox(
          constraints: const BoxConstraints(maxWidth: 460),
          child: Container(
            padding: const EdgeInsets.all(20),
            color: pal.panel,
            child: Column(mainAxisSize: MainAxisSize.min, crossAxisAlignment: CrossAxisAlignment.stretch, children: [
              Text('Review with Stockfish', style: TextStyle(color: pal.text, fontSize: 22, fontWeight: FontWeight.w800)),
              const SizedBox(height: 8),
              Text(
                g == null || g.moveCount == 0
                    ? 'There are no moves yet. Play a game, or open a PGN file.'
                    : 'Reviewing ${c.source}: ${g.moveCount} moves.',
                key: const ValueKey('review-source'),
                style: body,
              ),
              if (c.importWarning != null) Padding(padding: const EdgeInsets.only(top: 6), child: Text(c.importWarning!, style: muted)),
              const SizedBox(height: 16),
              if (running) ...[
                LinearProgressIndicator(value: c.total == 0 ? null : c.done / c.total, key: const ValueKey('review-progress')),
                const SizedBox(height: 10),
                Text('Analysing position ${c.done} of ${c.total}…', style: body),
                const SizedBox(height: 4),
                Text('Depth ${c.depth.depth}. This can take a minute or two for a long game.', style: muted),
                const SizedBox(height: 16),
                OutlinedButton(key: const ValueKey('review-cancel'), onPressed: c.cancel, child: const Text('Cancel')),
              ] else ...[
                if (c.phase == ReviewPhase.error)
                  Container(
                    key: const ValueKey('review-error'),
                    padding: const EdgeInsets.all(10),
                    margin: const EdgeInsets.only(bottom: 14),
                    color: const Color(0x33CA3431),
                    child: Text(c.error ?? 'Something went wrong.', style: body),
                  ),
                Text('How deep should Stockfish look?', style: muted),
                const SizedBox(height: 6),
                SegmentedButton<ReviewDepth>(
                  key: const ValueKey('review-depth'),
                  segments: [
                    for (final d in ReviewDepth.values) ButtonSegment(value: d, label: Text('${d.title} (${d.depth})')),
                  ],
                  selected: {c.depth},
                  showSelectedIcon: false,
                  onSelectionChanged: (s) => c.setDepth(s.first),
                ),
                const SizedBox(height: 6),
                Text('Deeper is slower but finds more tactics. Standard takes about a minute for 30 moves.', style: muted),
                const SizedBox(height: 18),
                FilledButton(
                  key: const ValueKey('review-start'),
                  onPressed: g == null || g.moveCount == 0 ? null : c.start,
                  child: Text(c.phase == ReviewPhase.error ? 'Try again' : 'Start review'),
                ),
                const SizedBox(height: 8),
                OutlinedButton(key: const ValueKey('review-open'), onPressed: onOpen, child: const Text('Open a PGN file…')),
              ],
              if (toast.isNotEmpty) Padding(padding: const EdgeInsets.only(top: 12), child: Text(toast, style: muted)),
            ]),
          ),
        ),
      ),
    );
  }
}

// ---------------------------------------------------------------- results

String formatScore(EngineScore? s) {
  if (s == null) return '';
  if (s.isMate) return s.mate! > 0 ? 'M${s.mate}' : '-M${-s.mate!}';
  final v = s.cp! / 100;
  return '${v >= 0 ? '+' : ''}${v.toStringAsFixed(2)}';
}

class _Results extends StatelessWidget {
  final ReviewController controller;
  final AppSettings settings;
  final String toast;
  const _Results({required this.controller, required this.settings, required this.toast});

  @override
  Widget build(BuildContext context) {
    final c = controller;
    return LayoutBuilder(builder: (context, box) {
      final landscape = box.maxWidth >= box.maxHeight * 1.1;
      const m = 14.0;
      final board = Center(
        child: Padding(
          padding: const EdgeInsets.all(m),
          child: AspectRatio(aspectRatio: 1, child: ReviewBoard(controller: c, settings: settings)),
        ),
      );
      if (landscape) {
        final w = (box.maxWidth * 0.34).clamp(320.0, 460.0);
        return Row(children: [
          Expanded(child: board),
          Padding(
            padding: const EdgeInsets.fromLTRB(0, m, m, m),
            child: SizedBox(width: w, child: _SidePanel(controller: c, settings: settings, expandList: true, toast: toast)),
          ),
        ]);
      }
      return ListView(padding: const EdgeInsets.only(bottom: 16), children: [
        SizedBox(height: box.maxWidth, child: board),
        Padding(
          padding: const EdgeInsets.symmetric(horizontal: m),
          child: _SidePanel(controller: c, settings: settings, expandList: false, toast: toast),
        ),
      ]);
    });
  }
}

class _SidePanel extends StatelessWidget {
  final ReviewController controller;
  final AppSettings settings;
  final bool expandList;
  final String toast;
  const _SidePanel({required this.controller, required this.settings, required this.expandList, required this.toast});

  @override
  Widget build(BuildContext context) {
    final c = controller;
    final pal = settings.palette;
    final review = c.review!;

    final list = _MoveList(controller: c, settings: settings);
    return Container(
      color: pal.panel,
      padding: const EdgeInsets.all(12),
      child: Column(
        mainAxisSize: expandList ? MainAxisSize.max : MainAxisSize.min,
        crossAxisAlignment: CrossAxisAlignment.stretch,
        children: [
          _Accuracy(review: review, settings: settings, white: c.game!.initial.sideToMove == PieceColor.white),
          if (review.opening != null)
            Padding(
              padding: const EdgeInsets.only(top: 6),
              child: Text('${review.openingEco ?? ''} ${review.opening}', key: const ValueKey('review-opening'), style: TextStyle(color: pal.muted, fontSize: 12)),
            ),
          const SizedBox(height: 10),
          SizedBox(height: 84, child: _EvalGraph(controller: c, settings: settings)),
          const SizedBox(height: 10),
          _Detail(controller: c, settings: settings),
          const SizedBox(height: 8),
          _Navigation(controller: c, settings: settings),
          const SizedBox(height: 8),
          if (expandList) Expanded(child: list) else SizedBox(height: 220, child: list),
          if (toast.isNotEmpty) Padding(padding: const EdgeInsets.only(top: 8), child: Text(toast, style: TextStyle(color: pal.muted, fontSize: 12))),
        ],
      ),
    );
  }
}

class _Accuracy extends StatelessWidget {
  final GameReview review;
  final AppSettings settings;
  final bool white;
  const _Accuracy({required this.review, required this.settings, required this.white});

  @override
  Widget build(BuildContext context) {
    final pal = settings.palette;
    Widget card(String who, double acc, Color disc, Key key) => Expanded(
          child: Container(
            key: key,
            padding: const EdgeInsets.all(8),
            color: pal.background.withValues(alpha: 0.5),
            child: Row(children: [
              Container(
                width: 14,
                height: 14,
                decoration: BoxDecoration(shape: BoxShape.circle, color: disc, border: Border.all(color: pal.muted, width: 1.5)),
              ),
              const SizedBox(width: 8),
              Expanded(child: Text(who, style: TextStyle(color: pal.muted, fontSize: 12))),
              Text('${acc.toStringAsFixed(1)}%', style: TextStyle(color: pal.text, fontSize: 18, fontWeight: FontWeight.w800)),
            ]),
          ),
        );
    return Row(children: [
      card('White accuracy', review.accuracyWhite, const Color(0xFFF8F6EE), const ValueKey('accuracy-white')),
      const SizedBox(width: 8),
      card('Black accuracy', review.accuracyBlack, const Color(0xFF141418), const ValueKey('accuracy-black')),
    ]);
  }
}

class _Detail extends StatelessWidget {
  final ReviewController controller;
  final AppSettings settings;
  const _Detail({required this.controller, required this.settings});

  @override
  Widget build(BuildContext context) {
    final c = controller;
    final pal = settings.palette;
    final review = c.review!;
    final style = TextStyle(color: pal.text, fontSize: 14);
    final muted = TextStyle(color: pal.muted, fontSize: 12);

    if (c.cursor == 0) {
      return Container(
        key: const ValueKey('review-detail'),
        constraints: const BoxConstraints(minHeight: 66),
        alignment: Alignment.centerLeft,
        child: Text('Starting position. Use the arrows or tap a move.', style: muted),
      );
    }
    final ply = review.plies[c.cursor - 1];
    final ls = labelStyle(ply.label);
    return Container(
      key: const ValueKey('review-detail'),
      constraints: const BoxConstraints(minHeight: 66),
      padding: const EdgeInsets.all(8),
      color: pal.background.withValues(alpha: 0.5),
      child: Row(children: [
        _Badge(ply.label, 26),
        const SizedBox(width: 10),
        Expanded(
          child: Column(crossAxisAlignment: CrossAxisAlignment.start, mainAxisAlignment: MainAxisAlignment.center, children: [
            Text.rich(TextSpan(children: [
              TextSpan(text: '${ply.san} ', style: style.copyWith(fontWeight: FontWeight.w800)),
              TextSpan(text: 'is ${labelMeaning(ply.label)}', style: style.copyWith(color: ls.color, fontWeight: FontWeight.w600)),
            ]), key: const ValueKey('detail-title')),
            const SizedBox(height: 2),
            Text(
              [
                if (ply.hasBetterMove) 'Best was ${ply.bestSan}',
                if (ply.scoreAfter != null) 'Eval ${formatScore(ply.scoreAfter)}',
                'Accuracy ${ply.accuracy.toStringAsFixed(0)}%',
              ].join('   ·   '),
              key: const ValueKey('detail-sub'),
              style: muted,
            ),
          ]),
        ),
      ]),
    );
  }
}

class _Badge extends StatelessWidget {
  final MoveLabel label;
  final double size;
  const _Badge(this.label, this.size);

  @override
  Widget build(BuildContext context) {
    final s = labelStyle(label);
    return Container(
      width: size,
      height: size,
      alignment: Alignment.center,
      decoration: BoxDecoration(shape: BoxShape.circle, color: s.color),
      child: s.icon != null
          ? Icon(s.icon, size: size * 0.62, color: Colors.white)
          : Text(s.glyph, style: TextStyle(color: Colors.white, fontSize: size * (s.glyph.length > 1 ? 0.45 : 0.6), fontWeight: FontWeight.w800)),
    );
  }
}

class _Navigation extends StatelessWidget {
  final ReviewController controller;
  final AppSettings settings;
  const _Navigation({required this.controller, required this.settings});

  @override
  Widget build(BuildContext context) {
    final c = controller;
    final pal = settings.palette;
    Widget btn(IconData icon, String key, VoidCallback? onTap, String tip) => Expanded(
          child: Tooltip(
            message: tip,
            child: SizedBox(
              height: 40,
              child: FilledButton(
                key: ValueKey(key),
                onPressed: onTap,
                style: FilledButton.styleFrom(
                  backgroundColor: pal.button,
                  foregroundColor: pal.buttonText,
                  disabledBackgroundColor: pal.buttonDisabled,
                  shape: const RoundedRectangleBorder(),
                ),
                child: Icon(icon),
              ),
            ),
          ),
        );
    final n = c.moveCount;
    return Row(children: [
      btn(Icons.first_page, 'nav-first', c.cursor > 0 ? c.first : null, 'Start'),
      const SizedBox(width: 6),
      btn(Icons.chevron_left, 'nav-prev', c.cursor > 0 ? c.previous : null, 'Previous move'),
      const SizedBox(width: 6),
      btn(Icons.chevron_right, 'nav-next', c.cursor < n ? c.next : null, 'Next move'),
      const SizedBox(width: 6),
      btn(Icons.last_page, 'nav-last', c.cursor < n ? c.last : null, 'End'),
    ]);
  }
}

class _MoveList extends StatefulWidget {
  final ReviewController controller;
  final AppSettings settings;
  const _MoveList({required this.controller, required this.settings});

  @override
  State<_MoveList> createState() => _MoveListState();
}

class _MoveListState extends State<_MoveList> {
  final Map<int, GlobalKey> _keys = {};
  int _shown = -1;

  @override
  Widget build(BuildContext context) {
    final c = widget.controller;
    final pal = widget.settings.palette;
    final review = c.review!;
    final firstIsWhite = c.game!.initial.sideToMove == PieceColor.white;

    if (c.cursor != _shown) {
      _shown = c.cursor;
      WidgetsBinding.instance.addPostFrameCallback((_) {
        final ctx = _keys[c.cursor]?.currentContext;
        if (ctx != null && ctx.mounted) Scrollable.ensureVisible(ctx, alignment: 0.5, duration: const Duration(milliseconds: 120));
      });
    }

    Widget cell(int ply) {
      final p = review.plies[ply - 1];
      final selected = c.cursor == ply;
      final key = _keys.putIfAbsent(ply, GlobalKey.new);
      return Expanded(
        child: InkWell(
          key: ValueKey('move-$ply'),
          onTap: () => c.goTo(ply),
          child: Container(
            key: key,
            height: 32,
            padding: const EdgeInsets.symmetric(horizontal: 8),
            color: selected ? pal.button : null,
            child: Row(children: [
              Expanded(child: Text(p.san, overflow: TextOverflow.ellipsis, style: TextStyle(color: selected ? pal.buttonText : pal.text, fontWeight: FontWeight.w600))),
              _Badge(p.label, 18),
            ]),
          ),
        ),
      );
    }

    // Rows pair White's and Black's moves; a game that starts with Black's move leaves the first White cell empty.
    final rows = <Widget>[];
    var ply = 1;
    var number = c.game!.initial.fullmoveNumber;
    if (!firstIsWhite) {
      rows.add(_row(pal, number, null, cell(ply)));
      ply++;
      number++;
    }
    while (ply <= review.plies.length) {
      final white = cell(ply);
      final black = ply + 1 <= review.plies.length ? cell(ply + 1) : const Expanded(child: SizedBox());
      rows.add(_row(pal, number, white, black));
      ply += 2;
      number++;
    }
    return Container(
      key: const ValueKey('move-list'),
      color: pal.background.withValues(alpha: 0.4),
      child: ListView(children: rows),
    );
  }

  Widget _row(UiPalette pal, int number, Widget? white, Widget black) => Row(children: [
        SizedBox(width: 34, child: Padding(padding: const EdgeInsets.only(left: 8), child: Text('$number.', style: TextStyle(color: pal.muted, fontSize: 12)))),
        white ?? const Expanded(child: Center(child: Text('…'))),
        black,
      ]);
}

class _EvalGraph extends StatelessWidget {
  final ReviewController controller;
  final AppSettings settings;
  const _EvalGraph({required this.controller, required this.settings});

  @override
  Widget build(BuildContext context) {
    final c = controller;
    return LayoutBuilder(builder: (context, box) {
      void seek(double x) {
        final n = c.review!.whiteWinChance.length;
        if (n < 2) return;
        c.goTo((x / box.maxWidth * (n - 1)).round());
      }

      return GestureDetector(
        key: const ValueKey('eval-graph'),
        behavior: HitTestBehavior.opaque,
        onTapDown: (d) => seek(d.localPosition.dx),
        onHorizontalDragUpdate: (d) => seek(d.localPosition.dx),
        child: CustomPaint(size: box.biggest, painter: _GraphPainter(c.review!, c.cursor, settings)),
      );
    });
  }
}

class _GraphPainter extends CustomPainter {
  final GameReview review;
  final int cursor;
  final AppSettings settings;
  _GraphPainter(this.review, this.cursor, this.settings);

  @override
  void paint(Canvas canvas, Size size) {
    final v = review.whiteWinChance;
    final n = v.length;
    canvas.drawRect(Offset.zero & size, Paint()..color = const Color(0xFF2B2B30));
    if (n < 2) return;
    final dx = size.width / (n - 1);
    double y(int i) => size.height * (1 - v[i] / 100);

    final area = Path()..moveTo(0, size.height);
    for (var i = 0; i < n; i++) {
      area.lineTo(i * dx, y(i));
    }
    area.lineTo(size.width, size.height);
    area.close();
    canvas.drawPath(area, Paint()..color = const Color(0xFFEDEDED));

    final line = Path()..moveTo(0, y(0));
    for (var i = 1; i < n; i++) {
      line.lineTo(i * dx, y(i));
    }
    canvas.drawPath(line, Paint()..color = const Color(0xFF9A9AA5)..style = PaintingStyle.stroke..strokeWidth = 1.5);
    canvas.drawLine(Offset(0, size.height / 2), Offset(size.width, size.height / 2),
        Paint()..color = const Color(0x66FF8A2A)..strokeWidth = 1);

    // Marks on the moments that matter.
    for (final p in review.plies) {
      const marked = {MoveLabel.brilliant, MoveLabel.great, MoveLabel.mistake, MoveLabel.miss, MoveLabel.blunder};
      if (!marked.contains(p.label)) continue;
      canvas.drawCircle(Offset(p.ply * dx, y(p.ply)), 3.2, Paint()..color = labelStyle(p.label).color);
    }

    final x = cursor * dx;
    canvas.drawLine(Offset(x, 0), Offset(x, size.height), Paint()..color = const Color(0xFFFF8A2A)..strokeWidth = 2);
  }

  @override
  bool shouldRepaint(covariant _GraphPainter old) => old.cursor != cursor || old.review != review;
}
