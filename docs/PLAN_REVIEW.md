# Game Review (Stockfish-powered) - Implementation Plan

> Living record: tick boxes show what is implemented; section 8 logs progress and deviations.

## 1. Context
The chess app (C++/SDL2 and Flutter, both finished and committed) lets two people play and save a game as PGN. The user now wants a **game review** like chess.com's: analyse a game with an engine and label every move as **Brilliant, Great, Best, Excellent, Good, Book, Inaccuracy, Mistake, Miss, Blunder**, with accuracy per player, an evaluation graph and the best alternative for each move. It must work in all environments (Windows, Android, iOS, web, desktop C++), so the design is: **real Stockfish** (decided by the user) as the analyser, a **pure-logic classifier** written once per stack and proven by one shared test file, and a UI per app. We do not build our own engine.

Facts that shape the plan (checked):
- Win-probability and accuracy formulas are public (Lichess): `win% = 50 + 50*(2/(1+exp(-0.00368208*cp)) - 1)`, `accuracy% = 103.1668*exp(-0.04354*(winBefore - winAfter)) - 3.1669`. Chess.com's labels (Brilliant/Great/Miss) are proprietary, so ours are documented approximations with tunable thresholds.
- Stockfish is GPLv3: running it as a separate process is fine; shipping its binary with an app triggers GPL obligations. **Open item for the user:** license the repo GPL-3.0 if we distribute apps bundling Stockfish. We will not commit the binary to git.
- Flutter packages exist for Stockfish (`stockfish` 1.8.1 GPL-3.0, `stockfish_for_flutter` Android/iOS only, `flutter_stockfish_plugin` SF17 incl. experimental web, `multistockfish_*`). Desktop platform support of these was not confirmed, so desktop uses a plain child process (`dart:io Process`) instead. The mobile package is chosen in phase 5 after reading its pub.dev platform list.
- This machine has no Android SDK and no Mac: Android/iOS engine code can be written but **not run here**. Web (stockfish.js/WASM) is also best-effort.

## 2. Scope
**In:** review of the game in progress or of an opened PGN; Stockfish via UCI; per-move label + symbol; best-move suggestion with arrow; accuracy; eval graph; opening name; export PGN with `[%eval]` comments and NAGs; Windows + Flutter first, then C++ desktop, then mobile/web.
**Out (for now):** Godot, our own engine, cloud/server analysis, persistent analysis cache, puzzles, engine-vs-human play.

## 3. Design

### 3.1 Data flow (per game, n plies)
1. Positions P0..Pn (FEN before each ply, plus final). Analyse each once with Stockfish `go depth D` (default 14), `MultiPV 3`. Cache by FEN+depth in memory.
2. For ply i (mover = side to move in Pi): `winBefore` = win% of the best line of Pi from the mover's view; `winAfter` = win% of the played move = **100 - win%(best line of P(i+1))** (no extra search: the next position's best line already evaluates the played move). `loss = max(0, winBefore - winAfter)` in percentage points.
3. Mate scores map to +-100 win% (with a large cp cap, e.g. mate => +-10000 cp) so mate-in-N losses are blunders.
4. Classification (all numbers live in one config table, mirrored in `docs/REVIEW.md`):

| Label | Rule (first match wins, top to bottom) |
|---|---|
| (none / Forced) | only one legal move |
| Book | resulting position is in the opening database (and every earlier ply was book) |
| Brilliant | `loss <= 2`, played a **sacrifice** (net material the mover gives up over the first 4 plies of the PV after the move is >= 2 pawns), `winBefore <= 80`, `winAfter >= 45` |
| Great | `loss <= 1`, **only good move**: 2nd-best line is >= 15 pp worse than best, `winBefore <= 90`, not a plain recapture |
| Best | played move == engine top move, or `loss <= 0.5` |
| Excellent | `loss <= 2` |
| Good | `loss <= 5` |
| Miss | opponent's previous move was a Mistake/Blunder, mover had `winBefore >= 60` and `loss >= 5` (missed the chance), unless it is already a Blunder from a won game to a lost one |
| Inaccuracy | `loss <= 10` |
| Mistake | `loss <= 20` |
| Blunder | `loss > 20` |

5. Accuracy per player: mean of arithmetic and harmonic mean of per-move accuracies (Lichess-style). Opening name: longest matching line in the opening database.

### 3.2 Openings database
`docs/data/openings.tsv` from the Lichess `chess-openings` dataset (CC0): `eco, name, pgn`. Loaded at runtime into a map `EPD (first 4 FEN fields) -> (eco, name)`, built by replaying each line with our own move code.

### 3.3 Engine access (per platform, behind one interface `UciEngine`)
| Platform | Implementation |
|---|---|
| Windows/Linux/macOS (Flutter) | `ProcessEngine` (dart:io `Process.start`), binary found in `<app>/engine/` or chosen by the user, path saved in settings |
| Android/iOS (Flutter) | native Stockfish package (decided in phase 5) |
| Web (Flutter) | stockfish.js in a Web Worker (best-effort, last) |
| C++ app | `UciEngine` class using CreateProcess pipes (Windows) / fork+pipes (POSIX) |

Stockfish is obtained by `tools/get_stockfish.ps1` (downloads a pinned official release, verifies hash into `flutter/engine/` and `cpp`-side `engine/`, both git-ignored). The Windows Flutter build copies `engine/` next to the exe if present.

## 4. Files

### Shared (repo root)
- `docs/REVIEW.md` - the algorithm spec above (formulas, thresholds, edge cases, example), source of truth for every stack.
- `docs/testdata/review.json` - **engine-free** test cases: each gives per-ply canned engine output (top-3 lines with cp/mate, PV of the next position, legal move count, book flag) and the expected label, `loss`, accuracy. Plus 2 end-to-end games (fool's mate, Opera Game) for the optional real-engine smoke test.
- `docs/testdata/pgn.json` - extended with PGN **import** cases (headers, comments, NAGs, variations, results, `[%eval]`).
- `docs/data/openings.tsv`, `tools/get_stockfish.ps1`.

### Flutter (`flutter/lib/`)
- `core/pgn_import.dart` - `Game? gameFromPgn(String)`; reuses `core/san.dart::moveFromSan` and `core/game.dart::Game`. Also extend `core/pgn.dart::toPgn` with optional review (adds `{[%eval x.xx]}` and `$` NAGs).
- `review/uci_engine.dart` (interface), `review/process_engine.dart` (desktop), `review/engine_stub.dart` (web/mobile placeholder via conditional import), `review/uci_parser.dart` (parses `info depth .. multipv .. score cp|mate .. pv ..`, `bestmove`).
- `review/analyzer.dart` - runs positions through an engine with progress/cancel, caches by FEN.
- `review/classify.dart` - pure functions: `winPercent`, `accuracyOf`, `classifyMove`, `reviewGame`; `review/openings.dart`; `review/material.dart` (sacrifice test using `Board`, `movegen.dart`).
- `ui/review_controller.dart`, `ui/review_view.dart` (summary, eval graph, move list with label icons, best-move text, prev/next, board arrow), small additions to `ui/board_view.dart` (arrow + label badge on destination square), `ui/side_panel.dart` (Review button), `ui/game_controller.dart` (view a historical position), `ui/save_pgn.dart` (open PGN with `file_selector.openFile`), `ui/app_settings.dart` (depth, engine path).
- Tests: `test/review_classify_test.dart` (runs `review.json`), `test/pgn_import_test.dart`, `test/uci_parser_test.dart`, `test/review_ui_test.dart` (fake engine), `test/engine_smoke_test.dart` (skipped unless `engine/stockfish.exe` exists).

### C++ (`cpp/`)
- `cpp/review/` library `chess_review`: `Classify.*`, `Openings.*`, `UciEngine.*`, `Analyzer.*`; `core/` gains `PgnImport.*`. Tests in `cpp/tests/test_review.cpp` reuse `json_mini.hpp` and the same `review.json`. App: `Review` button, background thread with progress, results view in `Renderer.cpp`/`Layout.hpp`/`State.hpp`/`App.cpp`, eval graph and label markers drawn with the existing pixel font and circles.

## 5. Phases and tasks (progress record)

### Phase 0 - Housekeeping and decisions
- [x] 0.1 Copy this plan to `docs/PLAN_REVIEW.md`.
- [ ] 0.2 Confirm with user: GPL-3.0 licensing for the repo if apps ship Stockfish (default: add a `LICENSE` only on request; never commit the binary).
- [x] 0.3 `tools/get_stockfish.ps1` (pinned official Windows build, hash-checked, into git-ignored `engine/`); extend `.gitignore`.

### Phase 1 - Shared spec and data
- [x] 1.1 `docs/REVIEW.md`.
- [x] 1.2 `docs/data/openings.tsv` (download CC0 Lichess data, merge a-e files, keep `eco,name,pgn`).
- [x] 1.3 `docs/testdata/review.json` (about 25 hand-computed cases: each label, boundaries, mate scores, forced move, book, miss, brilliant sacrifice, great only-move) and PGN import cases.

### Phase 2 - Flutter logic (no engine needed)
- [x] 2.1 PGN import + tests. 2.2 `classify.dart`, `material.dart`, `openings.dart` + tests against `review.json`. 2.3 `toPgn` with eval comments/NAGs.

### Phase 3 - Flutter engine layer
- [x] 3.1 `uci_parser.dart` + tests with recorded Stockfish output. 3.2 `ProcessEngine` + `analyzer.dart` (progress, cancel, FEN cache, timeouts, engine crash handling). 3.3 Real-engine smoke test on the fool's mate and the Opera Game (needs `engine/`), then tune thresholds if Morphy's 16.Qb8+ is not Brilliant/Great.

### Phase 4 - Flutter UI
- [x] 4.1 Review button + progress dialog. 4.2 Summary (accuracy both sides, counts per label, opening). 4.3 Move list with colored icons (`!!`, `!`, star/check, book, `?!`, `?`, miss X, `??`), tap to jump. 4.4 Eval graph. 4.5 Best-move arrow + label badge on board. 4.6 Open PGN, export reviewed PGN. 4.7 Settings (depth, engine path). 4.8 Widget tests with a fake engine; screenshot check on Windows.

### Phase 5 - Other Flutter platforms (best-effort, cannot be run here)
- [ ] 5.1 Mobile: pick a native Stockfish package and implement a `UciTransport` for it. **Deferred**: no Android SDK or Mac here, and adding an untestable native dependency to every build was judged riskier than documenting the gap.
- [ ] 5.2 Web worker engine (stockfish.js). Deferred, same reason.
- [x] 5.3 The gap is documented in both READMEs and the review screen shows a clear message when no engine is available.

### Phase 6 - C++ app
- [x] 6.1 `PgnImport`, `Classify`, `Openings` + tests (same JSON). 6.2 `UciEngine` (Windows pipes, POSIX fork) + `Analyzer` with thread. 6.3 SDL UI. 6.4 Self-test additions.

### Phase 7 - Docs and wrap-up
- [x] 7.1 Root `Readme.MD`, `flutter/README.md`, `cpp/README.md`, `docs/PLAN.md`, `docs/logo` unaffected. 7.2 Pre-push hook already runs the suites; smoke test stays opt-in. 7.3 Commits per stack (cpp, flutter, shared docs), no co-author line.

## 6. Verification
- **Logic:** `flutter test` and `ctest` both run `review.json`; identical expected labels, losses and accuracy (tolerance 0.1) in Dart and C++.
- **Engine plumbing:** `uci_parser` tests on recorded output; fake-engine UI tests; with `engine/stockfish.exe` present, the smoke test reviews 1.f3 e5 2.g4 Qh4# (g4 = Blunder, f3 = Mistake/Blunder, Qh4# = Best) and the Opera Game (acceptance: 16.Qb8+ flagged Brilliant or Great; no White move flagged Blunder).
- **Cross-check:** export a reviewed game, import into Lichess analysis, compare accuracy within a few points.
- **Manual:** run the Windows Flutter app, review a 40-move game (expect about 1-2 min at depth 14), step through moves, check arrows, graph, export.
- **Pre-push hook** (existing) still passes; no engine binary or large data committed (openings file about 300 KB is the only new data).

## 7. Risks and mitigations
- **Labels differ from chess.com** (proprietary): documented approximations, thresholds in one table, unit-tested so tuning is safe.
- **Brilliant/Great heuristics are noisy:** gated by conservative conditions; acceptance tested on famous games; easy to loosen.
- **Mobile/web untestable here:** isolated behind `UciEngine`; marked untested in docs until verified on devices.
- **GPL:** do not commit/bundle the engine by default; decision with the user before distribution.
- **Speed:** n+1 searches with MultiPV 3 at depth 14; progress UI, cancel, FEN cache; depth configurable.
- **Engine missing/crashing:** clear message with the `get_stockfish.ps1` hint; timeouts and restart.

## 8. Progress log and deviations

### Log
| Date | Step | Note |
|---|---|---|
| 2026-10-18 | Phase 0-1 | `tools/get_stockfish.ps1` pins Stockfish 19 (SHA-256 checked) into git-ignored `engine/`. `docs/REVIEW.md`, `docs/data/openings.tsv` (3,865 lines, Lichess CC0) and `docs/testdata/review.json` (generated by `tools/gen_review_testdata.py`, a reference implementation whose assertions keep the data honest) written. PGN import cases added to `pgn.json`. |
| 2026-10-18 | Phase 2-3 (Flutter logic and engine) | PGN import, classifier, sacrifice, openings, review, UCI parser/engine/analyzer. Real Stockfish reviews of Fool's Mate and the Opera Game run in `flutter test` when installed. |
| 2026-10-18 | Phase 4 (Flutter UI) | Review page, controller, board with badge and arrow, graph, move list, export. 164 Flutter tests pass; screenshots rendered from a widget test (`docs/screenshots/`). |
| 2026-10-18 | Phase 6 (C++) | `chess_review` library, PGN import, SDL review screen with a worker thread, lower-case font glyphs, self-test and screenshot modes. 8 ctest groups pass, including real Stockfish; the C++ numbers match Flutter's (Opera Game: White 96.4 %, Black 86.5 % at depth 16). |

### Findings that changed the design
- **Depth matters.** At depth 14 Stockfish still prefers Black's losing `Nxd7` in the Opera Game; it sees the mate at depth 16-18. Default depth is therefore **16** (not 14): 17 s / 43 s / 76 s for 33 plies at depth 14 / 16 / 18.
- **Multi-threading was slower, not faster**, on the development machine (80 s with 4 threads vs 2.6 s with 1 for one position), and four parallel engines took four times as long as one. The default is **one thread**, one engine; the option exists.
- **Book moves must not hide errors.** The opening database contains Fool's Mate (1.f3 e5 2.g4), so `g4` was labelled Book. A book move now needs `loss <= 10`.
- **"Great" no longer requires not-already-winning**, because Morphy's `Qb8+` is the only winning move even though he was far ahead; the gap to the second-best line already tests for an only move. Mate-in-one is excluded (obvious).
- **Brilliant gained a second route:** a sacrifice of at least 5 pawns leading to a forced mate counts, even when already winning (the classic queen-sacrifice mate). `Qb8+` is Brilliant at depths 16-18.

### Deviations from the plan above
- Names differ from the plan: `ReviewController`/`ReviewPage`/`ReviewBoard` (Flutter), `ReviewSession`/`ReviewUi` (C++).
- Flutter test files: `review_classify_test.dart`, `uci_engine_test.dart`, `review_ui_test.dart` (the plan named `pgn_import_test.dart` and `uci_parser_test.dart`; their cases live in those files).
- The C++ pixel font gained lower-case letters, `%`, `*`, `(` and `)`, used only through `font::drawExact`, so existing screens look as before.
- Opening data is stored once in `docs/data/` and copied to `flutter/assets/data/` (a test checks the copies are identical) and next to `chess.exe` (CMake post-build step).
- The C++ "Open a PGN file" dialog exists for Windows only.
- GPL decision (task 0.2) is still open: Stockfish is never committed, and a licence for the repo is only needed if apps are distributed together with the engine.
- Not done: phase 5 (mobile and web engines), as noted above; persistent analysis cache; Godot.

### Known limits
- Labels approximate chess.com's proprietary rules; thresholds live in one table (`ReviewConfig`) pinned by `review.json`.
- A mate-in-N blunder in an already lost position scores as a small loss in win percentage (everything is "lost"), so it can be labelled merely Good.
- Accuracy and labels depend on search depth; a deeper search can change them.
