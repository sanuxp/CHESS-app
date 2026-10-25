"""Small drawing helpers: fonts, text, translucent shapes and buttons."""
from __future__ import annotations

import pygame

from .theme import UiPalette

_fonts: dict = {}
FONT_NAMES = "segoeui,helveticaneue,arial,dejavusans,sans"


def reset_fonts() -> None:
    """Forgets cached fonts (they become invalid when pygame is shut down and started again)."""
    _fonts.clear()


def font(size: int, bold: bool = False) -> pygame.font.Font:
    size = max(8, int(size))
    key = (size, bold)
    f = _fonts.get(key)
    if f is None:
        f = _fonts[key] = pygame.font.SysFont(FONT_NAMES, size, bold=bold)
    return f


def text_surface(text: str, size: int, color, bold: bool = False) -> pygame.Surface:
    return font(size, bold).render(text, True, color)


def draw_text(surf, text: str, size: int, color, pos, anchor: str = "topleft", bold: bool = False,
              max_width: "int | None" = None) -> pygame.Rect:
    """Draws text with its `anchor` point (a pygame Rect attribute name) at `pos`. Text wider than `max_width` is
    shrunk to fit. Returns the rect drawn."""
    img = text_surface(text, size, color, bold)
    if max_width is not None and img.get_width() > max_width:
        img = text_surface(text, max(8, int(size * max_width / img.get_width())), color, bold)
    rect = img.get_rect(**{anchor: pos})
    surf.blit(img, rect)
    return rect


def fit_size(labels, width: int, size: int, bold: bool = True) -> int:
    """The largest font size up to `size` at which every label is at most `width` pixels wide."""
    while size > 9 and max(font(size, bold).size(t)[0] for t in labels) > width:
        size -= 1
    return size


def fill_alpha(surf, rect, color) -> None:
    """Fills `rect` with an (r, g, b, a) colour, blending over what is there."""
    rect = pygame.Rect(rect)
    layer = pygame.Surface(rect.size, pygame.SRCALPHA)
    layer.fill(color)
    surf.blit(layer, rect.topleft)


def circle_alpha(surf, color, center, radius, width: int = 0) -> None:
    r = max(1, int(radius))
    layer = pygame.Surface((2 * r + 2, 2 * r + 2), pygame.SRCALPHA)
    pygame.draw.circle(layer, color, (r + 1, r + 1), r, width)
    surf.blit(layer, (center[0] - r - 1, center[1] - r - 1))


def aa_circle(surf, color, center, radius) -> None:
    """A smooth filled circle."""
    x, y, r = round(center[0]), round(center[1]), max(1, round(radius))
    pygame.draw.circle(surf, color, (x, y), r)
    try:
        import pygame.gfxdraw as gfx
        gfx.aacircle(surf, x, y, r, color)
    except (ImportError, ValueError):
        pass


def rounded(surf, color, rect, radius: int = 8, width: int = 0) -> None:
    pygame.draw.rect(surf, color, rect, width, border_radius=radius)


class Button:
    def __init__(self, action: str, label: str, rect, enabled: bool = True):
        self.action = action
        self.label = label
        self.rect = pygame.Rect(rect)
        self.enabled = enabled

    def draw(self, surf, palette: UiPalette, mouse, size: "int | None" = None) -> None:
        hover = self.enabled and self.rect.collidepoint(mouse)
        color = palette.button_hover if hover else palette.button if self.enabled else palette.button_disabled
        rounded(surf, color, self.rect, max(4, self.rect.height // 5))
        text_color = palette.button_text if self.enabled else palette.muted
        draw_text(surf, self.label, size or max(11, int(self.rect.height * 0.42)), text_color, self.rect.center,
                  "center", bold=True, max_width=self.rect.width - 12)


def hit(buttons, pos) -> "str | None":
    for b in buttons:
        if b.enabled and b.rect.collidepoint(pos):
            return b.action
    return None
