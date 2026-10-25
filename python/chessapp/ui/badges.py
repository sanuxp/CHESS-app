"""Colour and mark shown for a move label (badge on the board, marks in the move list)."""
from __future__ import annotations

import math

import pygame

from ..review import MoveLabel
from .gfx import aa_circle, draw_text
from .theme import rgb

# label -> (colour, text mark or None when an icon is drawn)
STYLE = {
    MoveLabel.BRILLIANT: (rgb("#1BAAA6"), "!!"),
    MoveLabel.GREAT: (rgb("#5C8BB0"), "!"),
    MoveLabel.BEST: (rgb("#81B64C"), None),
    MoveLabel.EXCELLENT: (rgb("#96BC4B"), None),
    MoveLabel.GOOD: (rgb("#A3B18A"), None),
    MoveLabel.BOOK: (rgb("#A88865"), None),
    MoveLabel.FORCED: (rgb("#8A8A8A"), None),
    MoveLabel.INACCURACY: (rgb("#F7C631"), "?!"),
    MoveLabel.MISTAKE: (rgb("#E58F2A"), "?"),
    MoveLabel.MISS: (rgb("#E05A4F"), None),
    MoveLabel.BLUNDER: (rgb("#CA3431"), "??"),
}

MEANING = {
    MoveLabel.BRILLIANT: "a brilliant sacrifice",
    MoveLabel.GREAT: "a great find, the only good move",
    MoveLabel.BEST: "the best move",
    MoveLabel.EXCELLENT: "an excellent move",
    MoveLabel.GOOD: "a good move",
    MoveLabel.BOOK: "an opening book move",
    MoveLabel.FORCED: "the only legal move",
    MoveLabel.INACCURACY: "an inaccuracy",
    MoveLabel.MISS: "a missed chance",
    MoveLabel.MISTAKE: "a mistake",
    MoveLabel.BLUNDER: "a blunder",
}

WHITE_INK = (255, 255, 255)


def label_color(label: MoveLabel) -> tuple:
    return STYLE[label][0]


def _star(cx, cy, r):
    pts = []
    for i in range(10):
        ang = -math.pi / 2 + i * math.pi / 5
        rad = r if i % 2 == 0 else r * 0.42
        pts.append((cx + rad * math.cos(ang), cy + rad * math.sin(ang)))
    return pts


def draw_badge(surf, label: MoveLabel, center, radius: float) -> None:
    """A round coloured badge with the label's mark."""
    color, mark = STYLE[label]
    cx, cy = center
    aa_circle(surf, (255, 255, 255), center, radius + max(1, radius * 0.08))
    aa_circle(surf, color, center, radius)
    w = max(2, round(radius * 0.2))
    r = radius * 0.55
    if mark is not None:
        draw_text(surf, mark, max(9, round(radius * (1.15 if len(mark) == 1 else 0.95))), WHITE_INK, (cx, cy + radius * 0.04),
                  "center", bold=True)
    elif label == MoveLabel.BEST:
        pygame.draw.polygon(surf, WHITE_INK, _star(cx, cy + radius * 0.03, radius * 0.66))
    elif label == MoveLabel.EXCELLENT:
        pygame.draw.line(surf, WHITE_INK, (cx - r, cy), (cx + r, cy), w)
        pygame.draw.line(surf, WHITE_INK, (cx, cy - r), (cx, cy + r), w)
    elif label == MoveLabel.GOOD:
        pygame.draw.lines(surf, WHITE_INK, False, [(cx - r, cy), (cx - r * 0.25, cy + r * 0.7), (cx + r, cy - r * 0.6)], w)
    elif label == MoveLabel.MISS:
        pygame.draw.line(surf, WHITE_INK, (cx - r * 0.8, cy - r * 0.8), (cx + r * 0.8, cy + r * 0.8), w)
        pygame.draw.line(surf, WHITE_INK, (cx - r * 0.8, cy + r * 0.8), (cx + r * 0.8, cy - r * 0.8), w)
    elif label == MoveLabel.FORCED:
        pygame.draw.line(surf, WHITE_INK, (cx - r, cy), (cx + r * 0.3, cy), w)
        pygame.draw.polygon(surf, WHITE_INK, [(cx + r * 0.2, cy - r * 0.75), (cx + r, cy), (cx + r * 0.2, cy + r * 0.75)])
    elif label == MoveLabel.BOOK:
        left = [(cx - r, cy - r * 0.6), (cx - r * 0.05, cy - r * 0.35), (cx - r * 0.05, cy + r * 0.7), (cx - r, cy + r * 0.45)]
        right = [(cx + r, cy - r * 0.6), (cx + r * 0.05, cy - r * 0.35), (cx + r * 0.05, cy + r * 0.7), (cx + r, cy + r * 0.45)]
        pygame.draw.polygon(surf, WHITE_INK, left)
        pygame.draw.polygon(surf, WHITE_INK, right)


def draw_arrow(surf, start, end, color, width: float) -> None:
    """A translucent arrow from one point to another (the engine's preferred move)."""
    dx, dy = end[0] - start[0], end[1] - start[1]
    length = math.hypot(dx, dy)
    if length < 1:
        return
    ux, uy = dx / length, dy / length
    nx, ny = -uy, ux
    head = width * 2.2
    base = (end[0] - ux * head, end[1] - uy * head)
    shaft = [(start[0] + nx * width / 2, start[1] + ny * width / 2), (base[0] + nx * width / 2, base[1] + ny * width / 2),
             (base[0] + nx * head * 0.75, base[1] + ny * head * 0.75), end,
             (base[0] - nx * head * 0.75, base[1] - ny * head * 0.75), (base[0] - nx * width / 2, base[1] - ny * width / 2),
             (start[0] - nx * width / 2, start[1] - ny * width / 2)]
    xs, ys = [p[0] for p in shaft], [p[1] for p in shaft]
    box = pygame.Rect(int(min(xs)) - 2, int(min(ys)) - 2, int(max(xs) - min(xs)) + 4, int(max(ys) - min(ys)) + 4)
    layer = pygame.Surface(box.size, pygame.SRCALPHA)
    pygame.draw.polygon(layer, color, [(x - box.x, y - box.y) for x, y in shaft])
    surf.blit(layer, box.topleft)
