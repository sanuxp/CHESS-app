"""Customization data and state, see docs/THEMES.md. Names, colours and defaults are fixed by that spec."""
from __future__ import annotations

import json
import os
import sys
from dataclasses import dataclass
from pathlib import Path

Color = tuple


def rgb(hex_string: str) -> tuple:
    h = hex_string.lstrip("#")
    return int(h[0:2], 16), int(h[2:4], 16), int(h[4:6], 16)


@dataclass(frozen=True)
class BoardTheme:
    name: str
    light: Color
    dark: Color


BOARD_THEMES = [BoardTheme(n, rgb(a), rgb(b)) for n, a, b in (
    ("Brown", "#F0D9B5", "#B58863"), ("Green", "#EEEED2", "#769656"), ("Blue", "#DEE3E6", "#8CA2AD"),
    ("Purple", "#EFEFEF", "#8C77B5"), ("Slate", "#DCDCDC", "#828282"), ("Ice", "#E8F0F7", "#7898BA"),
    ("Coral", "#F5E1D7", "#C46E5F"), ("Walnut", "#C8AA82", "#6E4B32"),
)]

HATCH, FLAT, GRADIENT = "hatch", "flat", "gradient"


@dataclass(frozen=True)
class PieceSet:
    name: str
    mode: str
    white_a: Color
    white_b: Color
    black_a: Color
    black_b: Color
    white_outline: Color
    black_outline: Color
    white_ink: Color
    black_ink: Color


def _set(name, mode, wa, wb, ba, bb, wo, bo, wi, bi):
    return PieceSet(name, mode, rgb(wa), rgb(wb), rgb(ba), rgb(bb), rgb(wo), rgb(bo), rgb(wi), rgb(bi))


PIECE_SETS = [
    _set("Engraved", HATCH, "#FAF8F2", "#1E1C1C", "#1A1A1E", "#D6D6DC", "#161414", "#08080A", "#161414", "#D6D6DC"),
    _set("Classic", FLAT, "#F8F6EE", "#F8F6EE", "#4A4C56", "#4A4C56", "#26221F", "#0E0E12", "#463F3A", "#C8C8D2"),
    _set("Shaded", GRADIENT, "#FFFFFF", "#B9B4A8", "#5A5C68", "#101014", "#26221F", "#08080A", "#6A655A", "#C8C8D2"),
    _set("Wood", GRADIENT, "#EBC98F", "#B98A4E", "#7A4A2A", "#3A2012", "#4A2E14", "#1E0F06", "#4A2E14", "#D9B98A"),
    _set("Royal", GRADIENT, "#FBF1D0", "#D2A94A", "#3A4F8F", "#141F44", "#6B5314", "#0A1230", "#6B5314", "#AAB8E8"),
]


@dataclass(frozen=True)
class UiPalette:
    name: str
    dark: bool
    background: Color
    panel: Color
    text: Color
    muted: Color
    button: Color
    button_hover: Color
    button_disabled: Color
    button_text: Color


def _palette(name, dark, *colors):
    return UiPalette(name, dark, *[rgb(c) for c in colors])


UI_PALETTES = [
    _palette("Dark", True, "#1E1E22", "#2C2C34", "#EBEBF0", "#9696A5", "#465270", "#5C6E96", "#34343C", "#EBEBF0"),
    _palette("Midnight", True, "#0B1020", "#141C33", "#E3E9FA", "#8091B8", "#2B4A8C", "#3C62B3", "#1B2640", "#E3E9FA"),
    _palette("Light", False, "#ECEBE8", "#FFFFFF", "#22252B", "#6A6F7A", "#D5DAE5", "#C0C9DB", "#EBEDF1", "#22252B"),
    _palette("Sepia", True, "#2B2118", "#3A2D20", "#F1E3CC", "#B69F80", "#7A5A38", "#946F45", "#4A3A2A", "#F6EBD8"),
]

# Fixed colours (not themeable): (r, g, b, alpha)
LAST_MOVE = (205, 210, 106, 120)
SELECTION = (246, 246, 105, 150)
CHECK_DISC = (220, 50, 50, 150)
HINT = (40, 40, 40, 102)


def _by_name(options, name):
    for o in options:
        if o.name == name:
            return o
    return options[0]


class Settings:
    """What the Themes view changes. Stored by option name, falling back to the default when unknown."""

    def __init__(self, path: "Path | None" = None):
        self.path = path
        self.board = BOARD_THEMES[0]
        self.pieces = PIECE_SETS[0]
        self.palette = UI_PALETTES[0]
        self.coordinates = True
        self.hints = True
        self.animations = True
        self.depth = 16  # review depth: Quick 12, Standard 16, Deep 20

    def reset(self) -> None:
        keep_path, keep_depth = self.path, self.depth
        self.__init__(keep_path)
        self.depth = keep_depth

    def to_dict(self) -> dict:
        return {"board": self.board.name, "pieces": self.pieces.name, "interface": self.palette.name,
                "coordinates": self.coordinates, "hints": self.hints, "animations": self.animations,
                "depth": self.depth}

    def load_dict(self, d: dict) -> None:
        self.board = _by_name(BOARD_THEMES, d.get("board"))
        self.pieces = _by_name(PIECE_SETS, d.get("pieces"))
        self.palette = _by_name(UI_PALETTES, d.get("interface"))
        for key in ("coordinates", "hints", "animations"):
            if isinstance(d.get(key), bool):
                setattr(self, key, d[key])
        if d.get("depth") in (12, 16, 20):
            self.depth = d["depth"]

    def load(self) -> None:
        try:
            self.load_dict(json.loads(self.path.read_text(encoding="utf-8")))
        except (OSError, ValueError, AttributeError):
            pass

    def save(self) -> None:
        if self.path is None:
            return
        try:
            self.path.parent.mkdir(parents=True, exist_ok=True)
            self.path.write_text(json.dumps(self.to_dict(), indent=1), encoding="utf-8")
        except OSError:
            pass


def default_settings_path() -> Path:
    """Per-user settings file: %APPDATA% on Windows, ~/.config elsewhere."""
    if sys.platform == "win32":
        base = Path(os.environ.get("APPDATA", Path.home()))
    else:
        base = Path(os.environ.get("XDG_CONFIG_HOME", Path.home() / ".config"))
    return base / "chess_python" / "settings.json"
