import 'package:flutter/material.dart';

import '../core/game.dart';
import '../core/types.dart';
import 'app_settings.dart';
import 'game_controller.dart';
import 'piece_art.dart';
import 'status_text.dart';

/// Duration of a move animation, see docs/UI_SPEC.md.
const moveAnimationDuration = Duration(milliseconds: 220);

/// The board: squares, pieces, hints, move animation, drag and drop, promotion picker and game-over banner.
class BoardView extends StatefulWidget {
  final GameController controller;
  const BoardView({super.key, required this.controller});

  @override
  State<BoardView> createState() => _BoardViewState();
}

class _BoardViewState extends State<BoardView> with SingleTickerProviderStateMixin {
  late final AnimationController _clock;
  int _serial = 0;

  GameController get c => widget.controller;

  @override
  void initState() {
    super.initState();
    _clock = AnimationController(vsync: this, duration: moveAnimationDuration)
      ..addStatusListener((s) {
        if (s == AnimationStatus.completed) c.finishAnimation();
      });
    _serial = c.animationSerial;
    c.addListener(_onController);
  }

  @override
  void dispose() {
    c.removeListener(_onController);
    _clock.dispose();
    super.dispose();
  }

  void _onController() {
    if (c.animationSerial != _serial) {
      _serial = c.animationSerial;
      _clock.forward(from: 0);
    } else if (c.anim == null && _clock.isAnimating) {
      _clock.stop(); // new game during a move
    }
  }

  int _squareAt(Offset p, double size) {
    if (p.dx < 0 || p.dy < 0 || p.dx >= size || p.dy >= size) return noSquare;
    final cell = size / 8;
    final col = (p.dx / cell).floor().clamp(0, 7), row = (p.dy / cell).floor().clamp(0, 7);
    final file = c.flipped ? 7 - col : col;
    final rank = c.flipped ? row : 7 - row;
    return makeSquare(file, rank);
  }

  @override
  Widget build(BuildContext context) {
    return LayoutBuilder(builder: (context, constraints) {
      final size = constraints.biggest.shortestSide;
      return SizedBox(
        width: size,
        height: size,
        child: Stack(children: [
          Listener(
            behavior: HitTestBehavior.opaque,
            onPointerDown: (e) => c.press(_squareAt(e.localPosition, size), e.localPosition),
            onPointerMove: (e) => c.drag(e.localPosition),
            onPointerUp: (e) => c.release(_squareAt(e.localPosition, size)),
            onPointerCancel: (_) => c.release(noSquare),
            child: RepaintBoundary(
              child: CustomPaint(
                size: Size.square(size),
                painter: _BoardPainter(c, _clock),
              ),
            ),
          ),
          ListenableBuilder(
            listenable: c,
            builder: (context, _) {
              if (c.promotionOpen) return _PromotionPicker(controller: c, size: size);
              if (c.game.status.isOver && !c.busy) return _Banner(controller: c, size: size);
              return const SizedBox.shrink();
            },
          ),
        ]),
      );
    });
  }
}

class _BoardPainter extends CustomPainter {
  final GameController c;
  final Animation<double> clock;
  _BoardPainter(this.c, this.clock) : super(repaint: Listenable.merge([c, c.settings, clock]));

  AppSettings get settings => c.settings;

  Rect _cell(int sq, double cell) {
    final col = c.flipped ? 7 - fileOf(sq) : fileOf(sq);
    final row = c.flipped ? rankOf(sq) : 7 - rankOf(sq);
    return Rect.fromLTWH(col * cell, row * cell, cell, cell);
  }

  @override
  void paint(Canvas canvas, Size size) {
    final cell = size.width / 8;
    final game = c.game, board = game.board;
    final colors = settings.board, set = settings.pieces;

    for (var sq = 0; sq < 64; sq++) {
      final light = (fileOf(sq) + rankOf(sq)) % 2 == 1;
      canvas.drawRect(_cell(sq, cell), Paint()..color = light ? colors.light : colors.dark);
    }

    final last = game.lastMove;
    if (last != null) {
      final tint = Paint()..color = lastMoveTint;
      canvas.drawRect(_cell(last.from, cell), tint);
      canvas.drawRect(_cell(last.to, cell), tint);
    }
    if (c.selected != noSquare) {
      canvas.drawRect(_cell(c.selected, cell), Paint()..color = selectionTint);
    }
    if (game.inCheck || game.status == GameStatus.checkmate) {
      final k = board.kingSquare(board.sideToMove);
      if (k != noSquare) {
        canvas.drawCircle(_cell(k, cell).center, cell / 2 - 2, Paint()..color = checkDisc);
      }
    }

    if (settings.coordinates) _coordinates(canvas, cell);

    final anim = c.anim;
    for (var sq = 0; sq < 64; sq++) {
      final p = board.pieceAt(sq);
      if (p.isNone) continue;
      if (anim != null && (sq == anim.hiddenA || sq == anim.hiddenB || sq == anim.capturedSquare && anim.captured != null)) {
        continue;
      }
      if (c.dragging && sq == c.dragFrom) continue;
      paintPiece(canvas, p, _cell(sq, cell), set);
    }

    // Legal-move hints.
    final hint = Paint()..color = hintDots;
    for (final m in settings.hints ? c.targets : const <Move>[]) {
      final center = _cell(m.to, cell).center;
      if (!board.pieceAt(m.to).isNone || m.isEnPassant) {
        canvas.drawCircle(center, cell * 0.42, hint..style = PaintingStyle.stroke..strokeWidth = cell * 0.08);
        hint.style = PaintingStyle.fill;
      } else {
        canvas.drawCircle(center, cell * 0.16, hint);
      }
    }

    // Pieces in flight.
    if (anim != null) {
      final t = Curves.easeInOut.transform(clock.value);
      Rect lerp(int a, int b) => Rect.lerp(_cell(a, cell), _cell(b, cell), t)!;
      if (anim.captured != null) {
        final alpha = anim.capturedFadesIn ? clock.value : 1 - clock.value;
        paintPiece(canvas, anim.captured!, _cell(anim.capturedSquare, cell), set, opacity: alpha);
      }
      if (anim.rook != null) paintPiece(canvas, anim.rook!, lerp(anim.rookFrom, anim.rookTo), set);
      paintPiece(canvas, anim.piece, lerp(anim.from, anim.to), set);
    }

    // Piece held by the mouse or finger.
    if (c.dragging && c.dragFrom != noSquare) {
      final d = cell * 1.1;
      paintPiece(canvas, board.pieceAt(c.dragFrom), Rect.fromCenter(center: c.dragPos, width: d, height: d), set);
    }
  }

  void _coordinates(Canvas canvas, double cell) {
    final colors = settings.board;
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
  bool shouldRepaint(covariant _BoardPainter old) => true;
}

class _PiecePainter extends CustomPainter {
  final Piece piece;
  final PieceSet set;
  _PiecePainter(this.piece, this.set);
  @override
  void paint(Canvas canvas, Size size) => paintPiece(canvas, piece, Offset.zero & size, set);
  @override
  bool shouldRepaint(covariant _PiecePainter old) => old.piece != piece || old.set != set;
}

class _PromotionPicker extends StatefulWidget {
  final GameController controller;
  final double size;
  const _PromotionPicker({required this.controller, required this.size});

  @override
  State<_PromotionPicker> createState() => _PromotionPickerState();
}

class _PromotionPickerState extends State<_PromotionPicker> {
  int _hover = -1;

  @override
  Widget build(BuildContext context) {
    final c = widget.controller;
    final palette = c.settings.palette;
    const types = [PieceType.queen, PieceType.rook, PieceType.bishop, PieceType.knight];
    final cell = widget.size / 8 * 1.2;
    return GestureDetector(
      behavior: HitTestBehavior.opaque,
      onTap: c.cancelPromotion,
      child: Container(
        color: const Color(0x8C000000),
        alignment: Alignment.center,
        child: Column(mainAxisSize: MainAxisSize.min, children: [
          Text('CHOOSE A PIECE', style: TextStyle(color: Colors.white, fontSize: cell * 0.25, fontWeight: FontWeight.w600)),
          SizedBox(height: cell * 0.12),
          Container(
            padding: EdgeInsets.all(cell * 0.12),
            decoration: BoxDecoration(color: palette.panel, borderRadius: BorderRadius.circular(6)),
            child: Row(mainAxisSize: MainAxisSize.min, children: [
              for (var i = 0; i < 4; i++) ...[
                if (i > 0) SizedBox(width: cell * 0.12),
                MouseRegion(
                  onEnter: (_) => setState(() => _hover = i),
                  onExit: (_) => setState(() => _hover = -1),
                  child: GestureDetector(
                    key: ValueKey('promo-${types[i].name}'),
                    onTap: () => c.choosePromotion(types[i]),
                    child: Container(
                      width: cell,
                      height: cell,
                      color: _hover == i ? palette.buttonHover : c.settings.board.light,
                      child: CustomPaint(painter: _PiecePainter(Piece(types[i], c.promoColor), c.settings.pieces)),
                    ),
                  ),
                ),
              ],
            ]),
          ),
        ]),
      ),
    );
  }
}

class _Banner extends StatelessWidget {
  final GameController controller;
  final double size;
  const _Banner({required this.controller, required this.size});

  @override
  Widget build(BuildContext context) {
    final detail = statusDetail(controller.game);
    return IgnorePointer(
      child: Center(
        child: Container(
          width: size,
          height: size / 4,
          color: const Color(0xCD0F0F14),
          alignment: Alignment.center,
          padding: const EdgeInsets.symmetric(horizontal: 10),
          child: FittedBox(
            fit: BoxFit.scaleDown,
            child: Text(detail, style: const TextStyle(color: Color(0xFFFFD678), fontSize: 40, fontWeight: FontWeight.bold)),
          ),
        ),
      ),
    );
  }
}
