"""Standard algebraic notation: writing a move as SAN and finding the move a SAN string denotes."""
from __future__ import annotations

import re

from .board import Board
from .movegen import generate_legal, generate_pseudo_legal, is_in_check, is_legal
from .types import (
    BISHOP, FLAG_CASTLE_KING, FLAG_CASTLE_QUEEN, KING, KNIGHT, NONE, PAWN, QUEEN, ROOK, Move, file_of, rank_of,
    square_from_name, square_name,
)

_LETTER = {KNIGHT: "N", BISHOP: "B", ROOK: "R", QUEEN: "Q", KING: "K"}
_TYPES = {v: k for k, v in _LETTER.items()}
_SAN_PATTERN = re.compile(r"^([NBRQK])?([a-h])?([1-8])?x?([a-h][1-8])(?:=?([NBRQ]))?$")


def _strip(s: str) -> str:
    return s.rstrip("+#!?").replace("0", "O")  # accept 0-0 as castling


def _file_char(sq: int) -> str:
    return chr(97 + file_of(sq))


def move_to_san(before: Board, m: Move) -> str:
    """Standard algebraic notation of `m` played from `before`, including the +/# suffix."""
    p = before.piece_at(m.from_sq) & 7
    s = []

    if m.flags & FLAG_CASTLE_KING:
        s.append("O-O")
    elif m.flags & FLAG_CASTLE_QUEEN:
        s.append("O-O-O")
    else:
        if p == PAWN:
            if m.is_capture:
                s.append(_file_char(m.from_sq))
        else:
            s.append(_LETTER[p])
            ambiguous = same_file = same_rank = False
            for other in generate_legal(before):
                if other.to_sq != m.to_sq or other.from_sq == m.from_sq:
                    continue
                if before.piece_at(other.from_sq) & 7 != p:
                    continue
                ambiguous = True
                if file_of(other.from_sq) == file_of(m.from_sq):
                    same_file = True
                if rank_of(other.from_sq) == rank_of(m.from_sq):
                    same_rank = True
            if ambiguous:
                if not same_file:
                    s.append(_file_char(m.from_sq))
                elif not same_rank:
                    s.append(chr(49 + rank_of(m.from_sq)))
                else:
                    s.append(square_name(m.from_sq))
        if m.is_capture:
            s.append("x")
        s.append(square_name(m.to_sq))
        if m.promo != NONE:
            s.append("=" + _LETTER[m.promo])

    after = before.copy()
    after.apply_move(m)
    if is_in_check(after, after.side_to_move):
        s.append("#" if not generate_legal(after) else "+")
    return "".join(s)


def move_from_san(board: Board, san: str) -> "Move | None":
    """The legal move in `board` that `san` denotes (check/mate marks and !? are ignored).

    Returns None when the text matches no legal move, or more than one.
    """
    text = _strip(san)
    # Pseudo-legal moves are cheap; only the few that match the text are checked for legality.
    candidates = generate_pseudo_legal(board)

    if text in ("O-O", "O-O-O"):
        flag = FLAG_CASTLE_KING if text == "O-O" else FLAG_CASTLE_QUEEN
        for m in candidates:
            if m.flags & flag:
                return m  # castling moves are only generated when legal
        return None

    match = _SAN_PATTERN.match(text)
    if match is None:
        return None
    piece_letter, file_hint, rank_hint, target, promo_letter = match.groups()
    to = square_from_name(target)
    ptype = PAWN if piece_letter is None else _TYPES[piece_letter]
    promo = NONE if promo_letter is None else _TYPES[promo_letter]

    found = None
    for m in candidates:
        if m.to_sq != to or m.promo != promo or board.piece_at(m.from_sq) & 7 != ptype:
            continue
        if file_hint is not None and file_of(m.from_sq) != ord(file_hint) - 97:
            continue
        if rank_hint is not None and rank_of(m.from_sq) != ord(rank_hint) - 49:
            continue
        if not is_legal(board, m):
            continue
        if found is not None:
            return None  # ambiguous
        found = m
    return found
