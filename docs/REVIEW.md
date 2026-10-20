# Game review: how moves are scored and labelled

Every implementation (Flutter, C++) follows this document and proves it by passing
[`testdata/review.json`](testdata/review.json). The numbers there are produced by the reference
implementation in [`../tools/gen_review_testdata.py`](../tools/gen_review_testdata.py).

The analysis itself is done by **Stockfish** (an external UCI engine). Everything below is plain logic on top
of the engine's numbers, so it can be tested without an engine.

## 1. Inputs per move (ply)
For the move that was played, from the position **before** it (side to move = the *mover*):

| Field | Meaning |
|---|---|
| `played` | the move played, UCI (`e2e4`) |
| `linesBefore` | engine's best lines for the position before the move, best first (MultiPV 3). Each: `move` (first move of the line, UCI) and `score` from the **mover's** view: `{"cp": 35}` or `{"mate": 3}` (negative = mover is getting mated) |
| `afterBest` | engine's best line for the position **after** the move (opponent to move), `score` from the **opponent's** view. `null` if the game ended with the move |
| `afterResult` | `null`, `"checkmate"` (the move mated) or `"stalemate"` |
| `legalMoves` | number of legal moves before the move |
| `book` | the position after the move is in the opening database *and so were all earlier positions of the game* |
| `recapture` | the move captures on the square where the opponent's previous move captured |
| `sacrificed` | material in pawns the mover gives up (section 4) |
| `opponentPrevLoss` | the opponent's `loss` on their previous move, or `null` for the first move |

One engine search per position is enough: the position after move *i* is the position before move *i+1*, so
`afterBest` of one ply is `linesBefore[0]` of the next, seen from the other side.

## 2. Win percentage and accuracy
(Lichess's published formulas.)

```
winPercent({mate: m}) = 100 if m > 0, else 0
winPercent({cp: c})   = 50 + 50 * (2 / (1 + exp(-0.00368208 * clamp(c, -1000, 1000))) - 1)

winBefore = winPercent(linesBefore[0].score)
winAfter  = 100 if afterResult == "checkmate"
            50  if afterResult == "stalemate"
            100 - winPercent(afterBest.score)          # opponent's view flipped to the mover
loss      = max(0, winBefore - winAfter)                # percentage points the move gave away

moveAccuracy = clamp(103.1668 * exp(-0.04354 * loss) - 3.1669, 0, 100)
```
A player's **game accuracy** = (arithmetic mean + harmonic mean) / 2 of their `moveAccuracy` values
(harmonic mean uses a floor of 0.01 per move). A player with no moves scores 100.

## 3. Labels
First rule that matches wins.

| # | Label | Rule |
|---|---|---|
| 1 | **forced** | `legalMoves == 1` (not scored as a decision) |
| 2 | **book** | `book` is true and `loss <= 10` (the opening database also contains bad lines such as Fool's Mate, which must not hide a blunder) |
| 3 | **brilliant** | `loss <= 2` and either (`sacrificed >= 2` and `winBefore <= 80` and `winAfter >= 45`) or (`sacrificed >= 5` and the engine's best line is a forced mate for the mover and the move does not itself mate: a big sacrifice leading to a forced mate, even when already winning, like Morphy's `Qb8+`) |
| 4 | **great** | `played == linesBefore[0].move` and `loss <= 1` and there are at least 2 lines and `winPercent(line0) - winPercent(line1) >= 15` (the only good move, even when already ahead) and not `recapture` and the move does not itself give checkmate (mate in one is obvious) |
| 5 | **best** | `played == linesBefore[0].move` or `loss <= 0.5` |
| 6 | **excellent** | `loss <= 2` |
| 7 | **good** | `loss <= 5` |
| 8 | **miss** | `opponentPrevLoss > 10` and `winBefore >= 60` and `loss >= 5` and not (`loss > 20` and `winAfter < 50`) |
| 9 | **inaccuracy** | `loss <= 10` |
| 10 | **mistake** | `loss <= 20` |
| 11 | **blunder** | otherwise |

Symbols used in the UI and exported PGN: brilliant `!!` (NAG `$3`), great `!` (`$1`), inaccuracy `?!` (`$6`),
mistake `?` (`$2`), blunder `??` (`$4`); miss, best, excellent, good, book and forced have no standard NAG.

These are approximations of chess.com's classification, whose exact rules are not public. All thresholds are in
one table in each implementation (`ReviewConfig`) so they can be tuned; the test data pins the current values.

## 4. Sacrifice
`sacrificed` is computed from the board and the engine's principal variation after the move:

```
plies = [played] + first 3 moves of the PV found after the move   (fewer if the PV is shorter)
values: pawn 1, knight 3, bishop 3, rook 5, queen 9 (kings not counted; a promotion adds the new piece, removes the pawn)
delta = (mover material change) - (opponent material change) over those plies
sacrificed = max(0, -delta)
```
A trade scores 0; giving a queen for a rook scores 4.

## 5. Opening book
`data/openings.tsv` (Lichess `chess-openings`, CC0): `eco`, `name`, `pgn`. Each line is replayed and every
position stored by its first four FEN fields (placement, side, castling, en passant), so transpositions
match. A move is **book** while the positions it leads to are in that set. The opening shown for the
game is the deepest matching name.

## 6. Engine settings
`MultiPV 3`, one search thread, fixed depth, default **16** (quick 12, deep 20). Depth matters: in the Opera Game,
Stockfish 19 still prefers Black's losing `Nxd7` at depth 14 and only sees the mate at depth 16-18. Measured on a
laptop for that 33-ply game: depth 14 = 17 s, 16 = 43 s, 18 = 76 s. More search threads or parallel engines were
not faster on the test machine (multi-threaded Stockfish was 30x slower there), so the default is one thread; it is
configurable. Mate scores are used as reported by the engine.
