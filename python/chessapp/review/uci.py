"""UCI engine support: parsing engine output, a line-based transport, and fixed-depth multi-line analysis."""
from __future__ import annotations

import os
import queue
import shutil
import subprocess
import sys
import threading
import time
from dataclasses import dataclass
from pathlib import Path

from .classify import EngineLine, EngineScore


@dataclass(frozen=True)
class UciInfo:
    """One `info ... pv ...` line; scores are from the side to move's point of view."""
    depth: int
    multipv: int  # 1 = best line
    score: EngineScore
    pv: tuple  # UCI moves


def parse_info(line: str) -> "UciInfo | None":
    """Parses an `info` line. Returns None for lines without a score and a variation (currmove, string, ...)
    and for aspiration-window bound lines (lowerbound/upperbound), which are not real results."""
    if not line.startswith("info "):
        return None
    t = line.split()
    depth, multipv, score, pv = 0, 1, None, ()
    i = 1
    while i < len(t):
        word = t[i]
        if word == "depth" and i + 1 < len(t):
            i += 1
            depth = int(t[i]) if t[i].lstrip("-").isdigit() else depth
        elif word == "multipv" and i + 1 < len(t):
            i += 1
            multipv = int(t[i]) if t[i].lstrip("-").isdigit() else multipv
        elif word == "score" and i + 2 < len(t):
            kind, raw = t[i + 1], t[i + 2]
            i += 2
            if not raw.lstrip("-").isdigit():
                return None
            score = EngineScore(mate=int(raw)) if kind == "mate" else EngineScore(cp=int(raw))
            if i + 1 < len(t) and t[i + 1] in ("lowerbound", "upperbound"):
                return None
        elif word == "pv":
            pv = tuple(t[i + 1:])
            break
        i += 1
    if score is None or not pv:
        return None
    return UciInfo(depth, multipv, score, pv)


def parse_best_move(line: str) -> "str | None":
    """The move in a `bestmove` line ("(none)" when there is no legal move), or None for other lines."""
    if not line.startswith("bestmove"):
        return None
    t = line.split()
    return t[1] if len(t) > 1 else "(none)"


class EngineException(Exception):
    pass


class UciTransport:
    """A line-based connection to a UCI engine: a child process, or a script in tests."""

    def readline(self, timeout: float) -> "str | None":
        """The next line the engine printed, or None when none arrived within `timeout` seconds."""
        raise NotImplementedError

    def send(self, line: str) -> None:
        raise NotImplementedError

    def close(self) -> None:
        raise NotImplementedError


class ProcessTransport(UciTransport):
    def __init__(self, path: str):
        flags = getattr(subprocess, "CREATE_NO_WINDOW", 0)
        self._proc = subprocess.Popen([path], stdin=subprocess.PIPE, stdout=subprocess.PIPE,
                                      stderr=subprocess.DEVNULL, text=True, bufsize=1, creationflags=flags)
        self._lines: queue.Queue = queue.Queue()
        threading.Thread(target=self._pump, daemon=True).start()

    def _pump(self) -> None:
        for line in self._proc.stdout:
            self._lines.put(line.rstrip("\r\n"))
        self._lines.put(None)  # engine exited

    def readline(self, timeout: float) -> "str | None":
        try:
            line = self._lines.get(timeout=timeout)
        except queue.Empty:
            return None
        if line is None:
            self._lines.put(None)
            raise EngineException("The engine stopped unexpectedly.")
        return line

    def send(self, line: str) -> None:
        try:
            self._proc.stdin.write(line + "\n")
            self._proc.stdin.flush()
        except OSError as e:
            raise EngineException("The engine stopped unexpectedly.") from e

    def close(self) -> None:
        try:
            self._proc.kill()
        except OSError:
            pass


class UciEngine:
    """Talks UCI over a transport: handshake, options and fixed-depth multi-line analysis."""

    def __init__(self, transport: UciTransport):
        self._t = transport
        self._multipv = 1
        self.name = ""

    def start(self, timeout: float = 20) -> None:
        """Performs the `uci` / `isready` handshake."""
        self._ask("uci", "uciok", timeout)
        self._ask("isready", "readyok", timeout)

    def _ask(self, command: str, until: str, timeout: float) -> None:
        self._t.send(command)
        for line in self._lines(timeout, f'The engine did not answer "{command}" within {int(timeout)} s.'):
            if line.startswith("id name "):
                self.name = line[8:].strip()
            if line.strip() == until:
                return

    def _lines(self, timeout: float, message: str):
        deadline = time.monotonic() + timeout
        while True:
            left = deadline - time.monotonic()
            if left <= 0:
                raise EngineException(message)
            line = self._t.readline(min(left, 0.25))
            if line is not None:
                yield line

    def set_option(self, name: str, value) -> None:
        """Sets an engine option such as Threads or Hash (no answer is expected)."""
        self._t.send(f"setoption name {name} value {value}")

    def stop(self) -> None:
        """Asks a running search to stop (its `bestmove` still arrives)."""
        self._t.send("stop")

    def analyze(self, fen: str, depth: int = 14, multi_pv: int = 3, timeout: float = 120,
                is_cancelled=None) -> list:
        """Best lines for `fen`, best first, scores from the side to move's view. Empty if there is no legal move.

        `is_cancelled`, if given, is polled about four times a second; when it returns True the search is stopped
        and EngineException("cancelled") is raised.
        """
        if multi_pv != self._multipv:
            self.set_option("MultiPV", multi_pv)
            self._multipv = multi_pv
        best: dict = {}  # multipv index -> deepest info seen
        self._t.send(f"position fen {fen}")
        self._t.send(f"go depth {depth}")
        message = f"The engine took longer than {int(timeout)} s for one position."
        try:
            for line in self._lines(timeout, message):
                if is_cancelled is not None and is_cancelled():
                    raise EngineException("cancelled")
                info = parse_info(line)
                if info is not None:
                    old = best.get(info.multipv)
                    if old is None or info.depth >= old.depth:
                        best[info.multipv] = info
                    continue
                move = parse_best_move(line)
                if move is not None:
                    if move == "(none)":
                        return []
                    return [EngineLine(best[k].pv[0], best[k].score, best[k].pv) for k in sorted(best)]
        except EngineException:
            self._t.send("stop")
            raise
        return []

    def dispose(self) -> None:
        try:
            self._t.send("quit")
        except EngineException:
            pass
        self._t.close()


def _exe_name() -> str:
    return "stockfish.exe" if sys.platform == "win32" else "stockfish"


def engine_search_hint() -> str:
    return ("Stockfish was not found. Run tools\\get_stockfish.ps1 (it installs engine\\stockfish.exe), "
            "put a Stockfish program in an \"engine\" folder next to the app, or set CHESS_STOCKFISH.")


def find_engine_path(custom_path: "str | None" = None) -> "str | None":
    """Looks for Stockfish: the chosen path, then `engine/` in this folder and its parents (so a development
    checkout finds the repository's `engine/`), then the CHESS_STOCKFISH variable, then PATH."""
    if custom_path and os.path.isfile(custom_path):
        return custom_path
    here = Path(__file__).resolve()
    for folder in [Path.cwd(), *here.parents]:
        candidate = folder / "engine" / _exe_name()
        if candidate.is_file():
            return str(candidate)
    env = os.environ.get("CHESS_STOCKFISH")
    if env and os.path.isfile(env):
        return env
    return shutil.which("stockfish")


def open_engine(custom_path: "str | None" = None) -> "UciEngine | None":
    """Starts Stockfish as a child process and does the handshake; None when it cannot be found."""
    path = find_engine_path(custom_path)
    if path is None:
        return None
    engine = UciEngine(ProcessTransport(path))
    try:
        engine.start()
    except EngineException:
        engine.dispose()
        raise
    return engine
