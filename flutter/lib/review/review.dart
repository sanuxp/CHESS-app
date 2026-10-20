import '../core/game.dart';
import '../core/movegen.dart';
import '../core/pgn.dart';
import '../core/san.dart';
import '../core/types.dart';
import 'classify.dart';
import 'material.dart';
import 'openings.dart';

/// What the engine said about one position: its best lines (best first, scores from the side to move's view).
/// Empty for finished games (checkmate, stalemate, draws).
class PositionEval {
  final List<EngineLine> lines;
  const PositionEval(this.lines);
  bool get isTerminal => lines.isEmpty;
}

class PlyReview {
  final int ply; // 1-based
  final PieceColor mover;
  final String san;
  final String played; // UCI
  final MoveLabel label;
  final double loss, winBefore, winAfter, accuracy;
  final String? bestMove; // engine's preferred move, UCI
  final String? bestSan;
  final EngineScore? scoreAfter; // from White's point of view
  final String? opening; // name while still in book

  const PlyReview({
    required this.ply,
    required this.mover,
    required this.san,
    required this.played,
    required this.label,
    required this.loss,
    required this.winBefore,
    required this.winAfter,
    required this.accuracy,
    this.bestMove,
    this.bestSan,
    this.scoreAfter,
    this.opening,
  });

  /// True when the engine had a different move it liked better.
  bool get hasBetterMove => bestMove != null && bestMove != played && loss > 0.5;
}

class GameReview {
  final List<PlyReview> plies;
  final double accuracyWhite, accuracyBlack;
  final String? opening; // deepest opening matched, e.g. "Ruy Lopez: Morphy Defense"
  final String? openingEco;
  final List<double> whiteWinChance; // win% for White at positions 0..n, for the graph

  const GameReview({
    required this.plies,
    required this.accuracyWhite,
    required this.accuracyBlack,
    required this.opening,
    required this.openingEco,
    required this.whiteWinChance,
  });

  Map<MoveLabel, int> counts(PieceColor color) {
    final m = <MoveLabel, int>{};
    for (final p in plies) {
      if (p.mover == color) m[p.label] = (m[p.label] ?? 0) + 1;
    }
    return m;
  }
}

/// Scores every move of [game]. [evals] has one entry per position: before move 1, ..., after the last move.
GameReview reviewGame(
  Game game,
  List<PositionEval> evals, {
  OpeningBook? book,
  ReviewConfig config = const ReviewConfig(),
}) {
  final history = game.history;
  assert(evals.length == history.length + 1, 'need one evaluation per position');

  final plies = <PlyReview>[];
  final accuracies = {PieceColor.white: <double>[], PieceColor.black: <double>[]};
  var inBook = book != null;
  Opening? opening;
  double? previousLoss;

  for (var i = 0; i < history.length; i++) {
    final entry = history[i];
    final before = entry.before;
    final mover = before.sideToMove;
    final played = entry.move.uci;
    final lines = evals[i].lines;
    final after = evals[i + 1];
    final isLast = i == history.length - 1;

    // Position after the move, for the book check.
    final afterBoard = i + 1 < history.length ? history[i + 1].before : game.board;
    Opening? matched;
    if (inBook) {
      if (book!.contains(afterBoard)) {
        matched = book.nameOf(afterBoard);
        if (matched != null) opening = matched;
      } else {
        inBook = false;
      }
    }

    var result = AfterResult.none;
    if (isLast && game.status == GameStatus.checkmate) {
      result = AfterResult.checkmate;
    } else if (isLast && game.status.isOver) {
      result = AfterResult.stalemate; // any drawn ending counts 50%
    }

    final prev = i > 0 ? history[i - 1] : null;
    final input = PlyInput(
      played: played,
      linesBefore: lines,
      afterBest: after.isTerminal ? null : after.lines.first.score,
      afterResult: after.isTerminal && result == AfterResult.none ? AfterResult.stalemate : result,
      legalMoves: generateLegal(before).length,
      book: inBook,
      recapture: prev != null && prev.move.isCapture && entry.move.isCapture && prev.move.to == entry.move.to,
      sacrificed: sacrificedValue(before, played, after.isTerminal ? const [] : after.lines.first.pv),
      opponentPrevLoss: previousLoss,
    );
    final score = classifyMove(input, config);
    if (input.book && score.label != MoveLabel.book) inBook = false; // a bad "book" move ends the book
    previousLoss = score.loss;
    accuracies[mover]!.add(score.accuracy);

    final best = lines.first.move;
    String? bestSan;
    for (final m in generateLegal(before)) {
      if (m.uci == best) {
        bestSan = moveToSan(before, m);
        break;
      }
    }

    EngineScore? whiteView;
    if (!after.isTerminal) {
      final s = after.lines.first.score; // opponent (side to move after) view
      whiteView = mover == PieceColor.white ? s.flipped : s;
    }

    plies.add(PlyReview(
      ply: i + 1,
      mover: mover,
      san: entry.san,
      played: played,
      label: score.label,
      loss: score.loss,
      winBefore: score.winBefore,
      winAfter: score.winAfter,
      accuracy: score.accuracy,
      bestMove: best,
      bestSan: bestSan,
      scoreAfter: whiteView,
      opening: matched?.name,
    ));
  }

  // White's winning chances at every position, for the graph.
  final graph = <double>[];
  for (var i = 0; i <= history.length; i++) {
    final board = i < history.length ? history[i].before : game.board;
    final e = evals[i];
    if (e.isTerminal) {
      graph.add(i == history.length && game.status == GameStatus.checkmate
          ? (board.sideToMove == PieceColor.white ? 0 : 100)
          : 50);
    } else {
      final w = winPercent(e.lines.first.score);
      graph.add(board.sideToMove == PieceColor.white ? w : 100 - w);
    }
  }

  return GameReview(
    plies: plies,
    accuracyWhite: gameAccuracy(accuracies[PieceColor.white]!),
    accuracyBlack: gameAccuracy(accuracies[PieceColor.black]!),
    opening: opening?.name,
    openingEco: opening?.eco,
    whiteWinChance: graph,
  );
}

/// `[%eval 0.27]` / `[%eval #3]`: the evaluation tag Lichess and other tools read from PGN comments.
String evalTag(EngineScore? s) {
  if (s == null) return '';
  return s.isMate ? '[%eval #${s.mate}]' : '[%eval ${(s.cp! / 100).toStringAsFixed(2)}]';
}

/// One PGN annotation per move: the standard glyph ($3 !!, $1 !, $6 ?!, $2 ?, $4 ??) and a comment with the
/// evaluation, the label and the engine's preferred move when it differs.
List<PlyAnnotation> reviewAnnotations(GameReview review) => [
      for (final p in review.plies)
        PlyAnnotation(
          nag: p.label.nag,
          comment: '${evalTag(p.scoreAfter)} ${p.label.title}${p.hasBetterMove ? '. Best: ${p.bestSan}' : ''}'.trim(),
        ),
    ];
