"""Opening database (docs/data/openings.tsv, Lichess chess-openings, CC0).

Positions are keyed by the first four FEN fields so transpositions match.
"""
from __future__ import annotations

from dataclasses import dataclass

from ..core.board import Board
from ..core.san import move_from_san


@dataclass(frozen=True)
class Opening:
    eco: str
    name: str


class OpeningBook:
    def __init__(self) -> None:
        self._positions: set = set()
        self._names: dict = {}

    @staticmethod
    def parse(tsv: str) -> "OpeningBook":
        """Parses the TSV (`eco<TAB>name<TAB>pgn`, first line is a header)."""
        book = OpeningBook()
        # Lines share long prefixes, so the position after each move sequence is worked out only once
        # (None marks a sequence that is not playable).
        after: dict = {(): Board.start()}
        for line in tsv.split("\n")[1:]:
            cols = line.rstrip().split("\t")
            if len(cols) < 3:
                continue
            board = after[()]
            prefix: tuple = ()
            for token in cols[2].split(" "):
                # "1." or "1.e4": keep what follows the dot
                san = token[token.rfind(".") + 1:] if "." in token else token
                if not san:
                    continue
                prefix += (san,)
                if prefix not in after:
                    after[prefix] = book._play(board, san)
                board = after[prefix]
                if board is None:
                    break
            if board is not None:
                book._names[book.key(board)] = Opening(cols[0], cols[1])
        return book

    @staticmethod
    def load(path) -> "OpeningBook":
        with open(path, encoding="utf-8") as f:
            return OpeningBook.parse(f.read())

    def _play(self, board: Board, san: str) -> "Board | None":
        """The position after `san` is played from `board` (which is left unchanged), or None if it is illegal."""
        m = move_from_san(board, san)
        if m is None:
            return None
        nxt = board.copy()
        nxt.apply_move(m)
        self._positions.add(self.key(nxt))
        return nxt

    @staticmethod
    def key(b: Board) -> str:
        """Placement, side, castling and en passant of `b`."""
        return " ".join(b.to_fen().split(" ")[:4])

    @property
    def size(self) -> int:
        return len(self._names)

    def contains(self, b: Board) -> bool:
        return self.key(b) in self._positions

    def name_of(self, b: Board) -> "Opening | None":
        """The opening that ends exactly at `b`, if any."""
        return self._names.get(self.key(b))
