import 'package:flutter/material.dart';
import 'package:flutter/services.dart';

import '../core/types.dart';
import 'app_settings.dart';
import 'piece_art.dart';

/// Opens the settings view as a dialog. It works in wide and narrow layouts (scrolls when short);
/// changes apply live to the board behind it.
Future<void> showThemesDialog(BuildContext context, AppSettings settings) {
  return showDialog<void>(
    context: context,
    barrierColor: const Color(0x66000000),
    builder: (dialogContext) => ListenableBuilder(
      listenable: settings,
      builder: (context, _) => Dialog(
        backgroundColor: settings.palette.panel,
        insetPadding: const EdgeInsets.symmetric(horizontal: 16, vertical: 24),
        shape: const RoundedRectangleBorder(borderRadius: BorderRadius.zero),
        child: ThemesView(settings: settings, onClose: () => Navigator.of(dialogContext).pop()),
      ),
    ),
  );
}

/// Rows BOARD, PIECES, INTERFACE (cycle with arrows or by tapping the row), COORDINATES, MOVE HINTS,
/// ANIMATIONS (toggles), then RESET and DONE.
class ThemesView extends StatelessWidget {
  final AppSettings settings;
  final VoidCallback onClose;
  const ThemesView({super.key, required this.settings, required this.onClose});

  @override
  Widget build(BuildContext context) {
    return ListenableBuilder(
      listenable: settings,
      builder: (context, _) {
        final pal = settings.palette;
        return Focus(
          autofocus: true,
          child: CallbackShortcuts(
            bindings: {const SingleActivator(LogicalKeyboardKey.escape): onClose},
            child: ConstrainedBox(
              constraints: const BoxConstraints(maxWidth: 440),
              child: SingleChildScrollView(
                padding: const EdgeInsets.all(18),
                child: Column(mainAxisSize: MainAxisSize.min, crossAxisAlignment: CrossAxisAlignment.stretch, children: [
                  Text('THEMES', style: TextStyle(color: pal.text, fontSize: 22, fontWeight: FontWeight.w800, letterSpacing: 2)),
                  const SizedBox(height: 10),
                  _CycleRow(
                    id: 'board',
                    label: 'BOARD',
                    value: settings.board.name,
                    palette: pal,
                    onStep: settings.cycleBoard,
                    preview: Row(children: [
                      Container(width: 28, height: 28, color: settings.board.light),
                      Container(width: 28, height: 28, color: settings.board.dark),
                    ]),
                  ),
                  _CycleRow(
                    id: 'pieces',
                    label: 'PIECES',
                    value: settings.pieces.name,
                    palette: pal,
                    onStep: settings.cyclePieces,
                    preview: CustomPaint(size: const Size(56, 28), painter: _PawnPreview(settings.pieces, settings.board)),
                  ),
                  _CycleRow(
                    id: 'interface',
                    label: 'INTERFACE',
                    value: pal.name,
                    palette: pal,
                    onStep: settings.cyclePalette,
                    preview: Container(
                      width: 56,
                      height: 28,
                      decoration: BoxDecoration(color: pal.background, border: Border.all(color: pal.muted)),
                      child: Row(children: [
                        const Spacer(),
                        Container(width: 20, color: pal.panel, alignment: Alignment.center, child: Container(width: 12, height: 8, color: pal.button)),
                      ]),
                    ),
                  ),
                  _ToggleRow(id: 'coordinates', label: 'COORDINATES', value: settings.coordinates, palette: pal, onChanged: settings.setCoordinates),
                  _ToggleRow(id: 'hints', label: 'MOVE HINTS', value: settings.hints, palette: pal, onChanged: settings.setHints),
                  _ToggleRow(id: 'animations', label: 'ANIMATIONS', value: settings.animations, palette: pal, onChanged: settings.setAnimations),
                  const SizedBox(height: 14),
                  Row(children: [
                    Expanded(child: _DialogButton('RESET', settings.reset, pal, key: const ValueKey('themes-reset'))),
                    const SizedBox(width: 8),
                    Expanded(child: _DialogButton('DONE', onClose, pal, key: const ValueKey('themes-done'))),
                  ]),
                ]),
              ),
            ),
          ),
        );
      },
    );
  }
}

TextStyle _labelStyle(UiPalette pal) => TextStyle(color: pal.muted, fontSize: 12, fontWeight: FontWeight.w700, letterSpacing: 1);

class _CycleRow extends StatelessWidget {
  final String id, label, value;
  final UiPalette palette;
  final void Function(int delta) onStep;
  final Widget preview;
  const _CycleRow({
    required this.id,
    required this.label,
    required this.value,
    required this.palette,
    required this.onStep,
    required this.preview,
  });

  Widget _arrow(IconData icon, int delta, String key) => IconButton(
        key: ValueKey('$key-$id'),
        onPressed: () => onStep(delta),
        icon: Icon(icon, color: palette.text),
        padding: EdgeInsets.zero,
        constraints: const BoxConstraints.tightFor(width: 32, height: 32),
      );

  @override
  Widget build(BuildContext context) {
    return InkWell(
      key: ValueKey('row-$id'),
      onTap: () => onStep(1),
      child: Padding(
        padding: const EdgeInsets.symmetric(vertical: 6),
        child: Row(children: [
          SizedBox(width: 100, child: Text(label, style: _labelStyle(palette))),
          _arrow(Icons.chevron_left, -1, 'prev'),
          Expanded(
            child: Text(value,
                textAlign: TextAlign.center,
                overflow: TextOverflow.ellipsis,
                style: TextStyle(color: palette.text, fontSize: 15, fontWeight: FontWeight.w600)),
          ),
          _arrow(Icons.chevron_right, 1, 'next'),
          const SizedBox(width: 8),
          SizedBox(width: 56, height: 28, child: preview),
        ]),
      ),
    );
  }
}

class _ToggleRow extends StatelessWidget {
  final String id, label;
  final bool value;
  final UiPalette palette;
  final ValueChanged<bool> onChanged;
  const _ToggleRow({required this.id, required this.label, required this.value, required this.palette, required this.onChanged});

  @override
  Widget build(BuildContext context) {
    return InkWell(
      key: ValueKey('row-$id'),
      onTap: () => onChanged(!value),
      child: Padding(
        padding: const EdgeInsets.symmetric(vertical: 8),
        child: Row(children: [
          Expanded(child: Text(label, style: _labelStyle(palette))),
          Container(
            width: 56,
            padding: const EdgeInsets.symmetric(vertical: 4),
            alignment: Alignment.center,
            color: value ? palette.button : palette.buttonDisabled,
            child: Text(value ? 'ON' : 'OFF',
                key: ValueKey('value-$id'),
                style: TextStyle(color: value ? palette.buttonText : palette.muted, fontSize: 13, fontWeight: FontWeight.w700)),
          ),
        ]),
      ),
    );
  }
}

class _DialogButton extends StatelessWidget {
  final String label;
  final VoidCallback onPressed;
  final UiPalette palette;
  const _DialogButton(this.label, this.onPressed, this.palette, {super.key});

  @override
  Widget build(BuildContext context) {
    return SizedBox(
      height: 42,
      child: FilledButton(
        onPressed: onPressed,
        style: FilledButton.styleFrom(
          foregroundColor: palette.buttonText,
          shape: const RoundedRectangleBorder(borderRadius: BorderRadius.zero),
        ).copyWith(
          backgroundColor: WidgetStateProperty.resolveWith((states) {
            if (states.contains(WidgetState.hovered) || states.contains(WidgetState.pressed)) return palette.buttonHover;
            return palette.button;
          }),
        ),
        child: Text(label, style: const TextStyle(fontWeight: FontWeight.w600, letterSpacing: 1)),
      ),
    );
  }
}

/// A white pawn on a light square and a black pawn on a dark one.
class _PawnPreview extends CustomPainter {
  final PieceSet set;
  final BoardTheme board;
  _PawnPreview(this.set, this.board);

  @override
  void paint(Canvas canvas, Size size) {
    final h = size.height;
    final left = Rect.fromLTWH(0, 0, h, h), right = Rect.fromLTWH(h, 0, h, h);
    canvas.drawRect(left, Paint()..color = board.light);
    canvas.drawRect(right, Paint()..color = board.dark);
    paintPiece(canvas, const Piece(PieceType.pawn, PieceColor.white), left, set);
    paintPiece(canvas, const Piece(PieceType.pawn, PieceColor.black), right, set);
  }

  @override
  bool shouldRepaint(covariant _PawnPreview old) => old.set != set || old.board != board;
}
