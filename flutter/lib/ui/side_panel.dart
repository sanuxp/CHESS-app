import 'package:flutter/material.dart';

import '../core/game.dart';
import '../core/types.dart';
import 'app_settings.dart';
import 'game_controller.dart';
import 'status_text.dart';
import 'themes_view.dart';

/// Turn indicator, status, toast and the six buttons. Beside the board on wide screens,
/// below it on tall ones.
class SidePanel extends StatelessWidget {
  final GameController controller;
  final bool landscape;
  final VoidCallback onSavePgn;
  final VoidCallback onCopyFen;

  const SidePanel({
    super.key,
    required this.controller,
    required this.landscape,
    required this.onSavePgn,
    required this.onCopyFen,
  });

  @override
  Widget build(BuildContext context) {
    return ListenableBuilder(
      listenable: Listenable.merge([controller, controller.settings]),
      builder: (context, _) {
        final pal = controller.settings.palette;
        final game = controller.game;
        final detail = statusDetail(game);
        final over = game.status.isOver;

        final buttons = [
          _PanelButton('NEW GAME', controller.newGame, pal, key: const ValueKey('btn-new')),
          _PanelButton('UNDO', controller.canUndo ? controller.undo : null, pal, key: const ValueKey('btn-undo')),
          _PanelButton('SAVE PGN', onSavePgn, pal, key: const ValueKey('btn-save')),
          _PanelButton('COPY FEN', onCopyFen, pal, key: const ValueKey('btn-fen')),
          _PanelButton(controller.flipMode ? 'FLIP: ON' : 'FLIP: OFF', controller.toggleFlip, pal, key: const ValueKey('btn-flip')),
          _PanelButton('THEMES', () => showThemesDialog(context, controller.settings), pal, key: const ValueKey('btn-themes')),
        ];

        final turn = Row(children: [
          Container(
            width: 18,
            height: 18,
            decoration: BoxDecoration(
              shape: BoxShape.circle,
              color: game.board.sideToMove == PieceColor.white ? const Color(0xFFF8F6EE) : const Color(0xFF141418),
              border: Border.all(color: pal.muted, width: 2),
            ),
          ),
          const SizedBox(width: 10),
          Flexible(
            child: Text(statusHeadline(game),
                key: const ValueKey('headline'), style: TextStyle(color: pal.text, fontSize: 16, fontWeight: FontWeight.w600)),
          ),
        ]);

        final info = <Widget>[
          if (landscape)
            Padding(
              padding: const EdgeInsets.only(bottom: 10),
              child: Text('CHESS', style: TextStyle(color: pal.text, fontSize: 32, fontWeight: FontWeight.w800, letterSpacing: 2)),
            ),
          turn,
          if (detail.isNotEmpty)
            Padding(
              padding: const EdgeInsets.only(top: 8),
              child: Text(detail,
                  key: const ValueKey('detail'),
                  style: TextStyle(color: over ? const Color(0xFFFFC85A) : const Color(0xFFFF7878), fontSize: 15, fontWeight: FontWeight.w600)),
            ),
        ];
        final toast = controller.toast.isEmpty
            ? const SizedBox(height: 18)
            : Padding(
                padding: const EdgeInsets.only(bottom: 8),
                child: Text(controller.toast, key: const ValueKey('toast'), style: TextStyle(color: pal.muted, fontSize: 12)),
              );

        return Container(
          color: pal.panel,
          padding: const EdgeInsets.all(14),
          child: landscape
              ? Column(crossAxisAlignment: CrossAxisAlignment.stretch, children: [
                  ...info,
                  const Spacer(),
                  toast,
                  for (final b in buttons) Padding(padding: const EdgeInsets.only(top: 8), child: b),
                ])
              : Column(crossAxisAlignment: CrossAxisAlignment.stretch, children: [
                  ...info,
                  const Spacer(),
                  toast,
                  LayoutBuilder(builder: (context, box) {
                    const gap = 8.0;
                    final w = (box.maxWidth - 2 * gap) / 3;
                    return Wrap(spacing: gap, runSpacing: gap, children: [
                      for (final b in buttons) SizedBox(width: w, child: b),
                    ]);
                  }),
                ]),
        );
      },
    );
  }
}

class _PanelButton extends StatelessWidget {
  final String label;
  final VoidCallback? onPressed;
  final UiPalette palette;
  const _PanelButton(this.label, this.onPressed, this.palette, {super.key});

  @override
  Widget build(BuildContext context) {
    return SizedBox(
      height: 42,
      child: FilledButton(
        onPressed: onPressed,
        style: FilledButton.styleFrom(
          foregroundColor: palette.buttonText,
          disabledForegroundColor: palette.muted,
          shape: const RoundedRectangleBorder(borderRadius: BorderRadius.zero),
          padding: const EdgeInsets.symmetric(horizontal: 4),
        ).copyWith(
          backgroundColor: WidgetStateProperty.resolveWith((states) {
            if (states.contains(WidgetState.disabled)) return palette.buttonDisabled;
            if (states.contains(WidgetState.hovered) || states.contains(WidgetState.pressed)) return palette.buttonHover;
            return palette.button;
          }),
        ),
        child: FittedBox(fit: BoxFit.scaleDown, child: Text(label, style: const TextStyle(fontWeight: FontWeight.w600, letterSpacing: 1))),
      ),
    );
  }
}
