"""The game review screen (docs/UI_SPEC.md): a setup card, a progress view while Stockfish works, and the
results (board with badge and arrow, accuracy, evaluation graph, move list)."""
from __future__ import annotations

import os
import threading
from pathlib import Path

import pygame

from ..core.pgn import PgnHeaders, import_pgn, to_pgn
from ..core.types import BLACK, WHITE, square_from_name
from ..review import (
    EngineAnalyzer, GameReview, MoveLabel, OpeningBook, ReviewCancelled, engine_search_hint, open_engine,
    review_annotations,
)
from .badges import MEANING, draw_arrow, draw_badge, label_color
from .board_view import BoardView
from .files import ask_open_pgn, ask_save_pgn, default_pgn_name, read_text, write_text
from .game_screen import draw_toast, game_layout
from .gfx import Button, draw_text, fill_alpha, fit_size, hit, rounded
from .theme import rgb

OPENINGS = Path(__file__).resolve().parents[3] / "docs" / "data" / "openings.tsv"
DEPTHS = ((12, "Quick 12"), (16, "Standard 16"), (20, "Deep 20"))
GRAPH_MARKS = (MoveLabel.BRILLIANT, MoveLabel.GREAT, MoveLabel.MISTAKE, MoveLabel.MISS, MoveLabel.BLUNDER)

_book_cache: dict = {}


def load_book() -> "OpeningBook | None":
    """The opening database, parsed once per run (about a second)."""
    if "book" not in _book_cache:
        try:
            _book_cache["book"] = OpeningBook.load(OPENINGS)
        except OSError:
            _book_cache["book"] = None
    return _book_cache["book"]


class ReviewScreen:
    def __init__(self, app, game, source: str = ""):
        self.app = app
        self.game = game
        self.source = source
        self.state = "setup"  # setup | running | results | error
        self.review: "GameReview | None" = None
        self.error = ""
        self.progress = (0, 1)
        self.phase = ""
        self.pos = 0  # position shown: 0 = start, k = after the k-th move
        self.flipped = False
        self.show_arrow = True
        self.scroll = 0
        self.toast_text, self.toast_start = "", -10.0
        self._cancel = threading.Event()
        self._thread: "threading.Thread | None" = None
        self._boards = []

    # --- running the engine ------------------------------------------------------------------------------

    def start(self) -> None:
        if self.state == "running" or not self.game.history:
            return
        self.state, self.error, self.progress, self.phase = "running", "", (0, 1), "Starting the engine"
        self._cancel.clear()
        self._thread = threading.Thread(target=self._work, args=(self.app.settings.depth,), daemon=True)
        self._thread.start()

    def _work(self, depth: int) -> None:
        engine = None
        try:
            self.phase = "Loading the opening book"
            book = load_book()
            self.phase = "Starting the engine"
            engine = open_engine()
            if engine is None:
                self.error = engine_search_hint()
                self.state = "error"
                return
            analyzer = EngineAnalyzer(engine, depth=depth)
            analyzer.configure()
            self.phase = f"Analysing with {engine.name or 'the engine'}"

            def on_progress(done, total):
                self.progress = (done, total)

            review = analyzer.review(self.game, book=book, on_progress=on_progress, is_cancelled=self._cancel.is_set)
            self._boards = [h.before for h in self.game.history] + [self.game.board]
            self.review = review
            self.pos = len(self.game.history)
            self.scroll = 0
            self.state = "results"
        except ReviewCancelled:
            self.state = "setup"
        except Exception as e:  # engine missing, crashed, or too slow: show it instead of dying
            self.error = str(e) or e.__class__.__name__
            self.state = "error"
        finally:
            if engine is not None:
                engine.dispose()

    def shutdown(self) -> None:
        """Stops a running analysis (called when leaving the screen or closing the window)."""
        self._cancel.set()
        if self._thread is not None and self._thread.is_alive():
            self._thread.join(timeout=2)

    # --- helpers -----------------------------------------------------------------------------------------

    def toast(self, text: str) -> None:
        self.toast_text, self.toast_start = text, self.app.now()

    def select(self, pos: int) -> None:
        n = len(self.game.history)
        self.pos = max(0, min(n, pos))
        self._reveal()

    def _ply(self):
        return self.review.plies[self.pos - 1] if self.review and self.pos >= 1 else None

    def source_text(self) -> str:
        n = len(self.game.history)
        return self.source or f"the current game: {(n + 1) // 2} move{'s' if (n + 1) // 2 != 1 else ''}"

    def open_file(self) -> None:
        path = ask_open_pgn()
        if not path:
            return
        text = read_text(path)
        if text is None:
            self.error, self.state = "Could not read that file.", "error"
            return
        result = import_pgn(text)
        if not result.game.history:
            self.error, self.state = result.error or "That file has no moves to review.", "error"
            return
        self.game, self.source, self.state, self.review = result.game, os.path.basename(path), "setup", None
        if result.error:
            self.toast("FILE ENDS EARLY: " + result.error)

    def save_pgn(self) -> None:
        if not self.review:
            return
        path = ask_save_pgn(default_pgn_name())
        if path:
            text = _pgn(self.game, self.review)
            self.toast(f"SAVED {os.path.basename(path)}" if write_text(path, text) else "COULD NOT SAVE THE FILE")

    def back(self) -> None:
        self.app.close_review()

    # --- layout ------------------------------------------------------------------------------------------

    def board_view(self, surf) -> BoardView:
        board, _, _ = game_layout(*surf.get_size())
        return BoardView(board, self.flipped)

    def card(self, surf) -> pygame.Rect:
        w, h = surf.get_size()
        card = pygame.Rect(0, 0, min(560, w - 24), min(440, h - 24))
        card.center = (w // 2, h // 2)
        return card

    def setup_buttons(self, surf) -> list:
        card = self.card(surf)
        pad = max(12, card.width // 24)
        bh = max(30, min(48, card.height // 9))
        y = card.y + round(card.height * 0.42)
        third = (card.width - 2 * pad - 2 * 8) // 3
        if self.state == "running":
            return [Button("cancel", "Cancel", (card.x + pad, card.bottom - pad - bh, card.width - 2 * pad, bh))]
        out = [Button(f"depth{d}", label, (card.x + pad + i * (third + 8), y, third, bh)) for i, (d, label) in enumerate(DEPTHS)]
        y += bh + 14
        out.append(Button("start", "Start review", (card.x + pad, y, card.width - 2 * pad, bh), bool(self.game.history)))
        y += bh + 8
        half = (card.width - 2 * pad - 8) // 2
        out.append(Button("open", "Open a PGN file", (card.x + pad, y, half, bh)))
        out.append(Button("back", "Back", (card.x + pad + half + 8, y, half, bh)))
        return out

    def geometry(self, surf) -> dict:
        board, panel, wide = game_layout(*surf.get_size())
        pad = max(8, panel.width // 40)
        inner = panel.inflate(-2 * pad, -2 * pad)
        u = max(0.62, min(1.15, inner.height / 640))
        gap = max(4, round(8 * u))
        g = {"panel": panel, "inner": inner, "u": u, "wide": wide}
        y = inner.y

        def take(h):
            nonlocal y
            r = pygame.Rect(inner.x, y, inner.width, h)
            y += h + gap
            return r

        g["top"] = take(round(36 * u))
        g["acc"] = take(round(50 * u))
        g["opening"] = take(round(22 * u))
        g["graph"] = take(max(44, round(inner.height * 0.14)))
        g["info"] = take(round(66 * u))
        g["nav"] = take(round(34 * u))
        g["list"] = pygame.Rect(inner.x, y, inner.width, max(40, inner.bottom - y))
        return g

    def result_buttons(self, g: dict) -> list:
        n = len(self.game.history)
        out = []
        top = g["top"]
        labels = [("back", "Back"), ("flip", "Flip"), ("arrow", f"Arrow: {'On' if self.show_arrow else 'Off'}"),
                  ("save", "Save PGN")]
        gap = 6
        w = (top.width - gap * 3) // 4
        for i, (action, label) in enumerate(labels):
            out.append(Button(action, label, (top.x + i * (w + gap), top.y, w, top.height)))
        nav = g["nav"]
        w = (nav.width - gap * 3) // 4
        for i, (action, label, enabled) in enumerate([("first", "<<", self.pos > 0), ("prev", "<", self.pos > 0),
                                                       ("next", ">", self.pos < n), ("last", ">>", self.pos < n)]):
            out.append(Button(action, label, (nav.x + i * (w + gap), nav.y, w, nav.height), enabled))
        return out

    # --- move list ---------------------------------------------------------------------------------------

    def row_height(self, g) -> int:
        return max(22, round(28 * g["u"]))

    def rows(self) -> list:
        """[(move number, white ply index or None, black ply index or None)], plies being 1-based."""
        start = self.game.initial
        n = len(self.game.history)
        rows, ply = [], 1
        number = start.fullmove_number
        if start.side_to_move == BLACK and n:
            rows.append((number, None, 1))
            ply, number = 2, number + 1
        while ply <= n:
            rows.append((number, ply, ply + 1 if ply + 1 <= n else None))
            ply, number = ply + 2, number + 1
        return rows

    def _reveal(self) -> None:
        """Scrolls the move list so the selected move is visible."""
        if self.review is None or self.pos < 1:
            return
        g = self.geometry(self.app.surface)
        rh = self.row_height(g)
        index = next((i for i, (_, w, b) in enumerate(self.rows()) if self.pos in (w, b)), 0)
        top, bottom = index * rh, (index + 1) * rh
        if top < self.scroll:
            self.scroll = top
        elif bottom > self.scroll + g["list"].height:
            self.scroll = bottom - g["list"].height
        self._clamp_scroll(g)

    def _clamp_scroll(self, g) -> None:
        total = len(self.rows()) * self.row_height(g)
        self.scroll = max(0, min(self.scroll, max(0, total - g["list"].height)))

    def list_cell_at(self, g, pos) -> "int | None":
        lst = g["list"]
        if not lst.collidepoint(pos):
            return None
        rows = self.rows()
        i = (pos[1] - lst.y + self.scroll) // self.row_height(g)
        if not 0 <= i < len(rows):
            return None
        _, w, b = rows[i]
        num_w = round(lst.width * 0.16)
        col_w = (lst.width - num_w) / 2
        x = pos[0] - lst.x - num_w
        if x < 0:
            return None
        return (w if x < col_w else b)

    # --- events ------------------------------------------------------------------------------------------

    def handle_event(self, e) -> None:
        if self.state == "results":
            self._results_event(e)
        else:
            self._setup_event(e)

    def _setup_event(self, e) -> None:
        surf = self.app.surface
        if e.type == pygame.KEYDOWN and e.key == pygame.K_ESCAPE:
            if self.state == "running":
                self._cancel.set()
            else:
                self.back()
        elif e.type == pygame.MOUSEBUTTONDOWN and e.button == 1:
            action = hit(self.setup_buttons(surf), e.pos)
            if action is None:
                return
            if action.startswith("depth"):
                self.app.settings.depth = int(action[5:])
                self.app.settings.save()
                if self.state == "error":
                    self.state = "setup"
            elif action == "start":
                self.start()
            elif action == "cancel":
                self._cancel.set()
            elif action == "open":
                self.open_file()
            elif action == "back":
                self.back()

    def _results_event(self, e) -> None:
        surf = self.app.surface
        g = self.geometry(surf)
        n = len(self.game.history)
        if e.type == pygame.KEYDOWN:
            k = e.key
            if k == pygame.K_ESCAPE:
                self.back()
            elif k == pygame.K_LEFT:
                self.select(self.pos - 1)
            elif k == pygame.K_RIGHT:
                self.select(self.pos + 1)
            elif k == pygame.K_HOME:
                self.select(0)
            elif k == pygame.K_END:
                self.select(n)
            elif k == pygame.K_f:
                self.flipped = not self.flipped
        elif e.type == pygame.MOUSEWHEEL:
            if g["list"].collidepoint(pygame.mouse.get_pos()):
                self.scroll -= e.y * self.row_height(g)
                self._clamp_scroll(g)
        elif e.type == pygame.MOUSEBUTTONDOWN and e.button == 1:
            action = hit(self.result_buttons(g), e.pos)
            if action == "back":
                self.back()
            elif action == "flip":
                self.flipped = not self.flipped
            elif action == "arrow":
                self.show_arrow = not self.show_arrow
            elif action == "save":
                self.save_pgn()
            elif action == "first":
                self.select(0)
            elif action == "prev":
                self.select(self.pos - 1)
            elif action == "next":
                self.select(self.pos + 1)
            elif action == "last":
                self.select(n)
            elif g["graph"].collidepoint(e.pos) and n:
                self.select(round((e.pos[0] - g["graph"].x) / max(1, g["graph"].width) * n))
            else:
                cell = self.list_cell_at(g, e.pos)
                if cell is not None:
                    self.select(cell)

    def update(self, now: float) -> None:
        pass

    # --- drawing -----------------------------------------------------------------------------------------

    def draw(self, surf) -> None:
        pal = self.app.settings.palette
        surf.fill(pal.background)
        if self.state == "results":
            self._draw_results(surf)
        else:
            self._draw_setup(surf)

    def _draw_setup(self, surf) -> None:
        pal = self.app.settings.palette
        card = self.card(surf)
        rounded(surf, pal.panel, card, 16)
        pad = max(12, card.width // 24)
        draw_text(surf, "GAME REVIEW", max(20, card.height // 12), pal.text, (card.centerx, card.y + card.height * 0.1),
                  "center", bold=True)
        draw_text(surf, f"Reviewing {self.source_text()}", max(13, card.height // 28), pal.muted,
                  (card.centerx, card.y + card.height * 0.2), "center", max_width=card.width - 2 * pad)
        if self.state == "running":
            self._draw_progress(surf, card, pad)
        elif self.state == "error":
            self._draw_error(surf, card, pad)
        else:
            hint = "Stockfish checks every position, so a long game takes a while."
            draw_text(surf, hint, max(11, card.height // 34), pal.muted, (card.centerx, card.y + card.height * 0.3),
                      "center", max_width=card.width - 2 * pad)
        mouse = pygame.mouse.get_pos()
        buttons = self.setup_buttons(surf)
        size = fit_size([b.label for b in buttons], min(b.rect.width for b in buttons) - 12,
                        max(11, int(min(b.rect.height for b in buttons) * 0.42)))
        for b in buttons:
            if b.action.startswith("depth"):
                selected = int(b.action[5:]) == self.app.settings.depth
                rounded(surf, pal.button_hover if selected else pal.button_disabled, b.rect, max(4, b.rect.height // 5))
                if selected:
                    rounded(surf, pal.text, b.rect, max(4, b.rect.height // 5), 2)
                draw_text(surf, b.label, size, pal.button_text if b.enabled else pal.muted, b.rect.center, "center",
                          bold=True, max_width=b.rect.width - 8)
            else:
                b.draw(surf, pal, mouse, size)
        if self.app.now() - self.toast_start < 4 and self.toast_text:
            draw_toast(surf, self.toast_text, max(11, card.height // 30), pal.text, 1.0, card.inflate(-2 * pad, -pad))

    def _draw_progress(self, surf, card, pad) -> None:
        pal = self.app.settings.palette
        done, total = self.progress
        bar = pygame.Rect(card.x + pad, card.y + round(card.height * 0.32), card.width - 2 * pad, max(14, card.height // 22))
        rounded(surf, pal.button_disabled, bar, bar.height // 2)
        fill = bar.copy()
        fill.width = round(bar.width * done / max(1, total))
        if fill.width > 0:
            rounded(surf, rgb("#81B64C"), fill, bar.height // 2)
        draw_text(surf, f"{self.phase}  -  position {done} of {total}", max(12, card.height // 30), pal.text,
                  (card.centerx, bar.bottom + card.height // 20), "center", max_width=card.width - 2 * pad)

    def _draw_error(self, surf, card, pad) -> None:
        box = pygame.Rect(card.x + pad, card.y + round(card.height * 0.27), card.width - 2 * pad, round(card.height * 0.14))
        rounded(surf, (110, 40, 40), box, 8)
        words, lines, line = self.error.split(), [], ""
        size = max(11, card.height // 36)
        for word in words:
            trial = (line + " " + word).strip()
            if self.app.font(size).size(trial)[0] > box.width - 16 and line:
                lines.append(line)
                line = word
            else:
                line = trial
        lines.append(line)
        lh = size + 3
        y = box.centery - lh * min(len(lines), 4) / 2
        for text in lines[:4]:
            draw_text(surf, text, size, (255, 230, 225), (box.centerx, y), "midtop", max_width=box.width - 12)
            y += lh

    def _draw_results(self, surf) -> None:
        app = self.app
        s, pal = app.settings, app.settings.palette
        g = self.geometry(surf)
        view = self.board_view(surf)
        ply = self._ply()
        last = None
        if ply is not None:
            m = self.game.history[self.pos - 1].move
            last = (m.from_sq, m.to_sq)
        view.draw(surf, self._boards[self.pos], s, last)
        if ply is not None:
            c = view.cell
            r = view.square_rect(self.game.history[self.pos - 1].move.to_sq)
            if self.show_arrow and ply.has_better_move:
                a, b = square_from_name(ply.best_move[:2]), square_from_name(ply.best_move[2:4])
                draw_arrow(surf, view.center(a), view.center(b), (110, 190, 80, 190), c * 0.17)
            draw_badge(surf, ply.label, (r.right - c * 0.16, r.y + c * 0.16), c * 0.2)

        panel = g["panel"]
        rounded(surf, pal.panel, panel, 14)
        mouse = pygame.mouse.get_pos()
        buttons = self.result_buttons(g)
        size = fit_size([b.label for b in buttons], min(b.rect.width for b in buttons) - 10,
                        max(10, int(min(b.rect.height for b in buttons) * 0.42)))
        for b in buttons:
            b.draw(surf, pal, mouse, size)
        self._draw_accuracy(surf, g)
        rv = self.review
        opening = rv.opening if rv.opening else "No opening recognised"
        eco = f"{rv.opening_eco}  " if rv.opening_eco else ""
        draw_text(surf, eco + opening, max(11, round(15 * g["u"])), pal.muted, g["opening"].midleft, "midleft",
                  max_width=g["opening"].width)
        self._draw_graph(surf, g["graph"])
        self._draw_info(surf, g["info"], ply)
        self._draw_list(surf, g)
        if app.now() - self.toast_start < 4 and self.toast_text:
            draw_toast(surf, self.toast_text, max(11, round(14 * g["u"])), pal.text, 1.0, g["inner"])

    def _draw_accuracy(self, surf, g) -> None:
        box = g["acc"]
        half = (box.width - 8) // 2
        for i, (name, value, color, ink) in enumerate((("White", self.review.accuracy_white, (240, 240, 240), (30, 30, 34)),
                                                        ("Black", self.review.accuracy_black, (36, 36, 42), (235, 235, 240)))):
            r = pygame.Rect(box.x + i * (half + 8), box.y, half, box.height)
            rounded(surf, color, r, 8)
            if i == 1:
                rounded(surf, (110, 110, 120), r, 8, 1)
            draw_text(surf, name.upper() + " ACCURACY", max(9, round(11 * g["u"])), ink, (r.x + 10, r.y + r.height * 0.28),
                      "midleft", bold=True, max_width=r.width - 16)
            draw_text(surf, f"{value:.1f}", max(14, round(24 * g["u"])), ink, (r.x + 10, r.y + r.height * 0.7),
                      "midleft", bold=True)

    def _draw_graph(self, surf, r: pygame.Rect) -> None:
        pal = self.app.settings.palette
        rv = self.review
        pygame.draw.rect(surf, (56, 56, 64), r, border_radius=6)
        chances = rv.white_win_chance
        n = len(chances) - 1
        if n < 1:
            return

        def x_of(i):
            return r.x + r.width * i / n

        def y_of(v):
            return r.bottom - r.height * v / 100

        pts = [(x_of(i), y_of(v)) for i, v in enumerate(chances)]
        pygame.draw.polygon(surf, (236, 236, 236), pts + [(r.right, r.bottom), (r.x, r.bottom)])
        pygame.draw.line(surf, (120, 120, 130), (r.x, r.centery), (r.right, r.centery), 1)
        for p in rv.plies:
            if p.label in GRAPH_MARKS:
                pygame.draw.circle(surf, label_color(p.label), pts[p.ply], max(3, r.height // 12))
        cx = x_of(self.pos)
        pygame.draw.line(surf, rgb("#F7C631"), (cx, r.y), (cx, r.bottom), 2)
        pygame.draw.rect(surf, pal.button_disabled, r, 1, border_radius=6)

    def _draw_info(self, surf, r: pygame.Rect, ply) -> None:
        pal = self.app.settings.palette
        u = max(0.62, r.height / 66)
        if ply is None:
            draw_text(surf, "Start position", max(12, round(16 * u)), pal.muted, r.midleft, "midleft")
            return
        badge_r = max(9, round(13 * u))
        draw_badge(surf, ply.label, (r.x + badge_r + 2, r.y + badge_r + 2), badge_r)
        tx = r.x + 2 * badge_r + 12
        number = f"{(ply.ply + 1) // 2}{'.' if ply.mover == WHITE else '...'}"
        draw_text(surf, f"{number} {ply.san} is {MEANING[ply.label]}", max(12, round(16 * u)), pal.text,
                  (tx, r.y + badge_r + 2), "midleft", bold=True, max_width=r.right - tx)
        parts = []
        if ply.has_better_move:
            parts.append(f"Better: {ply.best_san}")
        if ply.score_after is not None:
            parts.append(f"Eval {ply.score_after}")
        parts.append(f"Accuracy {ply.accuracy:.0f}%")
        if ply.opening:
            parts.append(ply.opening)
        draw_text(surf, "   ".join(parts[:3]), max(11, round(14 * u)), pal.muted, (r.x, r.y + 2 * badge_r + round(14 * u)),
                  "midleft", max_width=r.width)

    def _draw_list(self, surf, g) -> None:
        pal = self.app.settings.palette
        lst = g["list"]
        rh = self.row_height(g)
        rows = self.rows()
        self._clamp_scroll(g)
        old = surf.get_clip()
        surf.set_clip(lst)
        num_w = round(lst.width * 0.16)
        col_w = (lst.width - num_w) // 2
        size = max(11, round(15 * g["u"]))
        first = self.scroll // rh
        for i in range(first, min(len(rows), first + lst.height // rh + 2)):
            number, w, b = rows[i]
            y = lst.y + i * rh - self.scroll
            draw_text(surf, f"{number}.", size, pal.muted, (lst.x + num_w - 6, y + rh // 2), "midright")
            for k, ply_index in enumerate((w, b)):
                x = lst.x + num_w + k * col_w
                if ply_index is None:
                    if k == 0:
                        draw_text(surf, "...", size, pal.muted, (x + 8, y + rh // 2), "midleft")
                    continue
                if ply_index == self.pos:
                    rounded(surf, pal.button, (x, y + 1, col_w - 2, rh - 2), 6)
                p = self.review.plies[ply_index - 1]
                draw_text(surf, p.san, size, pal.text, (x + 8, y + rh // 2), "midleft", bold=ply_index == self.pos,
                          max_width=col_w - rh - 6)
                draw_badge(surf, p.label, (x + col_w - rh * 0.5 - 2, y + rh // 2), rh * 0.3)
        surf.set_clip(old)
        total = len(rows) * rh
        if total > lst.height:  # scroll bar
            bar_h = max(20, round(lst.height * lst.height / total))
            bar_y = lst.y + round((lst.height - bar_h) * self.scroll / max(1, total - lst.height))
            fill_alpha(surf, (lst.right - 4, bar_y, 4, bar_h), (*pal.muted, 150))


def _pgn(game, review) -> str:
    return to_pgn(game, PgnHeaders(), review_annotations(review))
