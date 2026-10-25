"""Runs a whole game through an engine and turns the numbers into a GameReview."""
from __future__ import annotations

from ..core.game import Game
from .classify import ReviewConfig
from .openings import OpeningBook
from .review import GameReview, PositionEval, review_game
from .uci import EngineException, UciEngine

# Quick = 12, standard = 16, deep = 20. Shallow searches miss deep tactics (see docs/REVIEW.md section 6).
DEFAULT_REVIEW_DEPTH = 16


class ReviewCancelled(Exception):
    def __str__(self) -> str:
        return "Review cancelled"


class EngineAnalyzer:
    """One search per position is enough (docs/REVIEW.md section 1); results are cached by FEN."""

    def __init__(self, engine: UciEngine, depth: int = DEFAULT_REVIEW_DEPTH, multi_pv: int = 3):
        self.engine = engine
        self.depth = depth
        self.multi_pv = multi_pv
        self._cache: dict = {}

    def configure(self, threads: int = 1, hash_mb: int = 128) -> None:
        """Gives the engine its working options; call once after the handshake."""
        # One thread by default: multi-threaded Stockfish was much slower on the development machine.
        self.engine.set_option("Threads", threads)
        self.engine.set_option("Hash", hash_mb)

    def evaluate(self, fen: str, is_cancelled=None) -> list:
        key = (self.depth, self.multi_pv, fen)
        hit = self._cache.get(key)
        if hit is None:
            hit = self._cache[key] = self.engine.analyze(
                fen, depth=self.depth, multi_pv=self.multi_pv, is_cancelled=is_cancelled)
        return hit

    def review(self, game: Game, book: "OpeningBook | None" = None, config: ReviewConfig = ReviewConfig(),
               on_progress=None, is_cancelled=None) -> GameReview:
        """Analyses every position of `game` and scores each move.

        `on_progress(done, total)` is called after each position; `is_cancelled()` is polled between positions
        and during searches.
        """
        history = game.history
        fens = [h.before.to_fen() for h in history] + [game.board.to_fen()]
        evals = []
        for i, fen in enumerate(fens):
            if is_cancelled is not None and is_cancelled():
                raise ReviewCancelled()
            last = i == len(fens) - 1
            if last and game.status.is_over:
                evals.append(PositionEval([]))  # nothing left to play, nothing to search
            else:
                try:
                    evals.append(PositionEval(self.evaluate(fen, is_cancelled)))
                except EngineException as e:
                    if str(e) == "cancelled":
                        raise ReviewCancelled() from e
                    raise
            if on_progress is not None:
                on_progress(i + 1, len(fens))
        return review_game(game, evals, book=book, config=config)
