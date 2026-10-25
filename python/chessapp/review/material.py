"""Material counting and the sacrifice measure of docs/REVIEW.md section 4."""
from __future__ import annotations

from ..core.board import Board
from ..core.movegen import generate_legal
from ..core.types import BISHOP, KNIGHT, NONE, PAWN, PROMO_FROM_LETTER, QUEEN, ROOK, square_from_name

_VALUE = {PAWN: 1, KNIGHT: 3, BISHOP: 3, ROOK: 5, QUEEN: 9}


def _material(b: Board, color: int) -> int:
    return sum(_VALUE.get(p & 7, 0) for p in b.sq if p and (p >> 3) == color)


def apply_uci(board: Board, uci: str) -> bool:
    """Applies a UCI move if it is legal; returns False otherwise."""
    if len(uci) < 4:
        return False
    frm, to = square_from_name(uci[0:2]), square_from_name(uci[2:4])
    promo = PROMO_FROM_LETTER.get(uci[4], NONE) if len(uci) == 5 else NONE
    for m in generate_legal(board):
        if m.from_sq == frm and m.to_sq == to and m.promo == promo:
            board.apply_move(m)
            return True
    return False


def sacrificed_value(before: Board, played: str, pv: list) -> float:
    """Material in pawns the mover gives up by playing `played` from `before`, judged over the move and the
    first three moves of the engine's variation `pv` that follows it. Trades score 0."""
    mover = before.side_to_move
    other = mover ^ 1
    mover_before, other_before = _material(before, mover), _material(before, other)

    b = before.copy()
    if not apply_uci(b, played):
        return 0
    for m in pv[:3]:
        if not apply_uci(b, m):
            break
    delta = (_material(b, mover) - mover_before) - (_material(b, other) - other_before)
    return float(-delta) if delta < 0 else 0.0
