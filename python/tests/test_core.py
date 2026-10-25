"""Rules tests driven by the shared data in docs/testdata (perft, scenarios, pgn)."""
import os

import pytest
from conftest import load_json

from chessapp.core import (
    START_FEN, Board, Game, PgnHeaders, generate_legal, import_pgn, move_from_san, move_to_san, perft, to_pgn,
)

# Pure Python is far slower than C++: stay below this many leaf nodes unless CHESS_PERFT_MAX is raised.
PERFT_MAX = int(os.environ.get("CHESS_PERFT_MAX", "200000"))


def _fen(value):
    return START_FEN if value == "startpos" else value


def _perft_params():
    return [pytest.param(pos["fen"], int(depth), nodes, id=f"{pos['name']} d{depth}")
            for pos in load_json("perft.json")["positions"]
            for depth, nodes in pos["nodes"].items() if nodes <= PERFT_MAX]


@pytest.mark.parametrize("fen,depth,expected", _perft_params())
def test_perft(fen, depth, expected):
    assert perft(Board.from_fen(fen), depth) == expected


def test_fen_round_trip():
    for pos in load_json("perft.json")["positions"]:
        assert Board.from_fen(pos["fen"]).to_fen() == pos["fen"]


@pytest.mark.parametrize("bad", ["", "8/8/8/8/8/8/8/8 w - - 0 1", "rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP w KQkq - 0 1",
                                 "4k3/8/8/8/8/8/8/4K3 x - - 0 1", "4k3/8/8/8/8/8/8/4K3 w X - 0 1",
                                 "P3k3/8/8/8/8/8/8/4K3 w - - 0 1"])
def test_bad_fen_rejected(bad):
    assert Board.from_fen(bad) is None


@pytest.mark.parametrize("sc", load_json("scenarios.json")["scenarios"], ids=lambda s: s["name"])
def test_scenario(sc):
    game = Game.from_fen(_fen(sc["fen"]))
    for uci in sc["moves"]:
        assert game.make_move_uci(uci), f"illegal move {uci}"
    exp = sc["expect"]
    assert game.status.data_name == exp["status"]
    if "result" in exp:
        assert game.result_token == exp["result"]
    ucis = {m.uci for m in game.legal_moves}
    if "legalCount" in exp:
        assert len(ucis) == exp["legalCount"]
    for u in exp.get("legalContains", []):
        assert u in ucis
    for u in exp.get("legalNotContains", []):
        assert u not in ucis
    if "fen" in exp:
        assert game.board.to_fen() == exp["fen"]


@pytest.mark.parametrize("tc", load_json("pgn.json")["san_cases"], ids=lambda c: c["name"])
def test_san(tc):
    game = Game.from_fen(_fen(tc["fen"]))
    for uci in tc["moves"]:
        assert game.make_move_uci(uci)
    assert [h.san for h in game.history] == tc["san"]
    # every SAN must parse back to the same move
    for entry in game.history:
        assert move_from_san(entry.before, entry.san) == entry.move
        assert move_to_san(entry.before, entry.move) == entry.san


@pytest.mark.parametrize("tc", load_json("pgn.json")["pgn_cases"], ids=lambda c: c["name"])
def test_pgn_export(tc):
    game = Game.from_fen(_fen(tc["fen"]))
    for uci in tc["moves"]:
        assert game.make_move_uci(uci)
    assert to_pgn(game, PgnHeaders()) == tc["pgn"]


@pytest.mark.parametrize("tc", load_json("pgn.json")["pgn_import_cases"], ids=lambda c: c["name"])
def test_pgn_import(tc):
    res = import_pgn(tc["pgn"])
    assert [h.san for h in res.game.history] == tc["san"]
    assert res.result == tc["result"]
    for k, v in tc.get("headers", {}).items():
        assert res.headers[k] == v
    if "startFen" in tc:
        assert res.game.initial.to_fen() == tc["startFen"]
    assert (res.error is not None) == bool(tc.get("error"))


def test_undo_restores_everything():
    game = Game()
    for u in ("e2e4", "e7e5", "g1f3"):
        assert game.make_move_uci(u)
    assert game.undo() and game.undo() and game.undo()
    assert game.board.to_fen() == START_FEN
    assert not game.undo()


def test_illegal_move_leaves_state():
    game = Game()
    assert not game.make_move_uci("e2e5")
    assert game.move_count == 0


def test_promotion_choices_are_separate_moves():
    game = Game.from_fen("4k3/P7/8/8/8/8/8/4K3 w - - 0 1")
    promos = sorted(m.uci for m in generate_legal(game.board) if m.from_sq == 48)
    assert promos == ["a7a8b", "a7a8n", "a7a8q", "a7a8r"]


def test_threefold_repetition_is_automatic():
    game = Game()
    for u in ("g1f3", "g8f6", "f3g1", "f6g8") * 2:
        game.make_move_uci(u)
    assert game.status.data_name == "DrawRepetition"
