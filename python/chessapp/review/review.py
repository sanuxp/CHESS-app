"""Turns engine numbers for every position of a game into a per-move review."""
from __future__ import annotations

from dataclasses import dataclass, field

from ..core.game import Game, GameStatus
from ..core.movegen import generate_legal
from ..core.pgn import PlyAnnotation
from ..core.san import move_to_san
from ..core.types import WHITE
from .classify import (
    CHECKMATE, STALEMATE, EngineScore, MoveLabel, PlyInput, ReviewConfig, classify_move, game_accuracy, win_percent,
)
from .material import sacrificed_value
from .openings import Opening, OpeningBook


@dataclass
class PositionEval:
    """What the engine said about one position: its best lines (best first, scores from the side to move's
    view). Empty for finished games (checkmate, stalemate, draws)."""
    lines: list = field(default_factory=list)

    @property
    def is_terminal(self) -> bool:
        return not self.lines


@dataclass
class PlyReview:
    ply: int  # 1-based
    mover: int
    san: str
    played: str  # UCI
    label: MoveLabel
    loss: float
    win_before: float
    win_after: float
    accuracy: float
    best_move: "str | None" = None  # engine's preferred move, UCI
    best_san: "str | None" = None
    score_after: "EngineScore | None" = None  # from White's point of view
    opening: "str | None" = None  # name while still in book

    @property
    def has_better_move(self) -> bool:
        """True when the engine had a different move it liked better."""
        return self.best_move is not None and self.best_move != self.played and self.loss > 0.5


@dataclass
class GameReview:
    plies: list
    accuracy_white: float
    accuracy_black: float
    opening: "str | None"  # deepest opening matched, e.g. "Ruy Lopez: Morphy Defense"
    opening_eco: "str | None"
    white_win_chance: list  # win% for White at positions 0..n, for the graph

    def counts(self, color: int) -> dict:
        out: dict = {}
        for p in self.plies:
            if p.mover == color:
                out[p.label] = out.get(p.label, 0) + 1
        return out


def review_game(game: Game, evals: list, book: "OpeningBook | None" = None,
                config: ReviewConfig = ReviewConfig()) -> GameReview:
    """Scores every move of `game`. `evals` has one entry per position: before move 1, ..., after the last move."""
    history = game.history
    assert len(evals) == len(history) + 1, "need one evaluation per position"

    plies = []
    accuracies = {WHITE: [], 1: []}
    in_book = book is not None
    opening: "Opening | None" = None
    previous_loss = None

    for i, entry in enumerate(history):
        before = entry.before
        mover = before.side_to_move
        played = entry.move.uci
        lines = evals[i].lines
        after = evals[i + 1]
        is_last = i == len(history) - 1

        after_board = history[i + 1].before if i + 1 < len(history) else game.board
        matched = None
        if in_book:
            if book.contains(after_board):
                matched = book.name_of(after_board)
                if matched is not None:
                    opening = matched
            else:
                in_book = False

        result = None
        if is_last and game.status == GameStatus.CHECKMATE:
            result = CHECKMATE
        elif is_last and game.status.is_over:
            result = STALEMATE  # any drawn ending counts 50%

        prev = history[i - 1] if i > 0 else None
        legal = generate_legal(before)
        inp = PlyInput(
            played=played,
            lines_before=lines,
            after_best=None if after.is_terminal else after.lines[0].score,
            after_result=STALEMATE if after.is_terminal and result is None else result,
            legal_moves=len(legal),
            book=in_book,
            recapture=(prev is not None and prev.move.is_capture and entry.move.is_capture
                       and prev.move.to_sq == entry.move.to_sq),
            sacrificed=sacrificed_value(before, played, [] if after.is_terminal else list(after.lines[0].pv)),
            opponent_prev_loss=previous_loss,
        )
        score = classify_move(inp, config)
        if inp.book and score.label != MoveLabel.BOOK:
            in_book = False  # a bad "book" move ends the book
        previous_loss = score.loss
        accuracies[mover].append(score.accuracy)

        best = lines[0].move
        best_san = next((move_to_san(before, m) for m in legal if m.uci == best), None)

        white_view = None
        if not after.is_terminal:
            s = after.lines[0].score  # opponent (side to move after) view
            white_view = s.flipped if mover == WHITE else s

        plies.append(PlyReview(
            ply=i + 1, mover=mover, san=entry.san, played=played, label=score.label, loss=score.loss,
            win_before=score.win_before, win_after=score.win_after, accuracy=score.accuracy,
            best_move=best, best_san=best_san, score_after=white_view,
            opening=matched.name if matched else None,
        ))

    # White's winning chances at every position, for the graph.
    graph = []
    for i in range(len(history) + 1):
        board = history[i].before if i < len(history) else game.board
        e = evals[i]
        if e.is_terminal:
            if i == len(history) and game.status == GameStatus.CHECKMATE:
                graph.append(0.0 if board.side_to_move == WHITE else 100.0)
            else:
                graph.append(50.0)
        else:
            w = win_percent(e.lines[0].score)
            graph.append(w if board.side_to_move == WHITE else 100 - w)

    return GameReview(
        plies=plies,
        accuracy_white=game_accuracy(accuracies[WHITE]),
        accuracy_black=game_accuracy(accuracies[1]),
        opening=opening.name if opening else None,
        opening_eco=opening.eco if opening else None,
        white_win_chance=graph,
    )


def eval_tag(s: "EngineScore | None") -> str:
    """`[%eval 0.27]` / `[%eval #3]`: the evaluation tag Lichess and other tools read from PGN comments."""
    if s is None:
        return ""
    return f"[%eval #{s.mate}]" if s.is_mate else f"[%eval {s.cp / 100:.2f}]"


def review_annotations(review: GameReview) -> list:
    """One PGN annotation per move: the standard glyph ($3 !!, $1 !, $6 ?!, $2 ?, $4 ??) and a comment with the
    evaluation, the label and the engine's preferred move when it differs."""
    out = []
    for p in review.plies:
        better = f". Best: {p.best_san}" if p.has_better_move else ""
        out.append(PlyAnnotation(nag=p.label.nag, comment=f"{eval_tag(p.score_after)} {p.label.title}{better}".strip()))
    return out
