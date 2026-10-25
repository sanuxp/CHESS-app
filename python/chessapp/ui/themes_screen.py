"""The Themes view (docs/THEMES.md): board colours, piece sets, interface palettes and three switches."""
from __future__ import annotations

import pygame

from ..core.types import BLACK, PAWN, WHITE, make_piece
from .gfx import Button, draw_text, hit, rounded
from .pieces import draw_piece
from .theme import BOARD_THEMES, PIECE_SETS, UI_PALETTES

CYCLES = (("board", "BOARD", BOARD_THEMES), ("pieces", "PIECES", PIECE_SETS), ("palette", "INTERFACE", UI_PALETTES))
TOGGLES = (("coordinates", "COORDINATES"), ("hints", "MOVE HINTS"), ("animations", "ANIMATIONS"))
ROWS = [c[0] for c in CYCLES] + [t[0] for t in TOGGLES]


class ThemesScreen:
    def __init__(self, app):
        self.app = app
        self.focus = 0

    # --- layout ------------------------------------------------------------------------------------------

    def geometry(self, surf) -> tuple:
        """(card rect, {row key: rect}, [buttons])."""
        w, h = surf.get_size()
        card = pygame.Rect(0, 0, min(620, w - 24), min(560, h - 24))
        card.center = (w // 2, h // 2)
        pad = max(10, card.width // 30)
        title_h = max(36, card.height // 9)
        foot_h = max(34, card.height // 9)
        avail = card.height - 2 * pad - title_h - foot_h
        row_h = max(30, min(64, avail // len(ROWS)))
        rows = {}
        for i, key in enumerate(ROWS):
            rows[key] = pygame.Rect(card.x + pad, card.y + pad + title_h + i * row_h, card.width - 2 * pad, row_h - 6)
        bw = (card.width - 3 * pad) // 2
        y = card.bottom - pad - foot_h + 6
        buttons = [Button("done", "DONE", (card.x + pad, y, bw, foot_h - 6)),
                   Button("reset", "RESET", (card.x + 2 * pad + bw, y, bw, foot_h - 6))]
        return card, rows, buttons

    # --- events ------------------------------------------------------------------------------------------

    def cycle(self, key: str, step: int) -> None:
        s = self.app.settings
        options = next(c[2] for c in CYCLES if c[0] == key)
        current = getattr(s, key)
        setattr(s, key, options[(options.index(current) + step) % len(options)])
        s.save()

    def toggle(self, key: str) -> None:
        s = self.app.settings
        setattr(s, key, not getattr(s, key))
        s.save()

    def activate(self, key: str, step: int = 1) -> None:
        if key in (c[0] for c in CYCLES):
            self.cycle(key, step)
        else:
            self.toggle(key)

    def handle_event(self, e) -> None:
        app = self.app
        card, rows, buttons = self.geometry(app.surface)
        if e.type == pygame.KEYDOWN:
            key = ROWS[self.focus]
            if e.key == pygame.K_ESCAPE:
                app.close_themes()
            elif e.key == pygame.K_UP:
                self.focus = (self.focus - 1) % len(ROWS)
            elif e.key == pygame.K_DOWN:
                self.focus = (self.focus + 1) % len(ROWS)
            elif e.key == pygame.K_LEFT:
                self.activate(key, -1)
            elif e.key in (pygame.K_RIGHT, pygame.K_RETURN, pygame.K_SPACE):
                self.activate(key, 1)
        elif e.type == pygame.MOUSEBUTTONDOWN and e.button == 1:
            action = hit(buttons, e.pos)
            if action == "done":
                app.close_themes()
            elif action == "reset":
                app.settings.reset()
                app.settings.save()
            else:
                for i, (key, r) in enumerate(rows.items()):
                    if r.collidepoint(e.pos):
                        self.focus = i
                        left_zone = e.pos[0] < r.centerx and key in (c[0] for c in CYCLES)
                        self.activate(key, -1 if left_zone else 1)

    def update(self, now: float) -> None:
        pass

    # --- drawing -----------------------------------------------------------------------------------------

    def draw(self, surf) -> None:
        s = self.app.settings
        pal = s.palette
        surf.fill(pal.background)
        card, rows, buttons = self.geometry(surf)
        rounded(surf, pal.panel, card, 16)
        draw_text(surf, "THEMES", max(18, card.height // 16), pal.text, (card.centerx, card.y + card.height // 14),
                  "center", bold=True)
        for i, (key, r) in enumerate(rows.items()):
            rounded(surf, pal.button_disabled if i != self.focus else pal.button, r, 8)
            label = next((c[1] for c in CYCLES if c[0] == key), None) or next(t[1] for t in TOGGLES if t[0] == key)
            size = max(12, min(22, r.height // 2))
            draw_text(surf, label, size, pal.text, (r.x + 14, r.centery), "midleft", bold=True)
            if key in (c[0] for c in CYCLES):
                value = getattr(s, key)
                draw_text(surf, f"<  {value.name}  >", size, pal.text, (r.centerx + r.width // 8, r.centery), "center",
                          bold=True)
                self._swatch(surf, key, value, pygame.Rect(r.right - r.height - 8, r.y + 4, r.height - 8, r.height - 8))
            else:
                on = getattr(s, key)
                draw_text(surf, "ON" if on else "OFF", size, (110, 200, 120) if on else pal.muted,
                          (r.right - 16, r.centery), "midright", bold=True)
        mouse = pygame.mouse.get_pos()
        for b in buttons:
            b.draw(surf, pal, mouse)

    def _swatch(self, surf, key, value, r: pygame.Rect) -> None:
        if key == "board":
            half = r.width // 2
            pygame.draw.rect(surf, value.light, (r.x, r.y, half, r.height))
            pygame.draw.rect(surf, value.dark, (r.x + half, r.y, r.width - half, r.height))
        elif key == "pieces":
            pygame.draw.rect(surf, self.app.settings.board.dark, r)
            draw_piece(surf, make_piece(PAWN, WHITE), pygame.Rect(r.x - r.width // 10, r.y, r.width, r.height), value)
            draw_piece(surf, make_piece(PAWN, BLACK), pygame.Rect(r.x + r.width // 10, r.y, r.width, r.height), value)
        else:
            third = r.width // 3
            for i, color in enumerate((value.background, value.panel, value.button)):
                pygame.draw.rect(surf, color, (r.x + i * third, r.y, third + 1, r.height))
        pygame.draw.rect(surf, (128, 128, 128), r, 1)
