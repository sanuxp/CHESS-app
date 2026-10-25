"""Draws the board: squares, highlights, coordinates, pieces (still, sliding, fading, dragged), move hints and the
promotion picker. Works on any square rect so the review screen can reuse it."""
from __future__ import annotations

import pygame

from ..core.board import Board
from ..core.movegen import is_in_check
from ..core.types import BISHOP, KNIGHT, NO_SQUARE, QUEEN, ROOK, file_of, make_piece, rank_of
from . import theme
from .gfx import circle_alpha, draw_text, fill_alpha
from .pieces import draw_piece
from .theme import Settings

PROMOTION_ORDER = (QUEEN, ROOK, BISHOP, KNIGHT)


class BoardView:
    def __init__(self, rect, flipped: bool = False):
        self.rect = pygame.Rect(rect)
        self.flipped = flipped

    @property
    def cell(self) -> int:
        return self.rect.width // 8

    @property
    def origin(self) -> tuple:
        """Top-left of the 8x8 grid, centred in the rect when it is not a multiple of 8."""
        c = self.cell
        return self.rect.x + (self.rect.width - 8 * c) // 2, self.rect.y + (self.rect.height - 8 * c) // 2

    def square_rect(self, sq: int) -> pygame.Rect:
        c = self.cell
        ox, oy = self.origin
        f, r = file_of(sq), rank_of(sq)
        col = 7 - f if self.flipped else f
        row = r if self.flipped else 7 - r
        return pygame.Rect(ox + col * c, oy + row * c, c, c)

    def square_at(self, pos) -> int:
        """The square under a pixel position, or NO_SQUARE."""
        c = self.cell
        ox, oy = self.origin
        col, row = (pos[0] - ox) // c, (pos[1] - oy) // c
        if not (0 <= col < 8 and 0 <= row < 8) or pos[0] < ox or pos[1] < oy:
            return NO_SQUARE
        f = 7 - col if self.flipped else col
        r = row if self.flipped else 7 - row
        return r * 8 + f

    def center(self, sq: int) -> tuple:
        return self.square_rect(sq).center

    # --- drawing -----------------------------------------------------------------------------------------

    def draw(self, surf, board: Board, settings: Settings, last_move=None, selected: int = NO_SQUARE,
             targets=(), anim=None, now: float = 0.0, drag_square: int = NO_SQUARE, drag_pos=(0, 0)) -> None:
        """`last_move` is a (from, to) pair; `targets` the legal moves of the selected piece."""
        c = self.cell
        bt, ps = settings.board, settings.pieces
        for sq in range(64):
            r = self.square_rect(sq)
            pygame.draw.rect(surf, bt.light if (file_of(sq) + rank_of(sq)) % 2 else bt.dark, r)
        if last_move is not None:
            for sq in last_move:
                fill_alpha(surf, self.square_rect(sq), theme.LAST_MOVE)
        if selected != NO_SQUARE:
            fill_alpha(surf, self.square_rect(selected), theme.SELECTION)
        if is_in_check(board, board.side_to_move):
            k = board.king_square(board.side_to_move)
            circle_alpha(surf, theme.CHECK_DISC, self.center(k), c * 0.46)

        if settings.coordinates:
            self._coordinates(surf, bt)

        hidden = set(anim.hide) if anim is not None else set()
        if drag_square != NO_SQUARE:
            hidden.add(drag_square)
        for sq in range(64):
            p = board.piece_at(sq)
            if p and sq not in hidden:
                draw_piece(surf, p, self.square_rect(sq), ps)

        if anim is not None:
            t = anim.progress(now)
            for piece, sq, fading_in in anim.fades:
                draw_piece(surf, piece, self.square_rect(sq), ps, t if fading_in else 1 - t)
            for piece, a, b in anim.slides:
                ra, rb = self.square_rect(a), self.square_rect(b)
                moving = ra.copy()
                moving.x = round(ra.x + (rb.x - ra.x) * t)
                moving.y = round(ra.y + (rb.y - ra.y) * t)
                draw_piece(surf, piece, moving, ps)

        if settings.hints and targets:
            for m in targets:
                self._hint(surf, m.to_sq, board.piece_at(m.to_sq) != 0 or m.is_en_passant)

        if drag_square != NO_SQUARE:
            size = round(c * 1.12)
            r = pygame.Rect(0, 0, size, size)
            r.center = drag_pos
            draw_piece(surf, board.piece_at(drag_square), r, ps)

    def _coordinates(self, surf, bt) -> None:
        c = self.cell
        size = max(9, int(c * 0.2))
        bottom_rank = 7 if self.flipped else 0
        left_file = 7 if self.flipped else 0
        for i in range(8):
            r = self.square_rect(bottom_rank * 8 + i)
            light = (i + bottom_rank) % 2 == 1
            draw_text(surf, chr(97 + i), size, bt.dark if light else bt.light,
                      (r.right - c * 0.06, r.bottom - c * 0.04), "bottomright", bold=True)
            r = self.square_rect(i * 8 + left_file)
            light = (left_file + i) % 2 == 1
            draw_text(surf, str(i + 1), size, bt.dark if light else bt.light,
                      (r.x + c * 0.06, r.y + c * 0.04), "topleft", bold=True)

    def _hint(self, surf, sq: int, capture: bool) -> None:
        r = self.square_rect(sq)
        c = self.cell
        if capture:
            circle_alpha(surf, theme.HINT, r.center, c * 0.46, max(2, round(c * 0.08)))
        else:
            circle_alpha(surf, theme.HINT, r.center, c * 0.16)

    # --- overlays ----------------------------------------------------------------------------------------

    def promotion_rects(self, color: int) -> list:
        """(piece, rect) for the four choices of the promotion picker, centred on the board."""
        c = self.cell
        pad = c * 0.15
        width = 4 * c + 5 * pad
        bar = pygame.Rect(0, 0, round(width), round(c + 2 * pad))
        bar.center = self.rect.center
        out = []
        for i, t in enumerate(PROMOTION_ORDER):
            out.append((make_piece(t, color), pygame.Rect(round(bar.x + pad + i * (c + pad)), round(bar.y + pad), c, c)))
        return out

    def draw_promotion(self, surf, color: int, settings: Settings, mouse) -> None:
        fill_alpha(surf, self.rect, (0, 0, 0, 150))
        rects = self.promotion_rects(color)
        pad = self.cell * 0.15
        bar = rects[0][1].union(rects[-1][1]).inflate(2 * pad, 2 * pad)
        pygame.draw.rect(surf, settings.palette.panel, bar, border_radius=10)
        for piece, r in rects:
            if r.collidepoint(mouse):
                fill_alpha(surf, r, (*settings.palette.button_hover, 255))
            draw_piece(surf, piece, r, settings.pieces)

    def draw_banner(self, surf, text: str, settings: Settings) -> None:
        h = max(28, round(self.cell * 0.9))
        band = pygame.Rect(self.rect.x, 0, self.rect.width, h)
        band.centery = self.rect.centery
        fill_alpha(surf, band, (0, 0, 0, 185))
        draw_text(surf, text, round(h * 0.5), (240, 240, 245), band.center, "center", bold=True,
                  max_width=band.width - 16)

