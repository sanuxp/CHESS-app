"""A position: piece placement plus the state FEN records. No history."""
from __future__ import annotations

import re

from .types import (
    BISHOP, BLACK, CASTLE_BLACK_KING, CASTLE_BLACK_QUEEN, CASTLE_WHITE_KING, CASTLE_WHITE_QUEEN,
    FLAG_CASTLE_KING, FLAG_CASTLE_QUEEN, FLAG_DOUBLE_PUSH, KING, KNIGHT, NO_SQUARE, NONE, PAWN, QUEEN, ROOK, WHITE,
    Move, make_piece, make_square, piece_color, piece_type, rank_of, square_from_name, square_name,
)

START_FEN = "rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1"

_LETTERS = {"p": PAWN, "n": KNIGHT, "b": BISHOP, "r": ROOK, "q": QUEEN, "k": KING}
_CHARS = {v: k for k, v in _LETTERS.items()}

# A right is lost when its king or rook leaves home, or the rook is captured there.
_STRIP = {
    0: ~CASTLE_WHITE_QUEEN, 7: ~CASTLE_WHITE_KING, 56: ~CASTLE_BLACK_QUEEN, 63: ~CASTLE_BLACK_KING,
    4: ~(CASTLE_WHITE_KING | CASTLE_WHITE_QUEEN), 60: ~(CASTLE_BLACK_KING | CASTLE_BLACK_QUEEN),
}


class Board:
    __slots__ = ("sq", "side_to_move", "castling", "ep_square", "halfmove_clock", "fullmove_number")

    def __init__(self, sq, side_to_move, castling, ep_square, halfmove_clock, fullmove_number):
        self.sq = sq
        self.side_to_move = side_to_move
        self.castling = castling
        self.ep_square = ep_square
        self.halfmove_clock = halfmove_clock
        self.fullmove_number = fullmove_number

    @staticmethod
    def start() -> "Board":
        return Board.from_fen(START_FEN)  # type: ignore[return-value]

    def copy(self) -> "Board":
        return Board(self.sq[:], self.side_to_move, self.castling, self.ep_square,
                     self.halfmove_clock, self.fullmove_number)

    def piece_at(self, s: int) -> int:
        return self.sq[s]

    def king_square(self, color: int) -> int:
        try:
            return self.sq.index(make_piece(KING, color))
        except ValueError:
            return NO_SQUARE

    @staticmethod
    def from_fen(fen: str) -> "Board | None":
        """Parses a FEN string; returns None if it is malformed."""
        parts = fen.split()
        if len(parts) < 2:
            return None
        placement, side = parts[0], parts[1]
        castle_field = parts[2] if len(parts) > 2 else "-"
        ep_field = parts[3] if len(parts) > 3 else "-"
        half, full = 0, 1
        if len(parts) > 4:
            if not re.fullmatch(r"-?\d+", parts[4]):
                return None
            half = int(parts[4])
            if len(parts) > 5:
                full = int(parts[5]) if re.fullmatch(r"-?\d+", parts[5]) else 1
        if half < 0 or full < 1:
            return None

        sq = [NONE] * 64
        file, rank = 0, 7
        kings = {WHITE: 0, BLACK: 0}
        for ch in placement:
            if ch == "/":
                if file != 8 or rank == 0:
                    return None
                rank -= 1
                file = 0
            elif "1" <= ch <= "8":
                file += int(ch)
                if file > 8:
                    return None
            else:
                t = _LETTERS.get(ch.lower())
                if t is None or file >= 8:
                    return None
                color = WHITE if ch.isupper() else BLACK
                if t == PAWN and rank in (0, 7):
                    return None
                if t == KING:
                    kings[color] += 1
                sq[make_square(file, rank)] = make_piece(t, color)
                file += 1
        if rank != 0 or file != 8 or kings[WHITE] != 1 or kings[BLACK] != 1:
            return None

        if side == "w":
            stm = WHITE
        elif side == "b":
            stm = BLACK
        else:
            return None

        castling = 0
        if castle_field != "-":
            bits = {"K": CASTLE_WHITE_KING, "Q": CASTLE_WHITE_QUEEN, "k": CASTLE_BLACK_KING, "q": CASTLE_BLACK_QUEEN}
            for c in castle_field:
                if c not in bits:
                    return None
                castling |= bits[c]

        ep = NO_SQUARE
        if ep_field != "-":
            ep = square_from_name(ep_field)
            if ep == NO_SQUARE:
                return None
        return Board(sq, stm, castling, ep, half, full)

    def to_fen(self) -> str:
        rows = []
        for rank in range(7, -1, -1):
            row, empty = [], 0
            for file in range(8):
                p = self.sq[make_square(file, rank)]
                if p == NONE:
                    empty += 1
                    continue
                if empty:
                    row.append(str(empty))
                    empty = 0
                c = _CHARS[piece_type(p)]
                row.append(c.upper() if piece_color(p) == WHITE else c)
            if empty:
                row.append(str(empty))
            rows.append("".join(row))
        rights = ""
        for bit, ch in ((CASTLE_WHITE_KING, "K"), (CASTLE_WHITE_QUEEN, "Q"), (CASTLE_BLACK_KING, "k"),
                        (CASTLE_BLACK_QUEEN, "q")):
            if self.castling & bit:
                rights += ch
        ep = square_name(self.ep_square) if self.ep_square != NO_SQUARE else "-"
        return (f"{'/'.join(rows)} {'w' if self.side_to_move == WHITE else 'b'} {rights or '-'} "
                f"{ep} {self.halfmove_clock} {self.fullmove_number}")

    def apply_move(self, m: Move) -> None:
        """Applies a move that is known to be legal (flags as produced by move generation)."""
        sq = self.sq
        moving = sq[m.from_sq]
        resets_clock = piece_type(moving) == PAWN or m.is_capture

        if m.is_en_passant:
            sq[m.to_sq + (-8 if piece_color(moving) == WHITE else 8)] = NONE
        sq[m.to_sq] = make_piece(m.promo, piece_color(moving)) if m.promo != NONE else moving
        sq[m.from_sq] = NONE

        if m.flags & FLAG_CASTLE_KING:
            rank = rank_of(m.from_sq)
            sq[make_square(5, rank)] = sq[make_square(7, rank)]
            sq[make_square(7, rank)] = NONE
        elif m.flags & FLAG_CASTLE_QUEEN:
            rank = rank_of(m.from_sq)
            sq[make_square(3, rank)] = sq[make_square(0, rank)]
            sq[make_square(0, rank)] = NONE

        mask = _STRIP.get(m.from_sq)
        if mask is not None:
            self.castling &= mask
        mask = _STRIP.get(m.to_sq)
        if mask is not None:
            self.castling &= mask

        self.ep_square = (m.from_sq + m.to_sq) // 2 if m.flags & FLAG_DOUBLE_PUSH else NO_SQUARE
        self.halfmove_clock = 0 if resets_clock else self.halfmove_clock + 1
        if self.side_to_move == BLACK:
            self.fullmove_number += 1
        self.side_to_move ^= 1
