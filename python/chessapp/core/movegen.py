"""Pseudo-legal and legal move generation, attack tests and perft."""
from __future__ import annotations

from .board import Board
from .types import (
    BISHOP, CASTLE_BLACK_KING, CASTLE_BLACK_QUEEN, CASTLE_WHITE_KING, CASTLE_WHITE_QUEEN,
    FLAG_CAPTURE, FLAG_CASTLE_KING, FLAG_CASTLE_QUEEN, FLAG_DOUBLE_PUSH, FLAG_EN_PASSANT, KING, KNIGHT, NO_SQUARE,
    NONE, PAWN, QUEEN, ROOK, WHITE, Move, make_piece, make_square,
)

_KNIGHT = [(1, 2), (2, 1), (2, -1), (1, -2), (-1, -2), (-2, -1), (-2, 1), (-1, 2)]
_KING = [(1, 0), (1, 1), (0, 1), (-1, 1), (-1, 0), (-1, -1), (0, -1), (1, -1)]
_DIAGONALS = [(1, 1), (-1, 1), (-1, -1), (1, -1)]
_ORTHOGONALS = [(1, 0), (-1, 0), (0, 1), (0, -1)]


def _on(f: int, r: int) -> bool:
    return 0 <= f < 8 and 0 <= r < 8


def _steps(deltas):
    return [[make_square((s & 7) + d[0], (s >> 3) + d[1])
             for d in deltas if _on((s & 7) + d[0], (s >> 3) + d[1])] for s in range(64)]


def _rays(dirs):
    table = []
    for s in range(64):
        rays = []
        for df, dr in dirs:
            f, r, ray = (s & 7) + df, (s >> 3) + dr, []
            while _on(f, r):
                ray.append(make_square(f, r))
                f += df
                r += dr
            if ray:
                rays.append(ray)
        table.append(rays)
    return table


_KNIGHT_TO = _steps(_KNIGHT)
_KING_TO = _steps(_KING)
_DIAG_RAYS = _rays(_DIAGONALS)
_ORTHO_RAYS = _rays(_ORTHOGONALS)
# Squares a pawn of the given colour attacks from, to reach a target square.
_PAWN_FROM = [
    [[make_square((s & 7) + df, (s >> 3) - 1) for df in (-1, 1) if _on((s & 7) + df, (s >> 3) - 1)] for s in range(64)],
    [[make_square((s & 7) + df, (s >> 3) + 1) for df in (-1, 1) if _on((s & 7) + df, (s >> 3) + 1)] for s in range(64)],
]


def is_square_attacked(b: Board, sq: int, by: int) -> bool:
    """True if any piece of colour `by` attacks `sq`."""
    board = b.sq
    pawn = make_piece(PAWN, by)
    for s in _PAWN_FROM[by][sq]:
        if board[s] == pawn:
            return True
    knight = make_piece(KNIGHT, by)
    for s in _KNIGHT_TO[sq]:
        if board[s] == knight:
            return True
    king = make_piece(KING, by)
    for s in _KING_TO[sq]:
        if board[s] == king:
            return True
    bishop, queen, rook = make_piece(BISHOP, by), make_piece(QUEEN, by), make_piece(ROOK, by)
    for ray in _DIAG_RAYS[sq]:
        for s in ray:
            p = board[s]
            if p:
                if p == bishop or p == queen:
                    return True
                break
    for ray in _ORTHO_RAYS[sq]:
        for s in ray:
            p = board[s]
            if p:
                if p == rook or p == queen:
                    return True
                break
    return False


def is_in_check(b: Board, color: int) -> bool:
    k = b.king_square(color)
    return k != NO_SQUARE and is_square_attacked(b, k, color ^ 1)


_PROMOTIONS = (QUEEN, ROOK, BISHOP, KNIGHT)


def _pawn(b: Board, frm: int, us: int, out: list) -> None:
    board = b.sq
    file, rank = frm & 7, frm >> 3
    direction = 1 if us == WHITE else -1
    start_rank = 1 if us == WHITE else 6
    last_rank = 7 if us == WHITE else 0
    nxt = rank + direction
    if not 0 <= nxt < 8:
        return
    promotes = nxt == last_rank

    one = nxt * 8 + file
    if board[one] == NONE:
        if promotes:
            out.extend(Move(frm, one, t, 0) for t in _PROMOTIONS)
        else:
            out.append(Move(frm, one, NONE, 0))
            if rank == start_rank:
                two = (rank + 2 * direction) * 8 + file
                if board[two] == NONE:
                    out.append(Move(frm, two, NONE, FLAG_DOUBLE_PUSH))
    for df in (-1, 1):
        f = file + df
        if not 0 <= f < 8:
            continue
        target = nxt * 8 + f
        victim = board[target]
        if victim != NONE and (victim >> 3) != us:
            if promotes:
                out.extend(Move(frm, target, t, FLAG_CAPTURE) for t in _PROMOTIONS)
            else:
                out.append(Move(frm, target, NONE, FLAG_CAPTURE))
        elif victim == NONE and target == b.ep_square:
            out.append(Move(frm, target, NONE, FLAG_CAPTURE | FLAG_EN_PASSANT))


def _step(b: Board, frm: int, us: int, table, out: list) -> None:
    board = b.sq
    for to in table[frm]:
        t = board[to]
        if t == NONE:
            out.append(Move(frm, to, NONE, 0))
        elif (t >> 3) != us:
            out.append(Move(frm, to, NONE, FLAG_CAPTURE))


def _slide(b: Board, frm: int, us: int, rays, out: list) -> None:
    board = b.sq
    for ray in rays[frm]:
        for to in ray:
            t = board[to]
            if t == NONE:
                out.append(Move(frm, to, NONE, 0))
            else:
                if (t >> 3) != us:
                    out.append(Move(frm, to, NONE, FLAG_CAPTURE))
                break


def _castling(b: Board, us: int, out: list) -> None:
    rank = 0 if us == WHITE else 7
    king = make_square(4, rank)
    if b.sq[king] != make_piece(KING, us):
        return
    them = us ^ 1
    king_right = CASTLE_WHITE_KING if us == WHITE else CASTLE_BLACK_KING
    queen_right = CASTLE_WHITE_QUEEN if us == WHITE else CASTLE_BLACK_QUEEN
    if not b.castling & (king_right | queen_right):
        return
    if is_square_attacked(b, king, them):
        return
    rook = make_piece(ROOK, us)

    def empty(file):
        return b.sq[make_square(file, rank)] == NONE

    def safe(file):
        return not is_square_attacked(b, make_square(file, rank), them)

    if (b.castling & king_right and b.sq[make_square(7, rank)] == rook
            and empty(5) and empty(6) and safe(5) and safe(6)):
        out.append(Move(king, make_square(6, rank), NONE, FLAG_CASTLE_KING))
    if (b.castling & queen_right and b.sq[make_square(0, rank)] == rook
            and empty(1) and empty(2) and empty(3) and safe(3) and safe(2)):
        out.append(Move(king, make_square(2, rank), NONE, FLAG_CASTLE_QUEEN))


def generate_pseudo_legal(b: Board) -> list:
    """Moves that obey piece movement but may leave the own king in check."""
    out: list = []
    us = b.side_to_move
    for s in range(64):
        p = b.sq[s]
        if p == NONE or (p >> 3) != us:
            continue
        t = p & 7
        if t == PAWN:
            _pawn(b, s, us, out)
        elif t == KNIGHT:
            _step(b, s, us, _KNIGHT_TO, out)
        elif t == BISHOP:
            _slide(b, s, us, _DIAG_RAYS, out)
        elif t == ROOK:
            _slide(b, s, us, _ORTHO_RAYS, out)
        elif t == QUEEN:
            _slide(b, s, us, _DIAG_RAYS, out)
            _slide(b, s, us, _ORTHO_RAYS, out)
        elif t == KING:
            _step(b, s, us, _KING_TO, out)
    _castling(b, us, out)
    return out


def is_legal(b: Board, m) -> bool:
    """True if the pseudo-legal move `m` does not leave the mover's own king attacked."""
    us = b.side_to_move
    nxt = b.copy()
    nxt.apply_move(m)
    k = m.to_sq if (b.sq[m.from_sq] & 7) == KING else b.king_square(us)
    return not is_square_attacked(nxt, k, us ^ 1)


def generate_legal(b: Board) -> list:
    """Fully legal moves for the side to move. Promotions appear as four separate moves."""
    us = b.side_to_move
    king = b.king_square(us)
    legal = []
    for m in generate_pseudo_legal(b):
        nxt = b.copy()
        nxt.apply_move(m)
        k = m.to_sq if (b.sq[m.from_sq] & 7) == KING else king
        if not is_square_attacked(nxt, k, us ^ 1):
            legal.append(m)
    return legal


def perft(b: Board, depth: int) -> int:
    """Leaf-node count of the legal move tree: the standard move-generation benchmark."""
    if depth == 0:
        return 1
    moves = generate_legal(b)
    if depth == 1:
        return len(moves)
    nodes = 0
    for m in moves:
        nxt = b.copy()
        nxt.apply_move(m)
        nodes += perft(nxt, depth - 1)
    return nodes


