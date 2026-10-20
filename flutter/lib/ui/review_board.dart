import 'dart:math' as math;

import 'package:flutter/material.dart';

import '../core/types.dart';
import '../review/review.dart';
import 'app_settings.dart';
import 'label_style.dart';
import 'piece_art.dart';
import 'review_controller.dart';

/// Read-only board for the position being reviewed: the last move, a badge with the move's label on its
/// destination square and an arrow for the engine's preferred move.
class ReviewBoard extends StatelessWidget {
  final ReviewController controller;
  final AppSettings settings;
  const ReviewBoard({super.key, required this.controller, required this.settings});

  @override
  Widget build(BuildContext context) {
    return LayoutBuilder(builder: (context, constraints) {
      final size = constraints.biggest.shortestSide;
      return SizedBox(
        width: size,
        height: size,
        child: RepaintBoundary(
          child: CustomPaint(painter: _ReviewBoardPainter(controller, settings)),
        ),
      );
    });
  }
}

class _ReviewBoardPainter extends CustomPainter {
  final ReviewController c;
  final AppSettings settings;
  _ReviewBoardPainter(this.c, this.settings) : super(repaint: Listenable.merge([c, settings]));

  Rect _cell(int sq, double cell) {
    final col = c.flipped ? 7 - fileOf(sq) : fileOf(sq);
    final row = c.flipped ? rankOf(sq) : 7 - rankOf(sq);
    return Rect.fromLTWH(col * cell, row * cell, cell, cell);
  }

  @override
  void paint(Canvas canvas, Size size) {
    final cell = size.width / 8;
    final board = c.positionAt(c.cursor);
    final colors = settings.board, set = settings.pieces;

    for (var sq = 0; sq < 64; sq++) {
      final light = (fileOf(sq) + rankOf(sq)) % 2 == 1;
      canvas.drawRect(_cell(sq, cell), Paint()..color = light ? colors.light : colors.dark);
    }

    final game = c.game!;
    final played = c.cursor > 0 ? game.history[c.cursor - 1].move : null;
    final ply = c.cursor > 0 && c.review != null ? c.review!.plies[c.cursor - 1] : null;

    if (played != null) {
      final tint = Paint()..color = lastMoveTint;
      canvas.drawRect(_cell(played.from, cell), tint);
      canvas.drawRect(_cell(played.to, cell), tint);
    }

    if (settings.coordinates) _coordinates(canvas, cell, colors);

    for (var sq = 0; sq < 64; sq++) {
      final p = board.pieceAt(sq);
      if (!p.isNone) paintPiece(canvas, p, _cell(sq, cell), set);
    }

    // Engine's preferred move when the played one was worse.
    if (ply != null && c.showBestMove && ply.hasBetterMove && ply.bestMove!.length >= 4) {
      final from = squareFromName(ply.bestMove!.substring(0, 2)), to = squareFromName(ply.bestMove!.substring(2, 4));
      if (from != noSquare && to != noSquare) {
        _arrow(canvas, _cell(from, cell).center, _cell(to, cell).center, cell, const Color(0xCC81B64C));
      }
    }

    // Label badge on the destination square of the move just played.
    if (ply != null && played != null) _badge(canvas, _cell(played.to, cell), cell, ply);
  }

  void _badge(Canvas canvas, Rect square, double cell, PlyReview ply) {
    final style = labelStyle(ply.label);
    final r = cell * 0.24;
    final center = Offset(square.right - r * 0.9, square.top + r * 0.9);
    canvas.drawCircle(center + Offset(0, r * 0.08), r, Paint()..color = const Color(0x55000000));
    canvas.drawCircle(center, r, Paint()..color = style.color);
    canvas.drawCircle(
        center, r, Paint()..color = Colors.white..style = PaintingStyle.stroke..strokeWidth = math.max(1, cell * 0.02));
    final icon = style.icon;
    final tp = TextPainter(
      text: icon != null
          ? TextSpan(
              text: String.fromCharCode(icon.codePoint),
              style: TextStyle(color: Colors.white, fontSize: r * 1.3, fontFamily: icon.fontFamily, package: icon.fontPackage),
            )
          : TextSpan(
              text: style.glyph,
              style: TextStyle(color: Colors.white, fontSize: r * (style.glyph.length > 1 ? 1.0 : 1.3), fontWeight: FontWeight.w800),
            ),
      textDirection: TextDirection.ltr,
    )..layout();
    tp.paint(canvas, center - Offset(tp.width / 2, tp.height / 2));
  }

  void _arrow(Canvas canvas, Offset from, Offset to, double cell, Color color) {
    final dir = to - from;
    final len = dir.distance;
    if (len < 1) return;
    final unit = dir / len;
    final width = cell * 0.17, head = cell * 0.36;
    final tail = from + unit * (cell * 0.3);
    final neck = to - unit * head;
    final normal = Offset(-unit.dy, unit.dx);
    final path = Path()
      ..moveTo((tail + normal * width / 2).dx, (tail + normal * width / 2).dy)
      ..lineTo((neck + normal * width / 2).dx, (neck + normal * width / 2).dy)
      ..lineTo((neck + normal * head * 0.6).dx, (neck + normal * head * 0.6).dy)
      ..lineTo(to.dx, to.dy)
      ..lineTo((neck - normal * head * 0.6).dx, (neck - normal * head * 0.6).dy)
      ..lineTo((neck - normal * width / 2).dx, (neck - normal * width / 2).dy)
      ..lineTo((tail - normal * width / 2).dx, (tail - normal * width / 2).dy)
      ..close();
    canvas.drawPath(path, Paint()..color = color);
  }

  void _coordinates(Canvas canvas, double cell, BoardTheme colors) {
    final style = TextStyle(fontSize: cell * 0.17, fontWeight: FontWeight.w600);
    void label(String text, Offset at, Color color, {bool right = false, bool bottom = false}) {
      final tp = TextPainter(text: TextSpan(text: text, style: style.copyWith(color: color)), textDirection: TextDirection.ltr)
        ..layout();
      canvas.save();
      canvas.translate(at.dx - (right ? tp.width : 0), at.dy - (bottom ? tp.height : 0));
      tp.paint(canvas, Offset.zero);
      canvas.restore();
    }

    for (var i = 0; i < 8; i++) {
      final fileSq = makeSquare(i, c.flipped ? 7 : 0);
      final fr = _cell(fileSq, cell);
      final lightF = (fileOf(fileSq) + rankOf(fileSq)) % 2 == 1;
      label(String.fromCharCode(97 + i), fr.bottomRight - Offset(cell * 0.06, cell * 0.03), lightF ? colors.dark : colors.light,
          right: true, bottom: true);
      final rankSq = makeSquare(c.flipped ? 7 : 0, i);
      final rr = _cell(rankSq, cell);
      final lightR = (fileOf(rankSq) + rankOf(rankSq)) % 2 == 1;
      label('${i + 1}', rr.topLeft + Offset(cell * 0.06, cell * 0.03), lightR ? colors.dark : colors.light);
    }
  }

  @override
  bool shouldRepaint(covariant _ReviewBoardPainter old) => true;
}
