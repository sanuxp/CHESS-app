import 'dart:async';
import 'dart:ui' show Offset;

import 'package:flutter/foundation.dart';

import '../core/board.dart';
import '../core/game.dart';
import '../core/types.dart';
import 'app_settings.dart';

/// A piece sliding between two squares. The game has already applied the move (or undone it),
/// so the board hides the real piece on the "hidden" squares and draws this one in flight.
class MoveAnimation {
  final Piece piece;
  final int from, to;
  final int hiddenA;
  final int hiddenB;
  final Piece? rook;
  final int rookFrom, rookTo;
  final Piece? captured;
  final int capturedSquare;
  final bool capturedFadesIn; // true when undoing a capture

  const MoveAnimation({
    required this.piece,
    required this.from,
    required this.to,
    required this.hiddenA,
    this.hiddenB = noSquare,
    this.rook,
    this.rookFrom = noSquare,
    this.rookTo = noSquare,
    this.captured,
    this.capturedSquare = noSquare,
    this.capturedFadesIn = false,
  });
}

/// All game and interaction state, independent of the widgets (port of the C++ App logic).
/// The board view turns pointer positions into squares and calls the methods below.
class GameController extends ChangeNotifier {
  GameController({AppSettings? settings}) : settings = settings ?? AppSettings();

  /// Look and feel (colours, toggles); the animation toggle is honoured here.
  final AppSettings settings;

  Game game = Game();

  bool flipMode = false; // turn the board towards whoever is to move
  bool flipped = false;

  int selected = noSquare;
  List<Move> targets = const [];

  bool pointerDown = false;
  bool dragging = false;
  int dragFrom = noSquare;
  Offset downPos = Offset.zero;
  Offset dragPos = Offset.zero;

  MoveAnimation? anim;
  int animationSerial = 0; // bumped for every new animation so the view can start its clock

  bool promotionOpen = false;
  int promoFrom = noSquare, promoTo = noSquare;
  PieceColor promoColor = PieceColor.white;

  String toast = '';
  Timer? _toastTimer;

  bool get busy => anim != null;
  bool get canUndo => game.moveCount > 0 && !busy;

  // ---------------------------------------------------------------- pointer input

  /// Pointer pressed. [sq] is the square under it (or [noSquare]), [pos] its position on the board.
  void press(int sq, Offset pos) {
    if (busy) return;
    if (promotionOpen) {
      cancelPromotion();
      return;
    }
    if (sq == noSquare) {
      clearSelection();
      return;
    }
    if (game.status.isOver) return;

    if (selected != noSquare && targets.any((m) => m.to == sq)) {
      _tryMove(sq, dropped: false); // second click of click-click
      return;
    }
    final p = game.board.pieceAt(sq);
    if (!p.isNone && p.color == game.board.sideToMove) {
      _select(sq);
      pointerDown = true;
      dragFrom = sq;
      downPos = dragPos = pos;
    } else {
      clearSelection();
    }
  }

  void drag(Offset pos) {
    if (!pointerDown) return;
    dragPos = pos;
    if (!dragging && (pos - downPos).distanceSquared > 36) dragging = true;
    notifyListeners();
  }

  void release(int sq) {
    final wasDragging = dragging;
    pointerDown = false;
    dragging = false;
    if (!wasDragging || busy) {
      notifyListeners();
      return;
    }
    if (sq != noSquare && sq != dragFrom && targets.any((m) => m.to == sq)) {
      _tryMove(sq, dropped: true); // the piece is already under the pointer: no flight animation
    } else {
      notifyListeners();
    }
  }

  // ---------------------------------------------------------------- actions

  void _select(int sq) {
    selected = sq;
    targets = game.legalMovesFrom(sq);
    notifyListeners();
  }

  void clearSelection() {
    selected = noSquare;
    targets = const [];
    dragging = false;
    pointerDown = false;
    dragFrom = noSquare;
    notifyListeners();
  }

  void _tryMove(int to, {required bool dropped}) {
    final options = targets.where((m) => m.to == to).toList();
    if (options.isEmpty) return;
    if (options.length > 1) {
      // Promotion: ask which piece.
      promotionOpen = true;
      promoFrom = options.first.from;
      promoTo = to;
      promoColor = game.board.sideToMove;
      dragging = false;
      pointerDown = false;
      notifyListeners();
      return;
    }
    _play(options.first, animate: !dropped && settings.animations);
  }

  void choosePromotion(PieceType type) {
    final chosen = targets.where((m) => m.to == promoTo && m.promo == type);
    promotionOpen = false;
    if (chosen.isEmpty) {
      clearSelection();
    } else {
      _play(chosen.first, animate: settings.animations);
    }
  }

  void cancelPromotion() {
    promotionOpen = false;
    clearSelection();
  }

  void _play(Move m, {required bool animate}) {
    final before = game.board.copy();
    if (!game.playMove(m)) return;
    selected = noSquare;
    targets = const [];
    dragging = false;
    pointerDown = false;
    dragFrom = noSquare;
    if (animate) {
      _startAnimation(before, m, reverse: false);
    } else {
      _updateFlip();
    }
    notifyListeners();
  }

  void newGame() {
    game = Game();
    selected = noSquare;
    targets = const [];
    promotionOpen = false;
    anim = null;
    pointerDown = dragging = false;
    _updateFlip();
    say('NEW GAME');
  }

  void undo() {
    if (!canUndo) return;
    final entry = game.history.last;
    game.undo();
    selected = noSquare;
    targets = const [];
    promotionOpen = false;
    if (settings.animations) {
      _startAnimation(entry.before, entry.move, reverse: true);
    } else {
      _updateFlip();
    }
    notifyListeners();
  }

  void toggleFlip() {
    flipMode = !flipMode;
    _updateFlip();
    notifyListeners();
  }

  /// Called by the view when the animation clock has run out.
  void finishAnimation() {
    if (anim == null) return;
    anim = null;
    _updateFlip();
    notifyListeners();
  }

  void say(String text) {
    toast = text;
    _toastTimer?.cancel();
    _toastTimer = Timer(const Duration(seconds: 4), () {
      toast = '';
      notifyListeners();
    });
    notifyListeners();
  }

  void _updateFlip() {
    if (anim != null) return;
    flipped = flipMode && game.board.sideToMove == PieceColor.black;
  }

  void _startAnimation(Board before, Move m, {required bool reverse}) {
    final piece = before.pieceAt(m.from);
    Piece? captured;
    var capturedSquare = noSquare;
    if (m.isCapture) {
      capturedSquare = m.isEnPassant ? m.to + (piece.color == PieceColor.white ? -8 : 8) : m.to;
      captured = before.pieceAt(capturedSquare);
    }
    Piece? rook;
    var rookFrom = noSquare, rookTo = noSquare, hiddenB = noSquare;
    if (m.isCastle) {
      final rank = rankOf(m.from);
      final king = m.flags & flagCastleKing != 0;
      final home = makeSquare(king ? 7 : 0, rank), landed = makeSquare(king ? 5 : 3, rank);
      rook = Piece(PieceType.rook, piece.color);
      rookFrom = reverse ? landed : home;
      rookTo = reverse ? home : landed;
      hiddenB = rookTo;
    }
    final to = reverse ? m.from : m.to;
    anim = MoveAnimation(
      piece: piece,
      from: reverse ? m.to : m.from,
      to: to,
      hiddenA: to,
      hiddenB: hiddenB,
      rook: rook,
      rookFrom: rookFrom,
      rookTo: rookTo,
      captured: captured,
      capturedSquare: capturedSquare,
      capturedFadesIn: reverse,
    );
    animationSerial++;
  }

  @override
  void dispose() {
    _toastTimer?.cancel();
    super.dispose();
  }
}
