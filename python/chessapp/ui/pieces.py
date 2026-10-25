"""Piece artwork. Pieces are vector shapes in a 100x100 box (y down), the same geometry the C++ and Flutter apps
use (cpp/app/src/PieceArt.cpp). A body is the union of its parts minus the notches; it is outlined first, then
filled according to the piece set (hatch, flat or gradient). Each piece is drawn once at 4x size and then
scaled to the board's cell size.
"""
from __future__ import annotations

import math

import pygame

from ..core.types import BISHOP, KING, KNIGHT, PAWN, QUEEN, ROOK, WHITE
from .theme import GRADIENT, HATCH, PieceSet

K = 4  # supersampling factor
SIZE = 100 * K
CLEAR = (0, 0, 0, 0)
OPAQUE = (255, 255, 255, 255)

# Parts are tuples: ("rect", x0, y0, x1, y1), ("circle", cx, cy, r), ("ellipse", cx, cy, rx, ry), ("poly", [x, y, ...]).


def _rect(x0, y0, x1, y1):
    return ("rect", x0, y0, x1, y1)


def _circle(cx, cy, r):
    return ("circle", cx, cy, r)


def _ellipse(cx, cy, rx, ry):
    return ("ellipse", cx, cy, rx, ry)


def _poly(xy):
    return ("poly", list(xy))


def _line(x0, y0, x1, y1, w):
    """A thin bar of the given width between two points (detail strokes)."""
    dx, dy = x1 - x0, y1 - y0
    length = math.hypot(dx, dy)
    nx, ny = -dy / length * w / 2, dx / length * w / 2
    return _poly([x0 + nx, y0 + ny, x1 + nx, y1 + ny, x1 - nx, y1 - ny, x0 - nx, y0 - ny])


def _smooth(xy, steps=5):
    """Closed Catmull-Rom smoothing of a polygon outline."""
    n = len(xy) // 2

    def x(i):
        return xy[(i % n) * 2]

    def y(i):
        return xy[(i % n) * 2 + 1]

    def f(a, b, c, d, t):
        t2, t3 = t * t, t * t * t
        return 0.5 * ((2 * b) + (-a + c) * t + (2 * a - 5 * b + 4 * c - d) * t2 + (-a + 3 * b - 3 * c + d) * t3)

    out = []
    for i in range(n):
        for s in range(steps):
            t = s / steps
            out.append(f(x(i - 1 + n), x(i), x(i + 1), x(i + 2), t))
            out.append(f(y(i - 1 + n), y(i), y(i + 1), y(i + 2), t))
    return _poly(out)


def _base_fill():
    return [_ellipse(50, 77, 23, 3.5), _rect(30, 79, 70, 85), _ellipse(50, 79, 28, 4), _ellipse(50, 86, 31, 5)]


def _base_ink():
    return _line(23, 80.5, 77, 80.5, 0.9)


def _shape_for(t):
    """(fill parts, cut parts, ink parts) of a piece type."""
    if t == PAWN:
        return ([_circle(50, 24, 11), _ellipse(50, 39, 14, 3.5),
                 _smooth([44, 40, 56, 40, 58, 52, 63, 64, 70, 74, 30, 74, 37, 64, 42, 52]), *_base_fill()],
                [], [_line(36, 36.5, 64, 36.5, 0.9), _base_ink()])
    if t == ROOK:
        return ([_rect(28, 12, 72, 28), _rect(34, 28, 66, 33), _poly([37, 33, 63, 33, 65, 62, 70, 72, 30, 72, 35, 62]),
                 _rect(27, 69, 73, 75), *_base_fill()],
                [_rect(40, 12, 46, 21), _rect(54, 12, 60, 21)],
                [_line(28, 28, 72, 28, 0.9), _line(34, 33, 66, 33, 0.9), _line(27, 69, 73, 69, 0.9), _base_ink()])
    if t == BISHOP:
        return ([_circle(50, 11, 5), _ellipse(50, 33, 13, 19), _ellipse(50, 53, 16, 3.5),
                 _smooth([45, 54, 55, 54, 58, 63, 65, 72, 35, 72, 42, 63]), *_base_fill()],
                [], [_line(46, 38, 58, 22, 2.4), _line(36, 50, 64, 50, 0.9), _base_ink()])
    if t == KNIGHT:
        return ([_smooth([28, 78, 30, 64, 37, 53, 43, 44, 35, 43, 29, 35, 31, 25, 39, 16, 43, 8, 50, 14,
                          58, 15, 68, 25, 77, 40, 81, 49, 77, 55, 69, 54, 61, 51, 58, 60, 66, 70, 70, 78]),
                 *_base_fill()],
                [], [_circle(55, 28, 2.2), _circle(78, 47, 1.3), _line(38, 26, 46, 30, 1.2), _line(34, 34, 45, 38, 1.2),
                     _line(35, 44, 46, 48, 1.2), _line(37, 53, 47, 56, 1.2), _base_ink()])
    if t == QUEEN:
        return ([_circle(22, 22, 4.5), _circle(36, 15, 4.5), _circle(50, 11, 5), _circle(64, 15, 4.5),
                 _circle(78, 22, 4.5),
                 _poly([22, 24, 32, 50, 36, 26, 44, 48, 50, 20, 56, 48, 64, 26, 68, 50, 78, 24, 72, 62, 66, 70, 34, 70,
                        28, 62]),
                 _ellipse(50, 56, 20, 3.5), *_base_fill()],
                [], [_line(30, 53, 70, 53, 0.9), _base_ink()])
    if t == KING:
        return ([_rect(46.5, 3, 53.5, 24), _rect(40, 8, 60, 14.5), _ellipse(50, 37, 17, 13),
                 _smooth([35, 42, 65, 42, 68, 54, 66, 66, 70, 72, 30, 72, 34, 66, 32, 54]),
                 _ellipse(50, 56, 20, 3.5), *_base_fill()],
                [], [_line(32, 53, 68, 53, 0.9), _base_ink()])
    raise ValueError(t)


def _draw_part(surf, part, color):
    kind = part[0]
    if kind == "rect":
        _, x0, y0, x1, y1 = part
        pygame.draw.rect(surf, color, pygame.Rect(round(x0 * K), round(y0 * K), round((x1 - x0) * K), round((y1 - y0) * K)))
    elif kind == "circle":
        _, cx, cy, r = part
        pygame.draw.circle(surf, color, (cx * K, cy * K), r * K)
    elif kind == "ellipse":
        _, cx, cy, rx, ry = part
        pygame.draw.ellipse(surf, color, pygame.Rect(round((cx - rx) * K), round((cy - ry) * K),
                                                     round(2 * rx * K), round(2 * ry * K)))
    else:
        pts = part[1]
        pygame.draw.polygon(surf, color, [(pts[i] * K, pts[i + 1] * K) for i in range(0, len(pts), 2)])


class _Built:
    """Everything about one silhouette that does not depend on the colours."""

    PERIOD = 3.4  # hatch spacing in 100-unit space

    def __init__(self, t):
        fill, cut, ink = _shape_for(t)
        body = pygame.Surface((SIZE, SIZE), pygame.SRCALPHA)
        for p in fill:
            _draw_part(body, p, OPAQUE)
        for p in cut:
            _draw_part(body, p, CLEAR)
        self.body = body
        self.mask = pygame.mask.from_surface(body)
        rects = self.mask.get_bounding_rects()
        self.left = min(r.left for r in rects) / K
        self.right = max(r.right for r in rects) / K
        self.top = min(r.top for r in rects) / K
        self.bottom = max(r.bottom for r in rects) / K

        ink_layer = pygame.Surface((SIZE, SIZE), pygame.SRCALPHA)
        for p in ink:
            _draw_part(ink_layer, p, OPAQUE)
        ink_layer.blit(body, (0, 0), special_flags=pygame.BLEND_RGBA_MULT)  # limit the ink to the body
        self.ink = ink_layer
        self._hatch = {}

    def _row_extent(self, y):
        row = min(SIZE - 1, max(0, int(y * K)))
        xs = [x for x in range(SIZE) if self.mask.get_at((x, row))]
        return (xs[0] / K, xs[-1] / K) if xs else None

    @staticmethod
    def _coverage(light_lines, t):
        t = min(1.0, max(0.0, t))
        if light_lines:
            return 0.06 + 0.5 * (1 - t) ** 1.4
        return 0.06 + 0.55 * t ** 2.4 + 0.18 * (1 - t) ** 5.0

    def hatch(self, light_lines):
        """Mask surface of the hatch lines: heavy towards the right for light bodies, left for dark ones."""
        cached = self._hatch.get(light_lines)
        if cached is not None:
            return cached
        layer = pygame.Surface((SIZE, SIZE), pygame.SRCALPHA)
        steps = 12
        y0 = math.floor(self.top / self.PERIOD) * self.PERIOD
        while y0 < self.bottom:
            extent = self._row_extent(y0 + self.PERIOD / 2)
            if extent is not None and extent[1] - extent[0] >= 2.5:
                x0, x1 = extent[0] - 1, extent[1] + 1
                top, bottom = [], []
                for i in range(steps + 1):
                    t = i / steps
                    x = x0 + (x1 - x0) * t
                    top.append((x * K, y0 * K))
                    bottom.append((x * K, (y0 + self._coverage(light_lines, t) * self.PERIOD) * K))
                pygame.draw.polygon(layer, OPAQUE, top + bottom[::-1])
            y0 += self.PERIOD
        layer.blit(self.body, (0, 0), special_flags=pygame.BLEND_RGBA_MULT)
        self._hatch[light_lines] = layer
        return layer


_built = {}
_hi = {}
_scaled = {}


def _get_built(t):
    if t not in _built:
        _built[t] = _Built(t)
    return _built[t]


def _lerp(a, b, t):
    return tuple(round(a[i] + (b[i] - a[i]) * t) for i in range(3))


def _render_hi(t, color, ps: PieceSet):
    """The piece at 4x size with its outline: a SIZE x SIZE surface."""
    b = _get_built(t)
    white = color == WHITE
    a, c = (ps.white_a, ps.white_b) if white else (ps.black_a, ps.black_b)
    outline = ps.white_outline if white else ps.black_outline
    ink = ps.white_ink if white else ps.black_ink

    out = pygame.Surface((SIZE, SIZE), pygame.SRCALPHA)
    silhouette = b.mask.to_surface(setcolor=outline + (255,), unsetcolor=CLEAR)
    radius = 4.8 / 2 * K
    for i in range(24):
        ang = 2 * math.pi * i / 24
        out.blit(silhouette, (round(math.cos(ang) * radius), round(math.sin(ang) * radius)))

    fill = pygame.Surface((SIZE, SIZE), pygame.SRCALPHA)
    if ps.mode == GRADIENT:
        x0, x1 = b.left * K, b.right * K
        for x in range(SIZE):
            fill.fill(_lerp(a, c, min(1.0, max(0.0, (x - x0) / max(1.0, x1 - x0)))) + (255,), (x, 0, 1, SIZE))
    else:
        fill.fill(a + (255,))
    if ps.mode == HATCH:
        lines = b.hatch(not white)
        tint = pygame.Surface((SIZE, SIZE), pygame.SRCALPHA)
        tint.fill(c + (255,))
        tint.blit(lines, (0, 0), special_flags=pygame.BLEND_RGBA_MULT)
        fill.blit(tint, (0, 0))
    fill.blit(b.body, (0, 0), special_flags=pygame.BLEND_RGBA_MULT)
    out.blit(fill, (0, 0))

    ink_tint = pygame.Surface((SIZE, SIZE), pygame.SRCALPHA)
    ink_tint.fill(ink + (255,))
    ink_tint.blit(b.ink, (0, 0), special_flags=pygame.BLEND_RGBA_MULT)
    out.blit(ink_tint, (0, 0))
    return out


def piece_image(piece: int, cell: int, ps: PieceSet) -> pygame.Surface:
    """The piece scaled for a board cell `cell` pixels wide; blit it at the cell's top-left corner."""
    t, color = piece & 7, piece >> 3
    key = (t, color, ps.name, cell)
    img = _scaled.get(key)
    if img is not None:
        return img
    hi_key = (t, color, ps.name)
    if hi_key not in _hi:
        _hi[hi_key] = _render_hi(t, color, ps)
    inset = cell / 16
    box = max(1, round(cell - 2 * inset))
    img = pygame.Surface((cell, cell), pygame.SRCALPHA)
    img.blit(pygame.transform.smoothscale(_hi[hi_key], (box, box)), (round(inset), round(inset)))
    if len(_scaled) > 400:
        _scaled.clear()
    _scaled[key] = img
    return img


def draw_piece(target: pygame.Surface, piece: int, cell_rect, ps: PieceSet, opacity: float = 1.0) -> None:
    """Draws `piece` filling `cell_rect` in the rendering style of `ps`; `opacity` fades the whole piece."""
    if opacity <= 0 or not piece:
        return
    img = piece_image(piece, round(cell_rect.width), ps)
    if opacity < 1:
        img = img.copy()
        img.set_alpha(round(255 * opacity))
    target.blit(img, (round(cell_rect.x), round(cell_rect.y)))
