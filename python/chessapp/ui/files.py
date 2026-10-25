"""System dialogs and clipboard. Tkinter ships with Python, so no extra dependency; every call degrades to a
"not available" result instead of raising (a headless test run has no dialogs)."""
from __future__ import annotations

import datetime
from pathlib import Path


def default_pgn_name() -> str:
    """chess_YYYY-MM-DD_HHMM.pgn"""
    return datetime.datetime.now().strftime("chess_%Y-%m-%d_%H%M.pgn")


def _tk():
    import tkinter
    root = tkinter.Tk()
    root.withdraw()
    root.attributes("-topmost", True)
    return root


def ask_save_pgn(default_name: "str | None" = None) -> "str | None":
    """Opens the system save dialog; returns the chosen path or None."""
    try:
        from tkinter import filedialog
        root = _tk()
        try:
            path = filedialog.asksaveasfilename(
                title="Save game", defaultextension=".pgn", initialfile=default_name or default_pgn_name(),
                filetypes=[("PGN game", "*.pgn"), ("All files", "*.*")])
        finally:
            root.destroy()
        return path or None
    except Exception:
        return None


def ask_open_pgn() -> "str | None":
    """Opens the system file dialog for a PGN; returns the chosen path or None."""
    try:
        from tkinter import filedialog
        root = _tk()
        try:
            path = filedialog.askopenfilename(
                title="Open a PGN game", filetypes=[("PGN game", "*.pgn"), ("All files", "*.*")])
        finally:
            root.destroy()
        return path or None
    except Exception:
        return None


def write_text(path: str, text: str) -> bool:
    try:
        Path(path).write_text(text, encoding="utf-8", newline="\n")
        return True
    except OSError:
        return False


def read_text(path: str) -> "str | None":
    try:
        return Path(path).read_text(encoding="utf-8", errors="replace")
    except OSError:
        return None


def copy_to_clipboard(text: str) -> bool:
    """Puts text on the system clipboard; False when that is not possible."""
    try:
        import pygame
        if not pygame.scrap.get_init():
            pygame.scrap.init()
        pygame.scrap.put_text(text)
        return True
    except Exception:
        pass
    try:
        root = _tk()
        root.clipboard_clear()
        root.clipboard_append(text)
        root.update()
        root.destroy()
        return True
    except Exception:
        return False
