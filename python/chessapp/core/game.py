"""A game in progress: position, history, undo and end-of-game detection."""
from __future__ import annotations

import enum
from dataclasses import dataclass

from .board import Board
from .movegen import generate_legal, is_in_check
from .san import move_from_san, move_to_san
from .types import (
    BISHOP, KING, KNIGHT, NO_SQUARE, NONE, PROMO_FROM_LETTER, WHITE, Move, file_of, rank_of, square_from_name,
)


class GameStatus(enum.Enum):
    # The value is the name used by the shared test data.
    PLAYING = "Playing"
    CHECK = "Check"
    CHECKMATE = "Checkmate"
    STALEMATE = "Stalemate"
    DRAW_FIFTY_MOVE = "DrawFiftyMove"
    DRAW_REPETITION = "DrawRepetition"
    DRAW_INSUFFICIENT = "DrawInsufficient"

    @property
    def is_over(self) -> bool:
        return self not in (GameStatus.PLAYING, GameStatus.CHECK)

    @property
    def data_name(self) -> str:
        return self.value


@dataclass(frozen=True)
class HistoryEntry:
    before: Board  # position before the move
    move: Move
    san: str


def _insufficient_material(b: Board) -> bool:
    knights = bishops = light = dark = 0
    for s in range(64):
        t = b.sq[s] & 7
        if t == NONE or t == KING:
            continue
        if t == KNIGHT:
            knights += 1
        elif t == BISHOP:
            bishops += 1
            if (file_of(s) + rank_of(s)) % 2 == 0:
                dark += 1
            else:
                light += 1
        else:
            return False  # pawn, rook or queen on the board
    if knights + bishops <= 1:
        return True  # K v K, K+minor v K
    if knights == 0:
        return light == 0 or dark == 0  # only same-coloured bishops
    return False


def _position_key(b: Board) -> str:
    """Placement, side, castling rights, and en passant only if a capture onto it is legal."""
    copy = b.copy()
    if copy.ep_square != NO_SQUARE and not any(m.is_en_passant for m in generate_legal(copy)):
        copy.ep_square = NO_SQUARE
    copy.halfmove_clock = 0
    copy.fullmove_number = 1
    return copy.to_fen()


class Game:
    def __init__(self, start: "Board | None" = None):
        self.initial: Board = start if start is not None else Board.start()
        self._board: Board = self.initial.copy()
        self._history: list[HistoryEntry] = []
        self._keys: list[str] = [_position_key(self._board)]
        self._legal: list[Move] = []
        self._status = GameStatus.PLAYING
        self._refresh()

    @staticmethod
    def from_fen(fen: str) -> "Game | None":
        b = Board.from_fen(fen)
        return None if b is None else Game(b)

    @property
    def board(self) -> Board:
        return self._board

    @property
    def history(self) -> list:
        return list(self._history)

    @property
    def move_count(self) -> int:
        return len(self._history)

    @property
    def legal_moves(self) -> list:
        return self._legal

    @property
    def status(self) -> GameStatus:
        return self._status

    @property
    def in_check(self) -> bool:
        return is_in_check(self._board, self._board.side_to_move)

    @property
    def last_move(self) -> "Move | None":
        return self._history[-1].move if self._history else None

    def legal_moves_from(self, frm: int) -> list:
        return [m for m in self._legal if m.from_sq == frm]

    def make_move(self, frm: int, to: int, promo: int = NONE) -> bool:
        """Plays a legal move; returns False (state unchanged) if it is not legal here."""
        for m in self._legal:
            if m.from_sq == frm and m.to_sq == to and m.promo == promo:
                self._history.append(HistoryEntry(self._board.copy(), m, move_to_san(self._board, m)))
                self._board.apply_move(m)
                self._keys.append(_position_key(self._board))
                self._refresh()
                return True
        return False

    def play_move(self, m: Move) -> bool:
        return self.make_move(m.from_sq, m.to_sq, m.promo)

    def make_move_uci(self, uci: str) -> bool:
        """Plays a move written like "e2e4" or "e7e8q"."""
        if len(uci) < 4 or len(uci) > 5:
            return False
        frm, to = square_from_name(uci[0:2]), square_from_name(uci[2:4])
        if frm == NO_SQUARE or to == NO_SQUARE:
            return False
        promo = NONE
        if len(uci) == 5:
            promo = PROMO_FROM_LETTER.get(uci[4], NONE)
            if promo == NONE:
                return False
        return self.make_move(frm, to, promo)

    def make_move_san(self, san: str) -> bool:
        """Plays a move in standard algebraic notation such as "Nf3" or "O-O"."""
        m = move_from_san(self._board, san)
        return m is not None and self.play_move(m)

    def undo(self) -> bool:
        if not self._history:
            return False
        self._board = self._history.pop().before
        self._keys.pop()
        self._refresh()
        return True

    @property
    def result_token(self) -> str:
        """"1-0", "0-1", "1/2-1/2", or "*" while the game is still going."""
        s = self._status
        if s == GameStatus.CHECKMATE:
            return "0-1" if self._board.side_to_move == WHITE else "1-0"
        if s in (GameStatus.STALEMATE, GameStatus.DRAW_FIFTY_MOVE, GameStatus.DRAW_REPETITION,
                 GameStatus.DRAW_INSUFFICIENT):
            return "1/2-1/2"
        return "*"

    def _refresh(self) -> None:
        self._legal = generate_legal(self._board)
        check = is_in_check(self._board, self._board.side_to_move)
        if not self._legal:
            self._status = GameStatus.CHECKMATE if check else GameStatus.STALEMATE
        elif _insufficient_material(self._board):
            self._status = GameStatus.DRAW_INSUFFICIENT
        elif self._keys.count(self._keys[-1]) >= 3:
            self._status = GameStatus.DRAW_REPETITION
        elif self._board.halfmove_clock >= 100:
            self._status = GameStatus.DRAW_FIFTY_MOVE
        else:
            self._status = GameStatus.CHECK if check else GameStatus.PLAYING
