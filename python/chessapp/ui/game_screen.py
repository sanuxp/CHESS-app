"""The main screen: board on one side, control panel on the other (docs/UI_SPEC.md)."""
from __future__ import annotations

import os

import pygame

from ..core.types import WHITE
from .board_view import BoardView
from .controller import status_detail, status_headline
from .files import ask_save_pgn, copy_to_clipboard, default_pgn_name, write_text
from .gfx import Button, aa_circle, draw_text, fit_size, hit, rounded, text_surface


def game_layout(w: int, h: int) -> tuple:
    """(board rect, panel rect, wide): wide windows put the board left and a 26 % panel right, tall ones stack."""
    m = max(8, round(16 * min(w, h) / 720))
    if w >= 1.1 * h:
        panel_w = round(w * 0.26)
        side = max(64, min(h - 2 * m, w - panel_w - 3 * m))
        board = pygame.Rect(m, (h - side) // 2, side, side)
        panel = pygame.Rect(board.right + m, m, max(40, w - board.right - 2 * m), max(40, h - 2 * m))
        return board, panel, True
    side = max(64, min(w - 2 * m, round(h * 0.64)))
    board = pygame.Rect((w - side) // 2, m, side, side)
    panel = pygame.Rect(m, board.bottom + m, max(40, w - 2 * m), max(40, h - board.bottom - 2 * m))
    return board, panel, False


class GameScreen:
    def __init__(self, app):
        self.app = app

    # --- layout ------------------------------------------------------------------------------------------

    def layout(self, surf) -> tuple:
        w, h = surf.get_size()
        board, panel, wide = game_layout(w, h)
        c = self.app.controller
        return BoardView(board, c.flipped), panel, wide

    def buttons(self, panel: pygame.Rect, wide: bool, top: int) -> list:
        c = self.app.controller
        specs = [("new", "New Game", True), ("undo", "Undo", bool(c.game.history) and not c.busy),
                 ("save", "Save PGN", True), ("fen", "Copy FEN", True),
                 ("flip", f"Flip: {'On' if c.flip_mode else 'Off'}", True), ("review", "Review", True),
                 ("themes", "Themes", True)]
        gap = max(6, panel.height // 70)
        out = []
        if wide:
            toast_room = max(24, panel.height // 14)
            avail = panel.bottom - top - toast_room
            bh = max(26, min(54, (avail - gap * (len(specs) - 1)) // len(specs)))
            for i, (action, label, enabled) in enumerate(specs):
                out.append(Button(action, label, (panel.x, top + i * (bh + gap), panel.width, bh), enabled))
        else:
            cols = 4
            rows = (len(specs) + cols - 1) // cols
            toast_room = 22
            avail = panel.bottom - top - toast_room
            bh = max(26, min(54, (avail - gap * (rows - 1)) // rows))
            bw = (panel.width - gap * (cols - 1)) // cols
            for i, (action, label, enabled) in enumerate(specs):
                r, col = divmod(i, cols)
                out.append(Button(action, label, (panel.x + col * (bw + gap), top + r * (bh + gap), bw, bh), enabled))
        return out

    @staticmethod
    def geometry(panel: pygame.Rect, wide: bool) -> dict:
        """Where the panel's parts go; shared by drawing and click handling so they always agree."""
        pad = max(8, panel.width // 18)
        inner = panel.inflate(-2 * pad, -2 * pad)
        y = inner.y
        title = max(18, inner.width // 5) if wide else 0
        if wide:
            y += round(title * 1.5)
        disc_r = max(8, min(16, inner.width // 14))
        size = max(13, min(22, inner.width // 11))
        return {"inner": inner, "title": title, "status_y": y, "disc_r": disc_r, "size": size,
                "top": y + 2 * disc_r + max(26, size + 14)}

    # --- events ------------------------------------------------------------------------------------------

    def handle_event(self, e) -> None:
        app, c = self.app, self.app.controller
        now = app.now()
        view, panel, wide = self.layout(app.surface)
        if e.type == pygame.KEYDOWN:
            if e.key == pygame.K_ESCAPE:
                c.cancel()
            elif e.key == pygame.K_u or (e.key == pygame.K_z and e.mod & pygame.KMOD_CTRL):
                c.undo(now)
        elif e.type == pygame.MOUSEBUTTONDOWN and e.button == 1:
            if c.promotion is not None:
                color = c.game.board.side_to_move
                for piece, r in view.promotion_rects(color):
                    if r.collidepoint(e.pos):
                        c.choose_promotion(piece & 7, now)
                        return
                c.cancel()
                return
            geo = self.geometry(panel, wide)
            action = hit(self.buttons(geo["inner"], wide, geo["top"]), e.pos)
            if action:
                self.run(action, now)
            else:
                c.press(view.square_at(e.pos), e.pos)
        elif e.type == pygame.MOUSEMOTION:
            c.move_pointer(e.pos)
        elif e.type == pygame.MOUSEBUTTONUP and e.button == 1:
            c.release(view.square_at(e.pos), now)

    def run(self, action: str, now: float) -> None:
        app, c = self.app, self.app.controller
        if action == "new":
            c.new_game()
        elif action == "undo":
            c.undo(now)
        elif action == "flip":
            c.flip_mode = not c.flip_mode
        elif action == "themes":
            app.open_themes()
        elif action == "review":
            app.open_review()
        elif action == "save":
            path = ask_save_pgn(default_pgn_name())
            if path:
                if write_text(path, c.pgn()):
                    c.show_toast(f"SAVED {os.path.basename(path)}", now)
                else:
                    c.show_toast("COULD NOT SAVE THE FILE", now)
        elif action == "fen":
            fen = c.fen()
            c.show_toast("FEN COPIED TO CLIPBOARD" if copy_to_clipboard(fen) else f"FEN: {fen}", now)

    def update(self, now: float) -> None:
        self.app.controller.update(now)

    # --- drawing -----------------------------------------------------------------------------------------

    def draw(self, surf) -> None:
        app, c = self.app, self.app.controller
        s, pal = app.settings, app.settings.palette
        now = app.now()
        surf.fill(pal.background)
        view, panel, wide = self.layout(surf)
        g = c.game
        last = (g.last_move.from_sq, g.last_move.to_sq) if g.last_move else None
        view.draw(surf, g.board, s, last, c.selected, c.targets, c.anim, now,
                  c.drag_piece_square, c.drag_pos)
        if c.promotion is not None:
            view.draw_promotion(surf, g.board.side_to_move, s, pygame.mouse.get_pos())
        if g.status.is_over and not c.busy:
            view.draw_banner(surf, status_detail(g), s)
        self._draw_panel(surf, panel, wide, now)

    def _draw_panel(self, surf, panel: pygame.Rect, wide: bool, now: float) -> None:
        app, c = self.app, self.app.controller
        pal = app.settings.palette
        g = c.game
        rounded(surf, pal.panel, panel, 14)
        geo = self.geometry(panel, wide)
        inner, y, disc_r, size = geo["inner"], geo["status_y"], geo["disc_r"], geo["size"]
        if wide:
            draw_text(surf, "CHESS", geo["title"], pal.text, (inner.centerx, inner.y + geo["title"] // 2 + 4),
                      "center", bold=True)
        if not g.status.is_over:
            disc = (inner.x + disc_r + 1, y + disc_r + 2)
            aa_circle(surf, (120, 120, 130), disc, disc_r + 1)
            aa_circle(surf, (240, 240, 240) if g.board.side_to_move == WHITE else (20, 20, 24), disc, disc_r)
        tx = inner.x + 2 * disc_r + 12
        draw_text(surf, status_headline(g), size, pal.text, (tx, y + disc_r + 2), "midleft", bold=True,
                  max_width=inner.right - tx)
        draw_text(surf, status_detail(g), max(11, size - 3), (230, 90, 80) if g.status.name == "CHECK" else pal.muted,
                  (tx, y + disc_r + 2 + size), "midleft", bold=True, max_width=inner.right - tx)
        mouse = pygame.mouse.get_pos()
        buttons = self.buttons(inner, wide, geo["top"])
        label_size = fit_size([b.label for b in buttons], min(b.rect.width for b in buttons) - 12,
                              max(11, int(min(b.rect.height for b in buttons) * 0.42)))
        for b in buttons:
            b.draw(surf, pal, mouse, label_size)
        alpha = c.toast_alpha(now)
        if alpha > 0:
            draw_toast(surf, c.toast.text, max(11, size - 4), pal.text, alpha, inner)


def draw_toast(surf, text: str, size: int, color, alpha: float, inner: pygame.Rect) -> None:
    """Confirmation text at the bottom of a panel, fading out."""
    img = text_surface(text, size, color, True)
    if img.get_width() > inner.width:
        img = pygame.transform.smoothscale(img, (inner.width, round(img.get_height() * inner.width / img.get_width())))
    img.set_alpha(round(255 * alpha))
    surf.blit(img, img.get_rect(midbottom=(inner.centerx, inner.bottom)))
