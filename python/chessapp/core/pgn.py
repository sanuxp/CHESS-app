"""PGN export and import."""
from __future__ import annotations

import re
from dataclasses import dataclass, field

from .board import Board
from .game import Game
from .types import BLACK, WHITE


@dataclass
class PgnHeaders:
    event: str = "Casual Game"
    site: str = "Local"
    date: str = "????.??.??"  # YYYY.MM.DD
    round: str = "-"
    white: str = "White"
    black: str = "Black"


@dataclass
class PlyAnnotation:
    """Extra information written after a move: a numeric annotation glyph ($1 !, $2 ?, ...) and a comment."""
    nag: "int | None" = None
    comment: "str | None" = None


def to_pgn(game: Game, headers: "PgnHeaders | None" = None, annotations: "list | None" = None) -> str:
    """Full PGN text: seven tag roster, SetUp/FEN for custom starts, movetext wrapped at 80 columns, result.

    `annotations`, if given, has one entry per move (for example a game review's labels and evaluations).
    """
    headers = headers or PgnHeaders()
    out = []
    result = game.result_token
    for name, value in (("Event", headers.event), ("Site", headers.site), ("Date", headers.date),
                        ("Round", headers.round), ("White", headers.white), ("Black", headers.black),
                        ("Result", result)):
        out.append(f'[{name} "{value}"]\n')

    start = game.initial
    if start.to_fen() != Board.start().to_fen():
        out.append('[SetUp "1"]\n')
        out.append(f'[FEN "{start.to_fen()}"]\n')
    out.append("\n")

    tokens = []
    move_number = start.fullmove_number
    side = start.side_to_move
    first = True
    for i, entry in enumerate(game.history):
        if side == WHITE:
            tokens.append(f"{move_number}.")
        elif first:
            tokens.append(f"{move_number}...")
        tokens.append(entry.san)
        note = annotations[i] if annotations is not None and i < len(annotations) else None
        if note is not None and note.nag is not None:
            tokens.append(f"${note.nag}")
        words = [w for w in re.sub(r"[{}]", "", note.comment).split() if w] if note and note.comment else []
        for w, word in enumerate(words):
            tokens.append(("{" if w == 0 else "") + word + ("}" if w == len(words) - 1 else ""))
        if side == BLACK:
            move_number += 1
        side ^= 1
        first = False
    tokens.append(result)

    column = 0
    for t in tokens:
        if column > 0 and column + 1 + len(t) > 80:
            out.append("\n")
            column = 0
        if column > 0:
            out.append(" ")
            column += 1
        out.append(t)
        column += len(t)
    out.append("\n")
    return "".join(out)


@dataclass
class PgnImport:
    """The result of reading PGN text: the game, its tags, the result token and an error if a move was illegal."""
    game: Game
    headers: dict = field(default_factory=dict)
    result: str = "*"  # "1-0", "0-1", "1/2-1/2" or "*"
    error: "str | None" = None  # set when reading stopped early (the moves before it are still in `game`)


_TAG = re.compile(r'^\s*\[\s*(\w+)\s+"((?:[^"\\]|\\.)*)"\s*\]\s*$')
_MOVE_NUMBER = re.compile(r"^\d+\.+")
_RESULTS = {"1-0", "0-1", "1/2-1/2", "*"}


def import_pgn(text: str) -> PgnImport:
    """Reads the first game in `text`. Comments, variations, NAGs and engine annotations are ignored."""
    headers: dict = {}
    body = []
    in_headers = True
    for line in re.split(r"\r?\n", text):
        if in_headers:
            m = _TAG.match(line)
            if m:
                headers[m.group(1)] = m.group(2).replace('\\"', '"').replace("\\\\", "\\")
                continue
            if not line.strip():
                continue
            in_headers = False
        body.append(line)

    start = Board.start()
    fen = headers.get("FEN")
    if fen is not None:
        b = Board.from_fen(fen)
        if b is None:
            return PgnImport(Game(), headers, "*", "Invalid FEN in the PGN tags")
        start = b
    game = Game(start)

    result = headers.get("Result", "*")
    if result not in _RESULTS:
        result = "*"
    error = None

    s = "\n".join(body) + "\n"
    state = {"variation": 0, "move_no": start.fullmove_number}
    token: list = []

    def flush() -> bool:
        """Returns False when reading must stop (result token or illegal move)."""
        nonlocal result, error
        if not token:
            return True
        t = "".join(token)
        token.clear()
        if state["variation"] > 0:
            return True
        if t in _RESULTS:
            result = t
            return False
        if t.startswith("$"):
            return True  # NAG
        t = _MOVE_NUMBER.sub("", t, count=1)
        if not t:
            return True
        if not game.make_move_san(t):
            error = f'Illegal or unreadable move "{t}" (move {state["move_no"]})'
            return False
        if game.board.side_to_move == WHITE:
            state["move_no"] += 1
        return True

    i = 0
    stopped = False
    while i < len(s):
        ch = s[i]
        if ch == "{":
            if not flush():
                stopped = True
                break
            end = s.find("}", i)
            i = len(s) if end < 0 else end + 1
        elif ch == ";":
            if not flush():
                stopped = True
                break
            end = s.find("\n", i)
            i = len(s) if end < 0 else end + 1
        elif ch == "(":
            if not flush():
                stopped = True
                break
            state["variation"] += 1
            i += 1
        elif ch == ")":
            if not flush():
                stopped = True
                break
            if state["variation"] > 0:
                state["variation"] -= 1
            i += 1
        elif ch in " \t\n\r":
            if not flush():
                stopped = True
                break
            i += 1
        else:
            token.append(ch)
            i += 1
    if not stopped and error is None:
        flush()

    return PgnImport(game, headers, result, error)
