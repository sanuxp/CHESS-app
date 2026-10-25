"""Basic chess types. Squares are ints 0..63 with a1 = 0, h1 = 7, a8 = 56.

A piece is one int: 0 for an empty square, otherwise ``color << 3 | type``.
"""
from __future__ import annotations

WHITE, BLACK = 0, 1
NONE, PAWN, KNIGHT, BISHOP, ROOK, QUEEN, KING = range(7)

NO_SQUARE = -1

FLAG_CAPTURE = 1
FLAG_DOUBLE_PUSH = 2
FLAG_EN_PASSANT = 4
FLAG_CASTLE_KING = 8
FLAG_CASTLE_QUEEN = 16

CASTLE_WHITE_KING = 1
CASTLE_WHITE_QUEEN = 2
CASTLE_BLACK_KING = 4
CASTLE_BLACK_QUEEN = 8


def make_piece(ptype: int, color: int) -> int:
    return (color << 3) | ptype


def piece_type(p: int) -> int:
    return p & 7


def piece_color(p: int) -> int:
    return p >> 3


def file_of(s: int) -> int:
    return s & 7


def rank_of(s: int) -> int:
    return s >> 3


def make_square(file: int, rank: int) -> int:
    return rank * 8 + file


def square_name(s: int) -> str:
    if s < 0 or s > 63:
        return "-"
    return chr(97 + file_of(s)) + chr(49 + rank_of(s))


def square_from_name(name: str) -> int:
    """Returns NO_SQUARE when `name` is not a square like "e4"."""
    if len(name) != 2:
        return NO_SQUARE
    f, r = ord(name[0]) - 97, ord(name[1]) - 49
    if not (0 <= f < 8 and 0 <= r < 8):
        return NO_SQUARE
    return make_square(f, r)


_PROMO_LETTERS = {QUEEN: "q", ROOK: "r", BISHOP: "b", KNIGHT: "n"}
PROMO_FROM_LETTER = {v: k for k, v in _PROMO_LETTERS.items()}


class Move:
    """A move. Two moves are equal when origin, target and promotion match (flags are derived)."""

    __slots__ = ("from_sq", "to_sq", "promo", "flags")

    def __init__(self, from_sq: int, to_sq: int, promo: int = NONE, flags: int = 0):
        self.from_sq = from_sq
        self.to_sq = to_sq
        self.promo = promo
        self.flags = flags

    @property
    def is_capture(self) -> bool:
        return bool(self.flags & FLAG_CAPTURE)

    @property
    def is_en_passant(self) -> bool:
        return bool(self.flags & FLAG_EN_PASSANT)

    @property
    def is_castle(self) -> bool:
        return bool(self.flags & (FLAG_CASTLE_KING | FLAG_CASTLE_QUEEN))

    @property
    def uci(self) -> str:
        """Move written like "e2e4" or "e7e8q"."""
        return square_name(self.from_sq) + square_name(self.to_sq) + _PROMO_LETTERS.get(self.promo, "")

    def __eq__(self, other: object) -> bool:
        return (isinstance(other, Move) and self.from_sq == other.from_sq
                and self.to_sq == other.to_sq and self.promo == other.promo)

    def __hash__(self) -> int:
        return hash((self.from_sq, self.to_sq, self.promo))

    def __repr__(self) -> str:
        return self.uci
