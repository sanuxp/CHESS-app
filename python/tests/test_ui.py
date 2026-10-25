"""Drives the real pygame screens (headless) with synthetic mouse and key events."""
import json
import time

import pygame
import pytest

from chessapp.core import Game, import_pgn
from chessapp.core.types import KNIGHT, square_from_name as sq
from chessapp.review import UciEngine
from chessapp.ui import review_screen as review_module
from chessapp.ui.app import App
from chessapp.ui.game_screen import game_layout
from chessapp.ui.theme import BOARD_THEMES, PIECE_SETS, UI_PALETTES, Settings
from chessapp.ui.themes_screen import ThemesScreen
from test_review import FakeTransport


@pytest.fixture
def app(tmp_path):
    a = App((1100, 720), settings_path=tmp_path / "settings.json")
    a.settings.animations = a.controller.animations = False
    yield a
    pygame.quit()


def mouse(app, kind, pos):
    app.handle(pygame.event.Event(kind, pos=pos, button=1))


def click(app, pos):
    mouse(app, pygame.MOUSEBUTTONDOWN, pos)
    mouse(app, pygame.MOUSEBUTTONUP, pos)


def key(app, k, mod=0):
    app.handle(pygame.event.Event(pygame.KEYDOWN, key=k, mod=mod, unicode=""))


def square_pos(app, name):
    view, _, _ = app.game_screen.layout(app.surface)
    return view.square_rect(sq(name)).center


def play(app, *moves):
    for m in moves:
        click(app, square_pos(app, m[:2]))
        click(app, square_pos(app, m[2:4]))


def button_pos(app, action):
    view, panel, wide = app.game_screen.layout(app.surface)
    geo = app.game_screen.geometry(panel, wide)
    return next(b.rect.center for b in app.game_screen.buttons(geo["inner"], wide, geo["top"]) if b.action == action)


# --- the game screen -----------------------------------------------------------------------------------------

def test_clicks_play_a_move_and_pixels_map_to_squares(app):
    view, _, _ = app.game_screen.layout(app.surface)
    for name in ("a1", "h1", "a8", "h8", "e4"):
        assert view.square_at(view.square_rect(sq(name)).center) == sq(name)
    assert view.square_at((0, 0)) == -1  # outside the board
    play(app, "e2e4")
    assert app.controller.game.move_count == 1


def test_panel_buttons(app):
    play(app, "e2e4")
    click(app, button_pos(app, "flip"))
    assert app.controller.flip_mode and app.controller.flipped
    click(app, button_pos(app, "undo"))
    assert app.controller.game.move_count == 0
    play(app, "d2d4")
    click(app, button_pos(app, "new"))
    assert app.controller.game.move_count == 0


def test_undo_button_disabled_without_moves(app):
    view, panel, wide = app.game_screen.layout(app.surface)
    geo = app.game_screen.geometry(panel, wide)
    undo = next(b for b in app.game_screen.buttons(geo["inner"], wide, geo["top"]) if b.action == "undo")
    assert not undo.enabled


def test_keyboard_undo_and_escape(app):
    play(app, "e2e4")
    key(app, pygame.K_u)
    assert app.controller.game.move_count == 0
    play(app, "e2e4")
    key(app, pygame.K_z, pygame.KMOD_CTRL)
    assert app.controller.game.move_count == 0
    click(app, square_pos(app, "e2"))
    assert app.controller.selected == sq("e2")
    key(app, pygame.K_ESCAPE)
    assert app.controller.selected == -1


def test_drag_with_mouse_events(app):
    start, end = square_pos(app, "g1"), square_pos(app, "f3")
    mouse(app, pygame.MOUSEBUTTONDOWN, start)
    app.handle(pygame.event.Event(pygame.MOUSEMOTION, pos=(start[0] + 30, start[1] - 30), rel=(30, -30), buttons=(1, 0, 0)))
    assert app.controller.dragging
    app.frame()  # draws the dragged piece
    mouse(app, pygame.MOUSEBUTTONUP, end)
    assert app.controller.game.move_count == 1


def test_promotion_by_clicking_the_picker(app):
    app.controller.load_game(Game.from_fen("4k3/P7/8/8/8/8/8/4K3 w - - 0 1"))
    play(app, "a7a8")
    assert app.controller.promotion is not None
    app.frame()
    view, _, _ = app.game_screen.layout(app.surface)
    knight = next(r for piece, r in view.promotion_rects(0) if piece & 7 == KNIGHT)
    click(app, knight.center)
    assert app.controller.game.history[0].san.startswith("a8=N")


def test_clicking_away_cancels_the_picker(app):
    app.controller.load_game(Game.from_fen("4k3/P7/8/8/8/8/8/4K3 w - - 0 1"))
    play(app, "a7a8")
    click(app, square_pos(app, "e4"))
    assert app.controller.promotion is None and app.controller.game.move_count == 0


def test_animation_runs_and_finishes(app):
    app.settings.animations = app.controller.animations = True
    play(app, "e2e4")
    app.frame()
    assert app.controller.busy
    time.sleep(0.3)
    app.frame()
    assert not app.controller.busy


@pytest.mark.parametrize("size", [(1100, 720), (720, 720), (420, 760), (320, 400), (900, 300)])
def test_layout_fits_the_window(size):
    w, h = size
    board, panel, wide = game_layout(w, h)
    assert board.width == board.height
    assert board.left >= 0 and board.top >= 0 and board.right <= w and board.bottom <= h
    if size[0] >= 320 and size[1] >= 400 and size != (900, 300):
        assert not board.colliderect(panel)
    assert wide == (w >= 1.1 * h)


def test_every_theme_combination_renders(app):
    play(app, "e2e4", "e7e5")
    for board in BOARD_THEMES:
        for ps in PIECE_SETS:
            for pal in UI_PALETTES:
                app.settings.board, app.settings.pieces, app.settings.palette = board, ps, pal
                app.frame()
    app.settings.coordinates = app.settings.hints = False
    app.frame()


def test_game_over_banner_and_small_window_render(app):
    for m in ("f2f3", "e7e5", "g2g4", "d8h4"):
        app.controller.game.make_move_uci(m)
    app.frame()
    app.surface = pygame.display.set_mode((320, 400))
    app.frame()


# --- themes ---------------------------------------------------------------------------------------------------

def test_themes_screen_changes_persist_and_reset(app, tmp_path):
    app.settings.animations = True  # the default (the fixture turns it off to keep other tests instant)
    click(app, button_pos(app, "themes"))
    assert isinstance(app.screen, ThemesScreen)
    app.frame()
    card, rows, buttons = app.screen.geometry(app.surface)
    click(app, (rows["board"].right - 60, rows["board"].centery))  # right half: next board
    assert app.settings.board.name == BOARD_THEMES[1].name
    click(app, (rows["board"].x + 120, rows["board"].centery))  # left half: previous
    assert app.settings.board.name == BOARD_THEMES[0].name
    key(app, pygame.K_DOWN)
    key(app, pygame.K_RIGHT)  # pieces row
    assert app.settings.pieces.name == PIECE_SETS[1].name
    click(app, rows["animations"].center)
    click(app, rows["coordinates"].center)
    assert not app.settings.animations and not app.settings.coordinates

    saved = json.loads((tmp_path / "settings.json").read_text())
    assert saved["pieces"] == "Classic" and saved["animations"] is False

    reloaded = Settings(tmp_path / "settings.json")
    reloaded.load()
    assert reloaded.pieces.name == "Classic" and not reloaded.coordinates

    click(app, buttons[1].rect.center)  # RESET
    assert app.settings.pieces.name == "Engraved" and app.settings.animations
    key(app, pygame.K_ESCAPE)
    assert app.screen is app.game_screen


def test_unknown_names_in_settings_fall_back_to_defaults(tmp_path):
    (tmp_path / "s.json").write_text('{"board": "Nonexistent", "pieces": 5, "interface": null, "depth": 99}')
    s = Settings(tmp_path / "s.json")
    s.load()
    assert (s.board.name, s.pieces.name, s.palette.name, s.depth) == ("Brown", "Engraved", "Dark", 16)


def test_corrupt_settings_file_is_ignored(tmp_path):
    (tmp_path / "s.json").write_text("{not json")
    s = Settings(tmp_path / "s.json")
    s.load()
    assert s.board.name == "Brown"


# --- review ---------------------------------------------------------------------------------------------------

def wait_for(screen, states, timeout=30):
    t0 = time.time()
    while screen.state not in states and time.time() - t0 < timeout:
        time.sleep(0.02)
    assert screen.state in states, screen.state


def scripted_engine():
    """An engine that answers every position with the same canned analysis."""
    t = FakeTransport({
        "uci": ["id name FakeFish 1", "uciok"], "isready": ["readyok"],
        "go": ["info depth 1 multipv 1 score cp 30 pv e2e4 e7e5", "info depth 1 multipv 2 score cp 20 pv d2d4 d7d5",
               "bestmove e2e4"],
    })
    engine = UciEngine(t)
    engine.start(timeout=1)
    return engine


@pytest.fixture
def reviewed(app, monkeypatch, tmp_path):
    monkeypatch.setattr(review_module, "open_engine", scripted_engine)
    monkeypatch.setattr(review_module, "load_book", lambda: None)
    play(app, "e2e4", "e7e5", "g1f3", "b8c6")
    click(app, button_pos(app, "review"))
    screen = app.screen
    app.frame()
    screen.start()
    wait_for(screen, ("results", "error"))
    app.frame()
    return screen


def test_review_flow_results_and_navigation(app, reviewed):
    s = reviewed
    assert s.state == "results" and len(s.review.plies) == 4 and s.pos == 4
    key(app, pygame.K_LEFT)
    assert s.pos == 3
    key(app, pygame.K_HOME)
    assert s.pos == 0
    key(app, pygame.K_END)
    assert s.pos == 4
    key(app, pygame.K_f)
    assert s.flipped
    app.frame()
    g = s.geometry(app.surface)
    graph = g["graph"]
    click(app, (graph.x + 1, graph.centery))
    assert s.pos == 0
    click(app, (graph.right - 1, graph.centery))
    assert s.pos == 4
    for action, expected in (("prev", 3), ("first", 0), ("next", 1), ("last", 4)):
        pos = next(b.rect.center for b in s.result_buttons(g) if b.action == action)
        click(app, pos)
        assert s.pos == expected
    lst = g["list"]
    click(app, (lst.x + lst.width * 0.3, lst.y + 4))  # first row, White's move
    assert s.pos == 1
    arrow = next(b.rect.center for b in s.result_buttons(g) if b.action == "arrow")
    click(app, arrow)
    assert not s.show_arrow
    app.frame()


def test_review_save_pgn_has_annotations_and_reimports(app, reviewed, monkeypatch, tmp_path):
    target = tmp_path / "reviewed.pgn"
    monkeypatch.setattr(review_module, "ask_save_pgn", lambda name=None: str(target))
    g = reviewed.geometry(app.surface)
    click(app, next(b.rect.center for b in reviewed.result_buttons(g) if b.action == "save"))
    text = target.read_text(encoding="utf-8")
    assert "[%eval" in text
    again = import_pgn(text)
    assert [h.san for h in again.game.history] == ["e4", "e5", "Nf3", "Nc6"] and again.error is None


def test_review_back_returns_to_game(app, reviewed):
    key(app, pygame.K_ESCAPE)
    assert app.screen is app.game_screen


def test_missing_engine_explains_how_to_install(app, monkeypatch):
    monkeypatch.setattr(review_module, "open_engine", lambda: None)
    play(app, "e2e4")
    click(app, button_pos(app, "review"))
    screen = app.screen
    screen.start()
    wait_for(screen, ("error",))
    assert "Stockfish" in screen.error and "get_stockfish" in screen.error
    app.frame()


def test_review_needs_moves(app):
    click(app, button_pos(app, "review"))
    screen = app.screen
    screen.start()
    assert screen.state == "setup"
    app.frame()
    screen.handle_event(pygame.event.Event(pygame.KEYDOWN, key=pygame.K_ESCAPE, mod=0, unicode=""))
    assert app.screen is app.game_screen


def test_cancel_stops_a_running_review(app, monkeypatch):
    class Slow(UciEngine):
        def analyze(self, fen, depth=14, multi_pv=3, timeout=120, is_cancelled=None):
            for _ in range(200):
                if is_cancelled is not None and is_cancelled():
                    from chessapp.review import EngineException
                    raise EngineException("cancelled")
                time.sleep(0.01)
            return []

    def slow():
        e = Slow(FakeTransport({"uci": ["uciok"], "isready": ["readyok"]}))
        e.start(timeout=1)
        return e

    monkeypatch.setattr(review_module, "open_engine", slow)
    monkeypatch.setattr(review_module, "load_book", lambda: None)
    play(app, "e2e4", "e7e5")
    click(app, button_pos(app, "review"))
    screen = app.screen
    screen.start()
    t0 = time.time()
    while screen.progress == (0, 1) and screen.state == "running" and time.time() - t0 < 5:
        time.sleep(0.01)
    key(app, pygame.K_ESCAPE)  # cancels the running review first
    wait_for(screen, ("setup",))
    assert app.screen is screen
    app.frame()


def test_open_pgn_file_then_review(app, monkeypatch, tmp_path):
    f = tmp_path / "g.pgn"
    f.write_text("[Result \"*\"]\n\n1. d4 d5 2. c4 e6 *\n", encoding="utf-8")
    monkeypatch.setattr(review_module, "ask_open_pgn", lambda: str(f))
    monkeypatch.setattr(review_module, "open_engine", scripted_engine)
    monkeypatch.setattr(review_module, "load_book", lambda: None)
    click(app, button_pos(app, "review"))
    screen = app.screen
    click(app, next(b.rect.center for b in screen.setup_buttons(app.surface) if b.action == "open"))
    assert screen.source == "g.pgn" and len(screen.game.history) == 4
    click(app, next(b.rect.center for b in screen.setup_buttons(app.surface) if b.action == "start"))
    wait_for(screen, ("results",))
    assert [p.san for p in screen.review.plies] == ["d4", "d5", "c4", "e6"]
    assert app.controller.game.move_count == 0  # the game on the board is untouched


def test_review_of_a_game_that_started_from_a_black_move(app, monkeypatch):
    monkeypatch.setattr(review_module, "open_engine", scripted_engine)
    monkeypatch.setattr(review_module, "load_book", lambda: None)
    app.controller.load_game(Game.from_fen("4k3/8/8/8/8/8/4K3/R7 b - - 0 7"))
    play(app, "e8d7", "a1a7")
    click(app, button_pos(app, "review"))
    screen = app.screen
    screen.start()
    wait_for(screen, ("results",))
    assert screen.rows()[0] == (7, None, 1)
    app.frame()
