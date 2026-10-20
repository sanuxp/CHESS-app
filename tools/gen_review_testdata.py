#!/usr/bin/env python3
"""Reference implementation of the review rules in docs/REVIEW.md and generator of docs/testdata/review.json.

    python tools/gen_review_testdata.py

Every case below states the label it is meant to demonstrate and the script asserts that the reference
implementation agrees, so the JSON cannot silently drift from the intent. Dart and C++ then check they
reproduce the same numbers.
"""
import json
import math
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent

CONFIG = {
    "brilliantMaxLoss": 2, "brilliantMinSacrifice": 2, "brilliantMaxWinBefore": 80, "brilliantMinWinAfter": 45,
    "brilliantMateMinSacrifice": 5,
    "greatMaxLoss": 1, "greatMinGap": 15, "bookMaxLoss": 10,
    "bestMaxLoss": 0.5, "excellentMaxLoss": 2, "goodMaxLoss": 5,
    "missOpponentLoss": 10, "missMinWinBefore": 60, "missMinLoss": 5,
    "inaccuracyMaxLoss": 10, "mistakeMaxLoss": 20,
}


def win_percent(score):
    if "mate" in score:
        return 100.0 if score["mate"] > 0 else 0.0
    cp = max(-1000, min(1000, score["cp"]))
    return 50 + 50 * (2 / (1 + math.exp(-0.00368208 * cp)) - 1)


def move_accuracy(loss):
    return max(0.0, min(100.0, 103.1668 * math.exp(-0.04354 * loss) - 3.1669))


def classify(inp, c=CONFIG):
    lines = inp["linesBefore"]
    win_before = win_percent(lines[0]["score"])
    if inp.get("afterResult") == "checkmate":
        win_after = 100.0
    elif inp.get("afterResult") == "stalemate":
        win_after = 50.0
    else:
        win_after = 100 - win_percent(inp["afterBest"]["score"])
    loss = max(0.0, win_before - win_after)
    played_top = inp["played"] == lines[0]["move"]
    prev = inp.get("opponentPrevLoss")

    if inp["legalMoves"] == 1:
        label = "forced"
    elif inp["book"] and loss <= c["bookMaxLoss"]:
        label = "book"
    elif loss <= c["brilliantMaxLoss"] and (
            (inp["sacrificed"] >= c["brilliantMinSacrifice"]
             and win_before <= c["brilliantMaxWinBefore"] and win_after >= c["brilliantMinWinAfter"])
            or (inp["sacrificed"] >= c["brilliantMateMinSacrifice"]
                and lines[0]["score"].get("mate", 0) > 0 and inp.get("afterResult") != "checkmate")):
        label = "brilliant"
    elif played_top and loss <= c["greatMaxLoss"] and len(lines) >= 2 \
            and win_percent(lines[0]["score"]) - win_percent(lines[1]["score"]) >= c["greatMinGap"] \
            and not inp["recapture"] and inp.get("afterResult") != "checkmate":
        label = "great"
    elif played_top or loss <= c["bestMaxLoss"]:
        label = "best"
    elif loss <= c["excellentMaxLoss"]:
        label = "excellent"
    elif loss <= c["goodMaxLoss"]:
        label = "good"
    elif prev is not None and prev > c["missOpponentLoss"] and win_before >= c["missMinWinBefore"] \
            and loss >= c["missMinLoss"] and not (loss > c["mistakeMaxLoss"] and win_after < 50):
        label = "miss"
    elif loss <= c["inaccuracyMaxLoss"]:
        label = "inaccuracy"
    elif loss <= c["mistakeMaxLoss"]:
        label = "mistake"
    else:
        label = "blunder"
    return {"label": label, "loss": loss, "winBefore": win_before, "winAfter": win_after,
            "accuracy": move_accuracy(loss)}


def game_accuracy(values):
    if not values:
        return 100.0
    arith = sum(values) / len(values)
    harm = len(values) / sum(1 / max(v, 0.01) for v in values)
    return (arith + harm) / 2


PIECE_VALUE = {"p": 1, "n": 3, "b": 3, "r": 5, "q": 9, "k": 0}


def material(board, color):
    return sum(PIECE_VALUE[p.lower()] for p in board.values() if (p.isupper()) == (color == "w"))


def parse_placement(fen):
    board = {}
    for r, row in enumerate(fen.split()[0].split("/")):
        f = 0
        for ch in row:
            if ch.isdigit():
                f += int(ch)
            else:
                board[(f, 7 - r)] = ch
                f += 1
    return board


def sq(name):
    return (ord(name[0]) - 97, int(name[1]) - 1)


def apply_uci(board, uci):
    frm, to = sq(uci[0:2]), sq(uci[2:4])
    piece = board.pop(frm)
    board.pop(to, None)
    if len(uci) == 5:
        piece = uci[4].upper() if piece.isupper() else uci[4]
    board[to] = piece


def sacrificed(fen, played, pv):
    board = parse_placement(fen)
    mover = "w" if fen.split()[1] == "w" else "b"
    other = "b" if mover == "w" else "w"
    m0, o0 = material(board, mover), material(board, other)
    for uci in [played] + pv[:3]:
        apply_uci(board, uci)
    delta = (material(board, mover) - m0) - (material(board, other) - o0)
    return max(0, -delta)


FAILS = []


def L(move, **score):
    return {"move": move, "score": score}


def case(name, expect_label, played, before, after=None, result=None, legal=30, book=False, recapture=False,
         sac=0, prev=None):
    inp = {"played": played, "linesBefore": before, "afterBest": None if after is None else {"score": after},
           "afterResult": result, "legalMoves": legal, "book": book, "recapture": recapture,
           "sacrificed": sac, "opponentPrevLoss": prev}
    got = classify(inp)
    if got["label"] != expect_label:
        FAILS.append(f"{name}: wanted {expect_label}, reference says {got['label']} (loss {got['loss']:.2f}, "
                     f"before {got['winBefore']:.1f}, after {got['winAfter']:.1f})")
    return {"name": name, "input": inp, "expect": {k: (v if k == "label" else round(v, 4)) for k, v in got.items()}}


def build():
    cases = [
        case("Top move with tiny noise", "best", "e2e4",
             [L("e2e4", cp=30), L("d2d4", cp=28), L("g1f3", cp=25)], {"cp": -28}),
        case("Second choice, small loss", "excellent", "d2d4",
             [L("e2e4", cp=50), L("d2d4", cp=45), L("g1f3", cp=40)], {"cp": -35}),
        case("Loses about 4 points", "good", "a2a3",
             [L("e2e4", cp=100), L("d2d4", cp=90)], {"cp": -65}),
        case("Loses about 8 points", "inaccuracy", "a2a3",
             [L("e2e4", cp=100), L("d2d4", cp=90)], {"cp": -20}),
        case("Loses about 18 points", "mistake", "a2a3",
             [L("e2e4", cp=100), L("d2d4", cp=90)], {"cp": 100}),
        case("Loses about 40 points", "blunder", "a2a3",
             [L("e2e4", cp=100), L("d2d4", cp=90)], {"cp": 400}),
        case("Allows mate in one", "blunder", "g2g4",
             [L("e2e4", cp=200), L("d2d4", cp=190)], {"mate": 1}),
        case("Misses a chance after opponent error", "miss", "a2a3",
             [L("e2e4", cp=400), L("d2d4", cp=380)], {"cp": -250}, prev=15),
        case("Same loss but opponent had not erred", "inaccuracy", "a2a3",
             [L("e2e4", cp=400), L("d2d4", cp=380)], {"cp": -250}, prev=2),
        case("Throwing away a win is a blunder, not a miss", "blunder", "a2a3",
             [L("e2e4", cp=600), L("d2d4", cp=580)], {"cp": 300}, prev=25),
        case("Only legal move", "forced", "e1f1",
             [L("e1f1", cp=-900)], {"cp": 900}, legal=1),
        case("Book move", "book", "a2a3",
             [L("e2e4", cp=100), L("d2d4", cp=90)], {"cp": -80}, book=True),
        case("A book move that blunders is not hidden", "blunder", "g2g4",
             [L("e2e4", cp=-100), L("d2d4", cp=-105)], {"mate": 1}, book=True),
        case("Only good move", "great", "g1f3",
             [L("g1f3", cp=150), L("a2a3", cp=-250), L("h2h3", cp=-300)], {"cp": -148}),
        case("Only move that is a recapture stays best", "best", "g1f3",
             [L("g1f3", cp=150), L("a2a3", cp=-250)], {"cp": -148}, recapture=True),
        case("Only winning move while already ahead", "great", "g1f3",
             [L("g1f3", cp=700), L("a2a3", cp=100)], {"cp": -698}),
        case("Two equally winning moves is not an only move", "best", "g1f3",
             [L("g1f3", cp=700), L("a2a3", cp=680)], {"cp": -698}),
        case("Sacrifice that holds", "brilliant", "d1d8",
             [L("d1d8", cp=40), L("a2a3", cp=30)], {"cp": -38}, sac=4),
        case("Queen sacrifice that forces mate", "brilliant", "b3b8",
             [L("b3b8", mate=2), L("b3c3", cp=344)], {"mate": -1}, sac=9),
        case("Mate in one is not brilliant even after a sacrifice", "best", "d1d8",
             [L("d1d8", mate=1), L("a2a3", cp=100)], None, result="checkmate", sac=5),
        case("Sacrifice while already winning", "best", "d1d8",
             [L("d1d8", cp=500), L("a2a3", cp=490)], {"cp": -498}, sac=4),
        case("Sacrifice that costs too much", "good", "d1d8",
             [L("a2a3", cp=100), L("d1d8", cp=70)], {"cp": -45}, sac=4),
        case("Sacrifice in a lost position", "best", "d1d8",
             [L("d1d8", cp=-250), L("a2a3", cp=-300)], {"cp": 249}, sac=4),
        case("Delivering checkmate", "best", "d8h4",
             [L("d8h4", mate=1), L("h7h5", cp=-300)], None, result="checkmate"),
        case("Stalemating a won position", "blunder", "a1a2",
             [L("a1a8", cp=800), L("a1a2", cp=790)], None, result="stalemate"),
        case("Allowing a smaller mate than needed", "blunder", "a2a3",
             [L("e2e4", mate=5), L("d2d4", cp=900)], {"cp": -200}),
    ]

    win_cases = [{"score": s, "expect": round(win_percent(s), 4)} for s in (
        {"cp": 0}, {"cp": 100}, {"cp": -100}, {"cp": 300}, {"cp": 600}, {"cp": 1000}, {"cp": 5000},
        {"cp": -5000}, {"mate": 1}, {"mate": 7}, {"mate": -3}, {"mate": 0})]
    acc_cases = [{"loss": x, "expect": round(move_accuracy(x), 4)} for x in (0, 1, 2.5, 5, 10, 20, 40, 100)]

    sac_specs = [
        ("Queen for rook", "3rk3/8/8/8/8/8/8/3QK3 w - - 0 1", "d1d8", ["e8d8"]),
        ("Rook trade", "3rk3/8/8/8/8/8/8/3RK3 w - - 0 1", "d1d8", ["e8d8"]),
        ("Free pawn", "4k3/8/8/3p4/8/8/8/3QK3 w - - 0 1", "d1d5", []),
        ("Longer line keeps only four plies", "3rk3/8/8/8/8/8/8/3QK3 w - - 0 1", "d1d8", ["e8d8", "e1e2", "d8c7", "e2e3"]),
        ("Black sacrifices a knight for a pawn", "4k3/8/8/8/8/2n5/1P6/4K3 b - - 0 1", "c3b1", ["e1d1"]),
        ("Promotion is not a sacrifice", "4k3/P7/8/8/8/8/8/4K3 w - - 0 1", "a7a8q", ["e8d7"]),
    ]
    sac_cases = []
    for name, fen, played, pv in sac_specs:
        sac_cases.append({"name": name, "fen": fen, "played": played, "pv": pv, "expect": sacrificed(fen, played, pv)})
    assert [c["expect"] for c in sac_cases] == [4, 0, 0, 4, 0, 0], [c["expect"] for c in sac_cases]

    # Fool's mate as a whole game: per-ply input and the resulting accuracies.
    plies = [
        ("w", case("f3", "mistake", "f2f3", [L("e2e4", cp=30), L("d2d4", cp=28), L("g1f3", cp=25)], {"cp": 120}, legal=20)),
        ("b", case("e5", "best", "e7e5", [L("e7e5", cp=120), L("d7d5", cp=100), L("g8f6", cp=95)], {"cp": -110}, legal=20)),
        ("w", case("g4", "blunder", "g2g4", [L("e2e4", cp=-100), L("d2d4", cp=-105), L("g1h3", cp=-110)], {"mate": 1}, legal=29, prev=0)),
        ("b", case("Qh4#", "best", "d8h4", [L("d8h4", mate=1), L("h7h5", cp=374), L("d7d5", cp=230)], None, result="checkmate", legal=29)),
    ]
    by = {"w": [], "b": []}
    for mover, c in plies:
        by[mover].append(c["expect"]["accuracy"])
    game_case = {
        "name": "Fool's mate",
        "plies": [{"mover": m, **c} for m, c in plies],
        "expect": {"labels": [c["expect"]["label"] for _, c in plies],
                   "accuracyWhite": round(game_accuracy(by["w"]), 4),
                   "accuracyBlack": round(game_accuracy(by["b"]), 4)},
    }

    engine_games = [
        {"name": "Fool's mate", "san": ["f3", "e5", "g4", "Qh4#"],
         "expect": [{"ply": 3, "oneOf": ["blunder", "mistake"]}, {"ply": 4, "oneOf": ["best", "great"]}]},
        {"name": "Opera Game (Morphy vs Duke of Brunswick and Count Isouard, 1858)",
         "san": ["e4", "e5", "Nf3", "d6", "d4", "Bg4", "dxe5", "Bxf3", "Qxf3", "dxe5", "Bc4", "Nf6", "Qb3", "Qe7",
                 "Nc3", "c6", "Bg5", "b5", "Nxb5", "cxb5", "Bxb5+", "Nbd7", "O-O-O", "Rd8", "Rxd7", "Rxd7",
                 "Rd1", "Qe6", "Bxd7+", "Nxd7", "Qb8+", "Nxb8", "Rd8#"],
         "expect": [{"ply": 31, "oneOf": ["brilliant", "great", "best"]}, {"ply": 33, "oneOf": ["best", "great"]}]},
    ]

    return {
        "description": "Generated by tools/gen_review_testdata.py from the reference implementation of docs/REVIEW.md. Numbers are compared with a tolerance of 0.01.",
        "config": CONFIG,
        "winPercent": win_cases,
        "moveAccuracy": acc_cases,
        "classify": cases,
        "sacrifice": sac_cases,
        "game": [game_case],
        "engineGames": engine_games,
    }


if __name__ == "__main__":
    data = build()
    if FAILS:
        raise SystemExit("\n".join(FAILS))
    out = ROOT / "docs" / "testdata" / "review.json"
    out.write_text(json.dumps(data, indent=1) + "\n", encoding="utf-8")
    print(f"wrote {out.relative_to(ROOT)}: {len(data['classify'])} classify cases, {len(data['sacrifice'])} sacrifice cases")
