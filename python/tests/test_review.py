"""Review logic against docs/testdata/review.json, UCI parsing, a scripted fake engine, and (when installed) the
real Stockfish."""
import os
from collections import deque

import pytest
from conftest import ROOT, load_json

from chessapp.core import Board, Game
from chessapp.review import (
    EngineAnalyzer, EngineException, EngineScore, MoveLabel, OpeningBook, PlyInput, PositionEval, ReviewConfig,
    UciEngine, classify_move, find_engine_path, game_accuracy, move_accuracy, review_annotations, review_game,
    sacrificed_value, win_percent,
)
from chessapp.review.classify import EngineLine
from chessapp.review.uci import UciTransport, parse_best_move, parse_info

DATA = load_json("review.json")
TOL = 0.01


def test_win_percent():
    for c in DATA["winPercent"]:
        assert win_percent(EngineScore.from_json(c["score"])) == pytest.approx(c["expect"], abs=TOL)


def test_move_accuracy():
    for c in DATA["moveAccuracy"]:
        assert move_accuracy(c["loss"]) == pytest.approx(c["expect"], abs=TOL)


@pytest.mark.parametrize("case", DATA["classify"], ids=lambda c: c["name"])
def test_classify(case):
    config = ReviewConfig.from_json(DATA["config"])
    got = classify_move(PlyInput.from_json(case["input"]), config)
    exp = case["expect"]
    assert got.label.value == exp["label"]
    assert got.loss == pytest.approx(exp["loss"], abs=TOL)
    assert got.win_before == pytest.approx(exp["winBefore"], abs=TOL)
    assert got.win_after == pytest.approx(exp["winAfter"], abs=TOL)
    assert got.accuracy == pytest.approx(exp["accuracy"], abs=TOL)


def test_default_config_matches_shared_config():
    assert ReviewConfig.from_json(DATA["config"]) == ReviewConfig()


@pytest.mark.parametrize("case", DATA["sacrifice"], ids=lambda c: c["name"])
def test_sacrifice(case):
    assert sacrificed_value(Board.from_fen(case["fen"]), case["played"], case["pv"]) == case["expect"]


def test_game_accuracy_and_labels():
    case = DATA["game"][0]
    acc = {0: [], 1: []}
    for ply in case["plies"]:
        got = classify_move(PlyInput.from_json(ply["input"]))
        assert got.label.value == ply["expect"]["label"]
        acc[0 if ply["mover"] == "w" else 1].append(got.accuracy)
    assert game_accuracy(acc[0]) == pytest.approx(case["expect"]["accuracyWhite"], abs=TOL)
    assert game_accuracy(acc[1]) == pytest.approx(case["expect"]["accuracyBlack"], abs=TOL)
    assert game_accuracy([]) == 100


def test_labels_have_symbols_and_nags():
    assert MoveLabel.BRILLIANT.symbol == "!!" and MoveLabel.BRILLIANT.nag == 3
    assert MoveLabel.BLUNDER.symbol == "??" and MoveLabel.BLUNDER.nag == 4
    assert MoveLabel.BEST.symbol == "" and MoveLabel.BEST.nag is None


# --- UCI parsing -------------------------------------------------------------------------------------------

def test_parse_info_reads_score_and_pv():
    i = parse_info("info depth 16 seldepth 22 multipv 2 score cp -31 nodes 1000 nps 5000 pv e7e5 g1f3 b8c6")
    assert (i.depth, i.multipv, i.score, i.pv) == (16, 2, EngineScore(cp=-31), ("e7e5", "g1f3", "b8c6"))
    m = parse_info("info depth 5 multipv 1 score mate -2 pv d8h4")
    assert m.score == EngineScore(mate=-2)


@pytest.mark.parametrize("line", [
    "info depth 5 currmove e2e4 currmovenumber 1", "info string NNUE enabled", "readyok",
    "info depth 5 multipv 1 score cp 20 lowerbound pv e2e4", "info depth 5 score cp 20 nodes 5",
])
def test_parse_info_ignores_non_results(line):
    assert parse_info(line) is None


def test_parse_best_move():
    assert parse_best_move("bestmove e2e4 ponder e7e5") == "e2e4"
    assert parse_best_move("bestmove (none)") == "(none)"
    assert parse_best_move("info depth 1") is None


class FakeTransport(UciTransport):
    """Answers like an engine from a script: a dict of command prefix -> lines to print."""

    def __init__(self, replies):
        self.replies, self.sent, self.out = replies, [], deque()

    def send(self, line):
        self.sent.append(line)
        for prefix, lines in self.replies.items():
            if line.startswith(prefix):
                self.out.extend(lines)

    def readline(self, timeout):
        return self.out.popleft() if self.out else None

    def close(self):
        pass


def _engine(replies):
    t = FakeTransport({"uci": ["id name FakeFish 1", "uciok"], "isready": ["readyok"], **replies})
    e = UciEngine(t)
    e.start(timeout=1)
    return e, t


def test_handshake_reads_name():
    e, _ = _engine({})
    assert e.name == "FakeFish 1"


def test_analyze_keeps_deepest_line_per_multipv():
    e, t = _engine({"go": [
        "info depth 1 multipv 1 score cp 10 pv e2e4",
        "info depth 2 multipv 1 score cp 30 pv e2e4 e7e5",
        "info depth 2 multipv 2 score cp 20 pv d2d4 d7d5",
        "bestmove e2e4",
    ]})
    lines = e.analyze("startpos-fen", depth=2, multi_pv=2)
    assert [(ln.move, ln.score) for ln in lines] == [("e2e4", EngineScore(cp=30)), ("d2d4", EngineScore(cp=20))]
    assert "go depth 2" in t.sent and "setoption name MultiPV value 2" in t.sent


def test_analyze_no_legal_move_is_empty():
    e, _ = _engine({"go": ["bestmove (none)"]})
    assert e.analyze("x") == []


def test_silent_engine_times_out():
    e, _ = _engine({})
    with pytest.raises(EngineException):
        e.analyze("x", timeout=0.3)


def test_analyzer_caches_by_fen():
    e, t = _engine({"go": ["info depth 1 multipv 1 score cp 10 pv e2e4", "bestmove e2e4"]})
    a = EngineAnalyzer(e, depth=1, multi_pv=1)
    a.evaluate("same")
    a.evaluate("same")
    assert sum(1 for s in t.sent if s.startswith("go")) == 1


# --- review of a whole game from scripted numbers ------------------------------------------------------------

def _fools_mate_game():
    g = Game()
    for u in ("f2f3", "e7e5", "g2g4", "d8h4"):
        assert g.make_move_uci(u)
    return g


def _fools_mate_evals():
    L = lambda m, **s: EngineLine(m, EngineScore(**s), (m,))  # noqa: E731
    return [
        PositionEval([L("e2e4", cp=30), L("d2d4", cp=28), L("g1f3", cp=25)]),
        PositionEval([L("e7e5", cp=120), L("d7d5", cp=100), L("g8f6", cp=95)]),
        PositionEval([L("e2e4", cp=-100), L("d2d4", cp=-105), L("g1h3", cp=-110)]),
        PositionEval([L("d8h4", mate=1), L("h7h5", cp=374), L("d7d5", cp=230)]),
        PositionEval([]),
    ]


def test_review_game_fools_mate():
    r = review_game(_fools_mate_game(), _fools_mate_evals())
    assert [p.label.value for p in r.plies] == DATA["game"][0]["expect"]["labels"]
    # One search per position means ply 2's "after" score is ply 3's "before" score (-100 here, -110 in the shared
    # per-ply case), so only White's accuracy is exactly the shared number.
    assert r.accuracy_white == pytest.approx(DATA["game"][0]["expect"]["accuracyWhite"], abs=TOL)
    assert r.accuracy_black == pytest.approx(DATA["game"][0]["expect"]["accuracyBlack"], abs=3)
    assert len(r.white_win_chance) == 5 and r.white_win_chance[-1] == 0  # Black mated White
    assert r.plies[2].has_better_move and r.plies[2].best_san == "e4"
    notes = review_annotations(r)
    assert notes[2].nag == 4 and "Blunder. Best: e4" in notes[2].comment
    assert notes[3].nag is None and notes[3].comment == "Best"  # mate ends the game: no evaluation to show


def test_book_move_labelled_and_opening_named():
    book = OpeningBook.parse("eco\tname\tpgn\nB00\tKing's Pawn\t1. e4\nC20\tKing's Pawn Game\t1. e4 e5\n")
    g = Game()
    for u in ("e2e4", "e7e5"):
        g.make_move_uci(u)
    L = lambda m, c: EngineLine(m, EngineScore(cp=c), (m,))  # noqa: E731
    evals = [PositionEval([L("e2e4", 30), L("d2d4", 28)]), PositionEval([L("e7e5", 30), L("c7c5", 28)]),
             PositionEval([L("g1f3", 30), L("f2f4", 10)])]
    r = review_game(g, evals, book=book)
    assert [p.label for p in r.plies] == [MoveLabel.BOOK, MoveLabel.BOOK]
    assert (r.opening, r.opening_eco) == ("King's Pawn Game", "C20")


def test_shared_opening_book_loads():
    book = OpeningBook.load(ROOT.parent / "docs" / "data" / "openings.tsv")
    assert book.size > 1000
    g = Game()
    for san in ("e4", "e5", "Nf3", "Nc6", "Bb5"):
        assert g.make_move_san(san)
    assert book.contains(g.board)
    assert "Ruy Lopez" in book.name_of(g.board).name


# --- real Stockfish (skipped unless engine/stockfish.exe is installed) -------------------------------------------

needs_engine = pytest.mark.skipif(find_engine_path() is None, reason="Stockfish not installed (tools/get_stockfish.ps1)")


def _game_from_san(moves):
    g = Game()
    for san in moves:
        assert g.make_move_san(san), san
    return g


@needs_engine
def test_real_stockfish_fools_mate():
    from chessapp.review import open_engine
    engine = open_engine()
    try:
        assert engine.name.startswith("Stockfish")
        a = EngineAnalyzer(engine, depth=10)
        a.configure()
        case = DATA["engineGames"][0]
        r = a.review(_game_from_san(case["san"]))
        for exp in case["expect"]:
            assert r.plies[exp["ply"] - 1].label.value in exp["oneOf"]
    finally:
        engine.dispose()


@needs_engine
@pytest.mark.skipif(not os.environ.get("CHESS_SLOW_TESTS"), reason="about a minute; set CHESS_SLOW_TESTS=1")
def test_real_stockfish_opera_game():
    from chessapp.review import open_engine
    engine = open_engine()
    try:
        a = EngineAnalyzer(engine, depth=16)
        a.configure()
        case = DATA["engineGames"][1]
        r = a.review(_game_from_san(case["san"]))
        for exp in case["expect"]:
            assert r.plies[exp["ply"] - 1].label.value in exp["oneOf"]
    finally:
        engine.dispose()
