"""Move scoring and labelling. Pure logic, no engine and no UI: see docs/REVIEW.md, which this follows
and which docs/testdata/review.json pins down."""
from __future__ import annotations

import enum
import math
from dataclasses import dataclass, fields


@dataclass(frozen=True)
class EngineScore:
    """An engine score from the point of view of the side to move: centipawns or moves to mate."""
    cp: "int | None" = None
    mate: "int | None" = None  # negative when the side to move is being mated

    @staticmethod
    def from_json(d: dict) -> "EngineScore":
        return EngineScore(mate=d["mate"]) if "mate" in d else EngineScore(cp=d["cp"])

    @property
    def is_mate(self) -> bool:
        return self.mate is not None

    @property
    def flipped(self) -> "EngineScore":
        """The same score seen from the other side."""
        return EngineScore(mate=-self.mate) if self.is_mate else EngineScore(cp=-self.cp)

    def __str__(self) -> str:
        if self.is_mate:
            return f"M{self.mate}"
        return f"{'+' if self.cp >= 0 else ''}{self.cp / 100:.2f}"


@dataclass(frozen=True)
class EngineLine:
    """One line of engine output: its first move, score and the full variation (UCI moves)."""
    move: str
    score: EngineScore
    pv: tuple = ()


class MoveLabel(enum.Enum):
    FORCED = "forced"
    BOOK = "book"
    BRILLIANT = "brilliant"
    GREAT = "great"
    BEST = "best"
    EXCELLENT = "excellent"
    GOOD = "good"
    MISS = "miss"
    INACCURACY = "inaccuracy"
    MISTAKE = "mistake"
    BLUNDER = "blunder"

    @property
    def symbol(self) -> str:
        """Chess-style annotation shown next to the move ("" when there is none)."""
        return {"brilliant": "!!", "great": "!", "inaccuracy": "?!", "mistake": "?", "blunder": "??"}.get(
            self.value, "")

    @property
    def nag(self) -> "int | None":
        """Numeric annotation glyph used in PGN, or None."""
        return {"brilliant": 3, "great": 1, "inaccuracy": 6, "mistake": 2, "blunder": 4}.get(self.value)

    @property
    def title(self) -> str:
        return self.value.capitalize()


@dataclass(frozen=True)
class ReviewConfig:
    """All thresholds in one place (win-percentage points unless noted). Defaults match docs/REVIEW.md."""
    brilliantMaxLoss: float = 2
    brilliantMinSacrifice: float = 2
    brilliantMaxWinBefore: float = 80
    brilliantMinWinAfter: float = 45
    brilliantMateMinSacrifice: float = 5
    greatMaxLoss: float = 1
    greatMinGap: float = 15
    bookMaxLoss: float = 10
    bestMaxLoss: float = 0.5
    excellentMaxLoss: float = 2
    goodMaxLoss: float = 5
    missOpponentLoss: float = 10
    missMinWinBefore: float = 60
    missMinLoss: float = 5
    inaccuracyMaxLoss: float = 10
    mistakeMaxLoss: float = 20

    @staticmethod
    def from_json(d: dict) -> "ReviewConfig":
        names = {f.name for f in fields(ReviewConfig)}
        return ReviewConfig(**{k: float(v) for k, v in d.items() if k in names})


def win_percent(s: EngineScore) -> float:
    """Chance of winning (0-100) for the side the score is given for."""
    if s.is_mate:
        return 100.0 if s.mate > 0 else 0.0
    cp = max(-1000, min(1000, s.cp))
    return 50 + 50 * (2 / (1 + math.exp(-0.00368208 * cp)) - 1)


def move_accuracy(loss: float) -> float:
    """Accuracy (0-100) of a move that gave away `loss` win-percentage points."""
    return max(0.0, min(100.0, 103.1668 * math.exp(-0.04354 * loss) - 3.1669))


def game_accuracy(accuracies: list) -> float:
    """Mean of the arithmetic and harmonic means of the per-move accuracies; 100 when there are none."""
    if not accuracies:
        return 100.0
    arithmetic = sum(accuracies) / len(accuracies)
    harmonic = len(accuracies) / sum(1 / max(a, 0.01) for a in accuracies)
    return (arithmetic + harmonic) / 2


NONE_RESULT, CHECKMATE, STALEMATE = None, "checkmate", "stalemate"


@dataclass
class PlyInput:
    """Everything needed to label one move (see docs/REVIEW.md section 1)."""
    played: str  # UCI
    lines_before: list  # best first, scores from the mover's view
    after_best: "EngineScore | None"  # opponent's view; None if the game ended with the move
    after_result: "str | None" = None  # None, "checkmate" or "stalemate"
    legal_moves: int = 30
    book: bool = False
    recapture: bool = False
    sacrificed: float = 0
    opponent_prev_loss: "float | None" = None

    @staticmethod
    def from_json(d: dict) -> "PlyInput":
        after = d.get("afterBest")
        return PlyInput(
            played=d["played"],
            lines_before=[EngineLine(x["move"], EngineScore.from_json(x["score"])) for x in d["linesBefore"]],
            after_best=None if after is None else EngineScore.from_json(after["score"]),
            after_result=d.get("afterResult"),
            legal_moves=d["legalMoves"],
            book=d["book"],
            recapture=d["recapture"],
            sacrificed=d["sacrificed"],
            opponent_prev_loss=d.get("opponentPrevLoss"),
        )


@dataclass
class MoveScore:
    label: MoveLabel
    loss: float
    win_before: float
    win_after: float
    accuracy: float


def classify_move(inp: PlyInput, c: ReviewConfig = ReviewConfig()) -> MoveScore:
    """Scores and labels one move; the first rule that matches wins (docs/REVIEW.md section 3)."""
    lines = inp.lines_before
    win_before = win_percent(lines[0].score)
    if inp.after_result == CHECKMATE:
        win_after = 100.0
    elif inp.after_result == STALEMATE:
        win_after = 50.0
    else:
        win_after = 100 - win_percent(inp.after_best)
    loss = max(0.0, win_before - win_after)
    played_top = inp.played == lines[0].move
    prev = inp.opponent_prev_loss
    top_mates = lines[0].score.is_mate and lines[0].score.mate > 0

    if inp.legal_moves == 1:
        label = MoveLabel.FORCED
    elif inp.book and loss <= c.bookMaxLoss:
        label = MoveLabel.BOOK
    elif loss <= c.brilliantMaxLoss and (
            (inp.sacrificed >= c.brilliantMinSacrifice and win_before <= c.brilliantMaxWinBefore
             and win_after >= c.brilliantMinWinAfter)
            or (inp.sacrificed >= c.brilliantMateMinSacrifice and top_mates and inp.after_result != CHECKMATE)):
        label = MoveLabel.BRILLIANT
    elif (played_top and loss <= c.greatMaxLoss and len(lines) >= 2
          and win_percent(lines[0].score) - win_percent(lines[1].score) >= c.greatMinGap
          and not inp.recapture and inp.after_result != CHECKMATE):
        label = MoveLabel.GREAT
    elif played_top or loss <= c.bestMaxLoss:
        label = MoveLabel.BEST
    elif loss <= c.excellentMaxLoss:
        label = MoveLabel.EXCELLENT
    elif loss <= c.goodMaxLoss:
        label = MoveLabel.GOOD
    elif (prev is not None and prev > c.missOpponentLoss and win_before >= c.missMinWinBefore
          and loss >= c.missMinLoss and not (loss > c.mistakeMaxLoss and win_after < 50)):
        label = MoveLabel.MISS
    elif loss <= c.inaccuracyMaxLoss:
        label = MoveLabel.INACCURACY
    elif loss <= c.mistakeMaxLoss:
        label = MoveLabel.MISTAKE
    else:
        label = MoveLabel.BLUNDER
    return MoveScore(label, loss, win_before, win_after, move_accuracy(loss))
