"""The window and the main loop. Screens (game, themes, review) draw into one resizable pygame window."""
from __future__ import annotations

import time
from pathlib import Path

import pygame

from .controller import GameController
from .game_screen import GameScreen
from .gfx import font, reset_fonts
from .theme import Settings, default_settings_path

MIN_SIZE = (320, 400)
ICON = Path(__file__).resolve().parents[3] / "flutter" / "assets" / "icon" / "icon_square.png"


class App:
    def __init__(self, size=(1100, 720), settings_path="default"):
        pygame.init()
        reset_fonts()
        pygame.display.set_caption("Chess")
        self.surface = pygame.display.set_mode(size, pygame.RESIZABLE)
        try:
            pygame.display.set_icon(pygame.transform.smoothscale(pygame.image.load(str(ICON)), (64, 64)))
        except (pygame.error, FileNotFoundError):
            pass
        path = default_settings_path() if settings_path == "default" else settings_path
        self.settings = Settings(path)
        self.settings.load()
        self.controller = GameController(animations=self.settings.animations)
        self.game_screen = GameScreen(self)
        self.screen = self.game_screen
        self.clock = pygame.time.Clock()
        self.running = True

    # --- helpers -----------------------------------------------------------------------------------------

    @staticmethod
    def now() -> float:
        return time.monotonic()

    @staticmethod
    def font(size: int, bold: bool = False):
        return font(size, bold)

    # --- screens -----------------------------------------------------------------------------------------

    def open_themes(self) -> None:
        from .themes_screen import ThemesScreen
        self.screen = ThemesScreen(self)

    def close_themes(self) -> None:
        self.screen = self.game_screen

    def open_review(self, game=None, source: str = "") -> None:
        from .review_screen import ReviewScreen
        self.screen = ReviewScreen(self, game or self.controller.game, source)

    def close_review(self) -> None:
        if hasattr(self.screen, "shutdown"):
            self.screen.shutdown()
        self.screen = self.game_screen

    # --- loop --------------------------------------------------------------------------------------------

    def handle(self, event) -> None:
        if event.type == pygame.QUIT:
            self.running = False
        elif event.type == pygame.VIDEORESIZE:
            w, h = max(MIN_SIZE[0], event.w), max(MIN_SIZE[1], event.h)
            if (w, h) != (event.w, event.h):
                self.surface = pygame.display.set_mode((w, h), pygame.RESIZABLE)
        else:
            self.screen.handle_event(event)

    def frame(self) -> None:
        """Updates and draws one frame."""
        self.surface = pygame.display.get_surface()
        self.controller.animations = self.settings.animations
        self.screen.update(self.now())
        self.screen.draw(self.surface)
        pygame.display.flip()

    def run(self) -> None:
        while self.running:
            for event in pygame.event.get():
                self.handle(event)
            self.frame()
            self.clock.tick(60)
        if hasattr(self.screen, "shutdown"):
            self.screen.shutdown()
        pygame.quit()


def main() -> None:
    App().run()
