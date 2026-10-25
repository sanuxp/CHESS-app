"""Game interaction logic: selection, dragging, promotion, move animation, undo. No drawing and no pygame, so it
can be driven by tests. Times are passed in (seconds) to keep it deterministic."""
from __future__ import annotations

import math
from dataclasses import dataclass

from ..core.game import Game, GameStatus
from ..core.pgn import PgnHeaders, to_pgn
from ..core.types import (
    BLACK, FLAG_CASTLE_KING, FLAG_CASTLE_QUEEN, NO_SQUARE, NONE, WHITE, Move, make_square, piece_color, rank_of,
)

ANIM_SECONDS = 0.220
DRAG_THRESHOLD = 6  # pixels
TOAST_VISIBLE, TOAST_FADE = 3.0, 1.0


def smoothstep(t: float) -> float:
    t = min(1.0, max(0.0, t))
    return t * t * (3 - 2 * t)


@dataclass
class Anim:
    start: float
    slides: list  # (piece, from_square, to_square): pieces in flight
    hide: set  # squares whose own piece must not be drawn while the animation runs
    fades: list  # (piece, square, fading_in)
    reverse: bool = False  # an undo animation

    def progress(self, now: float) -> float:
        return smoothstep((now - self.start) / ANIM_SECONDS)

    def done(self, now: float) -> bool:
        return now - self.start >= ANIM_SECONDS


@dataclass
class Toast:
    text: str
    start: float


class GameController:
    def __init__(self, game: "Game | None" = None, animations: bool = True):
        self.game = game or Game()
        self.animations = animations
        self.selected = NO_SQUARE
        self.promotion: "list | None" = None  # the four promotion moves waiting for the player's choice
        self.flip_mode = False
        self.anim: "Anim | None" = None
        self.toast: "Toast | None" = None
        # dragging
        self._press_sq = NO_SQUARE
        self._press_pos = (0, 0)
        self._press_was_selected = False
        self.dragging = False
        self.drag_pos = (0, 0)

    # --- state for drawing -------------------------------------------------------------------------------

    @property
    def targets(self) -> list:
        """Legal moves of the selected piece."""
        return self.game.legal_moves_from(self.selected) if self.selected != NO_SQUARE else []

    @property
    def flipped(self) -> bool:
        """True when the board is drawn from Black's side."""
        return self.flip_mode and self.game.board.side_to_move == BLACK and not self.game.status.is_over

    @property
    def busy(self) -> bool:
        return self.anim is not None

    @property
    def drag_piece_square(self) -> int:
        return self._press_sq if self.dragging else NO_SQUARE

    def update(self, now: float) -> None:
        if self.anim is not None and self.anim.done(now):
            self.anim = None

    def toast_alpha(self, now: float) -> float:
        if self.toast is None:
            return 0.0
        age = now - self.toast.start
        if age <= TOAST_VISIBLE:
            return 1.0
        return max(0.0, 1.0 - (age - TOAST_VISIBLE) / TOAST_FADE)

    def show_toast(self, text: str, now: float) -> None:
        self.toast = Toast(text, now)

    def _own_piece(self, sq: int) -> bool:
        p = self.game.board.piece_at(sq)
        return p != NONE and piece_color(p) == self.game.board.side_to_move

    # --- input -------------------------------------------------------------------------------------------

    def press(self, sq: int, pos=(0, 0)) -> None:
        """Pointer down on a square (NO_SQUARE for outside the board)."""
        if self.busy or self.promotion is not None or self.game.status.is_over:
            return
        self._press_sq, self._press_pos, self.dragging = sq, pos, False
        self._press_was_selected = False
        if sq != NO_SQUARE and self._own_piece(sq):
            self._press_was_selected = self.selected == sq
            self.selected = sq

    def move_pointer(self, pos) -> None:
        if self._press_sq == NO_SQUARE or not self._own_piece(self._press_sq) or self.busy:
            return
        if not self.dragging and math.dist(pos, self._press_pos) > DRAG_THRESHOLD:
            self.dragging = True
        self.drag_pos = pos

    def release(self, sq: int, now: float) -> None:
        """Pointer up on a square (NO_SQUARE for outside the board)."""
        pressed, was_selected, dragging = self._press_sq, self._press_was_selected, self.dragging
        self._press_sq, self.dragging = NO_SQUARE, False
        if self.busy or self.promotion is not None or self.game.status.is_over:
            return
        if dragging:
            self._drop(pressed, sq, now)
        elif pressed != NO_SQUARE and pressed == sq and self._own_piece(sq):
            if was_selected:
                self.selected = NO_SQUARE  # clicking the selected piece again deselects it
        else:
            self._choose_target(sq, now)

    def click(self, sq: int, now: float) -> None:
        """A click without dragging: press and release on the same square."""
        self.press(sq)
        self.release(sq, now)

    def _drop(self, frm: int, to: int, now: float) -> None:
        moves = [m for m in self.game.legal_moves_from(frm) if m.to_sq == to]
        if moves:
            self._start(moves, now, animate=False)  # the piece is already there
        # dropped elsewhere: the piece returns, the selection stays

    def _choose_target(self, sq: int, now: float) -> None:
        if self.selected != NO_SQUARE and sq != NO_SQUARE:
            moves = [m for m in self.targets if m.to_sq == sq]
            if moves:
                self._start(moves, now, animate=self.animations)
                return
        if sq != NO_SQUARE and self._own_piece(sq):
            self.selected = sq
        else:
            self.selected = NO_SQUARE

    def _start(self, moves: list, now: float, animate: bool) -> None:
        if len(moves) > 1:  # promotion: one move per piece to choose from
            self.promotion = moves
            return
        self.play(moves[0], now, animate)

    def choose_promotion(self, piece_type: int, now: float) -> bool:
        if self.promotion is None:
            return False
        for m in self.promotion:
            if m.promo == piece_type:
                self.promotion = None
                self.play(m, now, animate=self.animations)
                return True
        return False

    def cancel(self) -> None:
        """Esc: cancels the promotion picker, then the selection."""
        if self.promotion is not None:
            self.promotion = None
        else:
            self.selected = NO_SQUARE

    # --- playing -----------------------------------------------------------------------------------------

    def play(self, m: Move, now: float, animate: bool = True) -> bool:
        board = self.game.board
        moving = board.piece_at(m.from_sq)
        slides = [(moving, m.from_sq, m.to_sq)]
        hide = {m.to_sq}
        fades = []
        if m.is_en_passant:
            cap_sq = m.to_sq + (-8 if piece_color(moving) == WHITE else 8)
            fades.append((board.piece_at(cap_sq), cap_sq, False))
        elif m.is_capture:
            fades.append((board.piece_at(m.to_sq), m.to_sq, False))
        rook = self._castle_rook(m)
        if rook is not None:
            slides.append((board.piece_at(rook[0]), rook[0], rook[1]))
            hide.add(rook[1])
        if not self.game.play_move(m):
            return False
        self.selected = NO_SQUARE
        self.promotion = None
        self.anim = Anim(now, slides, hide, fades) if animate else None
        return True

    @staticmethod
    def _castle_rook(m: Move):
        rank = rank_of(m.from_sq)
        if m.flags & FLAG_CASTLE_KING:
            return make_square(7, rank), make_square(5, rank)
        if m.flags & FLAG_CASTLE_QUEEN:
            return make_square(0, rank), make_square(3, rank)
        return None

    def undo(self, now: float) -> bool:
        if self.busy or not self.game.history:
            return False
        entry = self.game.history[-1]
        m = entry.move
        before = entry.before
        moving = before.piece_at(m.from_sq)
        slides = [(moving, m.to_sq, m.from_sq)]
        hide = {m.from_sq}
        fades = []
        if m.is_en_passant:
            cap_sq = m.to_sq + (-8 if piece_color(moving) == WHITE else 8)
            fades.append((before.piece_at(cap_sq), cap_sq, True))
        elif m.is_capture:
            fades.append((before.piece_at(m.to_sq), m.to_sq, True))
        rook = self._castle_rook(m)
        if rook is not None:
            slides.append((before.piece_at(rook[0]), rook[1], rook[0]))
            hide.add(rook[0])
        self.game.undo()
        self.selected = NO_SQUARE
        self.promotion = None
        self.anim = Anim(now, slides, hide, fades, reverse=True) if self.animations else None
        return True

    def new_game(self) -> None:
        self.game = Game()
        self.selected = NO_SQUARE
        self.promotion = None
        self.anim = None
        self.dragging = False

    def load_game(self, game: Game) -> None:
        self.game = game
        self.selected = NO_SQUARE
        self.promotion = None
        self.anim = None

    # --- text --------------------------------------------------------------------------------------------

    def fen(self) -> str:
        return self.game.board.to_fen()

    def pgn(self, headers: "PgnHeaders | None" = None, annotations=None) -> str:
        return to_pgn(self.game, headers, annotations)


def status_headline(g: Game) -> str:
    """"WHITE TO MOVE" while playing, "GAME OVER" once finished."""
    if g.status.is_over:
        return "GAME OVER"
    return "WHITE TO MOVE" if g.board.side_to_move == WHITE else "BLACK TO MOVE"


def status_detail(g: Game) -> str:
    """Extra line: "CHECK!", "CHECKMATE - WHITE WINS", the draw reason, or empty."""
    s = g.status
    if s == GameStatus.CHECK:
        return "CHECK!"
    if s == GameStatus.CHECKMATE:
        return "CHECKMATE - BLACK WINS" if g.board.side_to_move == WHITE else "CHECKMATE - WHITE WINS"
    return {
        GameStatus.STALEMATE: "STALEMATE - DRAW",
        GameStatus.DRAW_FIFTY_MOVE: "DRAW BY 50 MOVE RULE",
        GameStatus.DRAW_REPETITION: "DRAW BY REPETITION",
        GameStatus.DRAW_INSUFFICIENT: "DRAW - INSUFFICIENT MATERIAL",
    }.get(s, "")
