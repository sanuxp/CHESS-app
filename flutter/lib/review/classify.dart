/// Move scoring and labelling. Pure logic, no engine and no Flutter: see docs/REVIEW.md, which this follows
/// and which docs/testdata/review.json pins down.
library;

import 'dart:math' as math;

/// An engine score from the point of view of the side to move.
class EngineScore {
  final int? cp;
  final int? mate; // moves to mate; negative when the side to move is being mated
  const EngineScore.cp(int this.cp) : mate = null;
  const EngineScore.mate(int this.mate) : cp = null;

  bool get isMate => mate != null;

  factory EngineScore.fromJson(Map<String, dynamic> json) =>
      json.containsKey('mate') ? EngineScore.mate(json['mate'] as int) : EngineScore.cp(json['cp'] as int);

  /// The same score seen from the other side.
  EngineScore get flipped => isMate ? EngineScore.mate(-mate!) : EngineScore.cp(-cp!);

  @override
  String toString() => isMate ? 'M$mate' : '${cp! >= 0 ? '+' : ''}${(cp! / 100).toStringAsFixed(2)}';
}

/// One line of engine output: its first move, score and the full variation (UCI moves).
class EngineLine {
  final String move;
  final EngineScore score;
  final List<String> pv;
  const EngineLine(this.move, this.score, [this.pv = const []]);
}

enum MoveLabel {
  forced,
  book,
  brilliant,
  great,
  best,
  excellent,
  good,
  miss,
  inaccuracy,
  mistake,
  blunder;

  /// Chess-style annotation shown next to the move ("" when there is none).
  String get symbol => switch (this) {
        brilliant => '!!',
        great => '!',
        inaccuracy => '?!',
        mistake => '?',
        blunder => '??',
        _ => '',
      };

  /// Numeric annotation glyph used in PGN, or null.
  int? get nag => switch (this) {
        brilliant => 3,
        great => 1,
        inaccuracy => 6,
        mistake => 2,
        blunder => 4,
        _ => null,
      };

  String get title => name[0].toUpperCase() + name.substring(1);
}

/// All thresholds in one place (win-percentage points unless noted). Defaults match docs/REVIEW.md.
class ReviewConfig {
  final double brilliantMaxLoss, brilliantMinSacrifice, brilliantMaxWinBefore, brilliantMinWinAfter, brilliantMateMinSacrifice;
  final double greatMaxLoss, greatMinGap, bookMaxLoss;
  final double bestMaxLoss, excellentMaxLoss, goodMaxLoss;
  final double missOpponentLoss, missMinWinBefore, missMinLoss;
  final double inaccuracyMaxLoss, mistakeMaxLoss;

  const ReviewConfig({
    this.brilliantMaxLoss = 2,
    this.brilliantMinSacrifice = 2,
    this.brilliantMaxWinBefore = 80,
    this.brilliantMinWinAfter = 45,
    this.brilliantMateMinSacrifice = 5,
    this.greatMaxLoss = 1,
    this.greatMinGap = 15,
    this.bookMaxLoss = 10,
    this.bestMaxLoss = 0.5,
    this.excellentMaxLoss = 2,
    this.goodMaxLoss = 5,
    this.missOpponentLoss = 10,
    this.missMinWinBefore = 60,
    this.missMinLoss = 5,
    this.inaccuracyMaxLoss = 10,
    this.mistakeMaxLoss = 20,
  });

  factory ReviewConfig.fromJson(Map<String, dynamic> j) => ReviewConfig(
        brilliantMaxLoss: (j['brilliantMaxLoss'] as num).toDouble(),
        brilliantMinSacrifice: (j['brilliantMinSacrifice'] as num).toDouble(),
        brilliantMaxWinBefore: (j['brilliantMaxWinBefore'] as num).toDouble(),
        brilliantMinWinAfter: (j['brilliantMinWinAfter'] as num).toDouble(),
        brilliantMateMinSacrifice: (j['brilliantMateMinSacrifice'] as num).toDouble(),
        greatMaxLoss: (j['greatMaxLoss'] as num).toDouble(),
        greatMinGap: (j['greatMinGap'] as num).toDouble(),
        bookMaxLoss: (j['bookMaxLoss'] as num).toDouble(),
        bestMaxLoss: (j['bestMaxLoss'] as num).toDouble(),
        excellentMaxLoss: (j['excellentMaxLoss'] as num).toDouble(),
        goodMaxLoss: (j['goodMaxLoss'] as num).toDouble(),
        missOpponentLoss: (j['missOpponentLoss'] as num).toDouble(),
        missMinWinBefore: (j['missMinWinBefore'] as num).toDouble(),
        missMinLoss: (j['missMinLoss'] as num).toDouble(),
        inaccuracyMaxLoss: (j['inaccuracyMaxLoss'] as num).toDouble(),
        mistakeMaxLoss: (j['mistakeMaxLoss'] as num).toDouble(),
      );
}

/// Chance of winning (0-100) for the side the score is given for.
double winPercent(EngineScore s) {
  if (s.isMate) return s.mate! > 0 ? 100.0 : 0.0;
  final cp = s.cp!.clamp(-1000, 1000);
  return 50 + 50 * (2 / (1 + math.exp(-0.00368208 * cp)) - 1);
}

/// Accuracy (0-100) of a move that gave away [loss] win-percentage points.
double moveAccuracy(double loss) => (103.1668 * math.exp(-0.04354 * loss) - 3.1669).clamp(0.0, 100.0);

/// Mean of the arithmetic and harmonic means of the per-move accuracies; 100 when there are none.
double gameAccuracy(List<double> accuracies) {
  if (accuracies.isEmpty) return 100;
  final arithmetic = accuracies.reduce((a, b) => a + b) / accuracies.length;
  final harmonic = accuracies.length / accuracies.fold<double>(0, (sum, a) => sum + 1 / math.max(a, 0.01));
  return (arithmetic + harmonic) / 2;
}

enum AfterResult { none, checkmate, stalemate }

/// Everything needed to label one move (see docs/REVIEW.md section 1).
class PlyInput {
  final String played; // UCI
  final List<EngineLine> linesBefore; // best first, scores from the mover's view
  final EngineScore? afterBest; // score of the best line after the move, from the opponent's view
  final AfterResult afterResult;
  final int legalMoves;
  final bool book;
  final bool recapture;
  final double sacrificed;
  final double? opponentPrevLoss;

  const PlyInput({
    required this.played,
    required this.linesBefore,
    this.afterBest,
    this.afterResult = AfterResult.none,
    required this.legalMoves,
    this.book = false,
    this.recapture = false,
    this.sacrificed = 0,
    this.opponentPrevLoss,
  });

  factory PlyInput.fromJson(Map<String, dynamic> j) => PlyInput(
        played: j['played'] as String,
        linesBefore: [
          for (final l in j['linesBefore'] as List)
            EngineLine(l['move'] as String, EngineScore.fromJson(l['score'] as Map<String, dynamic>)),
        ],
        afterBest: j['afterBest'] == null
            ? null
            : EngineScore.fromJson((j['afterBest'] as Map<String, dynamic>)['score'] as Map<String, dynamic>),
        afterResult: switch (j['afterResult']) {
          'checkmate' => AfterResult.checkmate,
          'stalemate' => AfterResult.stalemate,
          _ => AfterResult.none,
        },
        legalMoves: j['legalMoves'] as int,
        book: j['book'] as bool,
        recapture: j['recapture'] as bool,
        sacrificed: (j['sacrificed'] as num).toDouble(),
        opponentPrevLoss: (j['opponentPrevLoss'] as num?)?.toDouble(),
      );
}

class PlyScore {
  final MoveLabel label;
  final double loss, winBefore, winAfter, accuracy;
  const PlyScore(this.label, this.loss, this.winBefore, this.winAfter, this.accuracy);
}

/// Scores and labels one move.
PlyScore classifyMove(PlyInput p, [ReviewConfig c = const ReviewConfig()]) {
  final lines = p.linesBefore;
  final winBefore = winPercent(lines.first.score);
  final double winAfter = switch (p.afterResult) {
    AfterResult.checkmate => 100,
    AfterResult.stalemate => 50,
    AfterResult.none => 100 - winPercent(p.afterBest!),
  };
  final loss = math.max(0.0, winBefore - winAfter);
  final playedTop = p.played == lines.first.move;
  final prev = p.opponentPrevLoss;

  final MoveLabel label;
  if (p.legalMoves == 1) {
    label = MoveLabel.forced;
  } else if (p.book && loss <= c.bookMaxLoss) {
    label = MoveLabel.book;
  } else if (loss <= c.brilliantMaxLoss &&
      ((p.sacrificed >= c.brilliantMinSacrifice &&
              winBefore <= c.brilliantMaxWinBefore &&
              winAfter >= c.brilliantMinWinAfter) ||
          (p.sacrificed >= c.brilliantMateMinSacrifice &&
              (lines.first.score.mate ?? 0) > 0 &&
              p.afterResult != AfterResult.checkmate))) {
    label = MoveLabel.brilliant;
  } else if (playedTop &&
      loss <= c.greatMaxLoss &&
      lines.length >= 2 &&
      winPercent(lines[0].score) - winPercent(lines[1].score) >= c.greatMinGap &&
      !p.recapture &&
      p.afterResult != AfterResult.checkmate) {
    label = MoveLabel.great;
  } else if (playedTop || loss <= c.bestMaxLoss) {
    label = MoveLabel.best;
  } else if (loss <= c.excellentMaxLoss) {
    label = MoveLabel.excellent;
  } else if (loss <= c.goodMaxLoss) {
    label = MoveLabel.good;
  } else if (prev != null &&
      prev > c.missOpponentLoss &&
      winBefore >= c.missMinWinBefore &&
      loss >= c.missMinLoss &&
      !(loss > c.mistakeMaxLoss && winAfter < 50)) {
    label = MoveLabel.miss;
  } else if (loss <= c.inaccuracyMaxLoss) {
    label = MoveLabel.inaccuracy;
  } else if (loss <= c.mistakeMaxLoss) {
    label = MoveLabel.mistake;
  } else {
    label = MoveLabel.blunder;
  }
  return PlyScore(label, loss, winBefore, winAfter, moveAccuracy(loss));
}
