# CHESS - Detailed Implementation Plan

> Living record: tick boxes show what is implemented, section 6 logs progress and deviations from the original plan.

## 1. Context
Greenfield repo: only `.gitignore` and an empty `Readme.MD`. Goal is a locally running two-player chess application (board graphics, animated piece movement, complete chess rules, no network, no AI) for PC, Android and iOS. The repo holds one independent implementation per tech stack (C++, Flutter, Godot) for comparison. C++ is built first and is the reference implementation.

Environment findings: Windows 11, PowerShell, git installed. NOT installed: g++, cl, cmake, ninja, vcpkg, Flutter, Godot.

## 2. Goals / Non-goals
**Goals (v1):** two humans on one device taking turns; full legal rules; smooth move animation; legal-move hints; check/checkmate/draw detection; promotion choice; new game and undo; works with mouse and touch.
**Also in v1 (added on request): Save game as PGN** (standard format that Stockfish GUIs, Lichess, Chess.com and Arena/Scid can import for engine analysis), plus copy-FEN of the current position.
**Non-goals (v1):** AI opponent, online play, clocks, sound, settings screen, loading saved games back into the app (import is a Future item). Listed under "Future" so they are not forgotten.

## 3. Decisions
| Topic | Decision | Why |
|---|---|---|
| C++ standard | C++17 | Wide compiler support incl. Android NDK |
| Build system | CMake + Ninja | Only build system that all of desktop, Android NDK and iOS toolchains support well |
| C++ graphics/input | SDL2 (fetched with CMake FetchContent) | Mouse and touch events, official Android/iOS support, small |
| Piece artwork | Procedurally drawn vector-like shapes initially; optional bundled PNG set later | No asset licensing issues, no files to ship at first |
| Rules engine | Separate library (`chess_core`) with zero third-party deps | Testable without graphics, reusable by other frontends |
| Correctness oracle | Perft node counts + scenario tests in shared JSON | Industry-standard way to prove move generation is correct |
| Stack independence | Each stack re-implements rules and passes the same `docs/testdata` | Fair comparison between stacks |
| Mobile in this pass | Document + keep code portable; actual Android/iOS builds are a later phase | iOS needs a Mac; Android needs NDK/Studio |

## 4. Repo layout (target)
```
CHESS/
├── Readme.MD                 overview, structure, how to run every stack, status table
├── .gitignore
├── docs/
│   ├── PLAN.md               this plan + progress log (living record)
│   ├── RULES.md              rules checklist every stack must satisfy
│   ├── UI_SPEC.md            colors, sizes, animation timings, interaction states
│   └── testdata/
│       ├── perft.json        FEN + expected node counts per depth
│       └── scenarios.json    FEN + move sequence + expected status/legal moves
├── cpp/
│   ├── README.md             folder structure + build/run/test (this stack)
│   ├── CMakeLists.txt        top-level; adds core, tests, app
│   ├── core/
│   │   ├── CMakeLists.txt
│   │   ├── include/chess/{Types,Board,MoveGen,Game}.hpp
│   │   └── src/{Board,MoveGen,Game}.cpp
│   ├── tests/
│   │   ├── CMakeLists.txt    registers CTest targets
│   │   ├── test_main.cpp, test_perft.cpp, test_scenarios.cpp
│   │   └── json_mini.hpp     tiny JSON reader (no external dependency)
│   └── app/
│       ├── CMakeLists.txt
│       └── src/{main,App,Renderer,PieceArt,Animator,Input,Ui}.cpp/.hpp
├── flutter/                  README.md + Flutter project (Phase 5)
└── godot/                    README.md + Godot project (Phase 6)
```

## 5. Phases and tasks (checklist = progress record)

### Phase 0 - Housekeeping
- [x] 0.1 Copy this plan to `docs/PLAN.md`.
- [x] 0.2 Create folder skeleton (`docs`, `cpp`, `flutter`, `godot`) with placeholder READMEs.
- [x] 0.3 Extend `.gitignore` (build/, out/, .vs/, .idea/, Flutter `.dart_tool/`, `build/`, Godot `.godot/`, `*.import`-cache as appropriate).
- [ ] 0.4 Initial commit (only if the user asks).

### Phase 1 - Toolchain (system change, needs explicit OK before running)
- [x] 1.1 (not needed, see log) `winget install Kitware.CMake Ninja-build.Ninja`.
- [x] 1.2 (not needed, see log) Compiler: Visual Studio 2022 Build Tools with "Desktop development with C++" (`Microsoft.VisualStudio.2022.BuildTools`). Fallback: MSYS2 UCRT64 g++.
- [x] 1.3 (not needed, see log) Verify: `cmake --version`, `ninja --version`, compiler hello-world through CMake configure.
- Exit criterion: an empty CMake project configures and builds.

### Phase 2 - Shared specs and test data (`docs/`)
- [x] 2.1 `RULES.md`: piece movement, check/pin legality, castling conditions (king/rook unmoved, empty path, not in/through/into check), en passant (only immediately after a double push, capture removes pawn, discovered-check edge case), promotion (Q/R/B/N, mandatory), end conditions (checkmate, stalemate, 50-move, threefold repetition, insufficient material: K v K, K+minor v K, K+B v K+B same colour), draw not auto-claimed beyond those listed.
- [x] 2.2 `UI_SPEC.md`: light/dark square colors, selected highlight, legal-move dot/ring, last-move tint, check red glow on king, animation 220 ms ease-in-out, capture fade 120 ms, input locked while animating, board flipping option (off by default; board stays White-at-bottom, optional "flip each turn" toggle for pass-and-play), promotion popup anchored to the promotion square, status banner texts, New Game/Undo buttons, responsive layout (square board fitted to the smaller window dimension, side panel when landscape / bottom bar when portrait).
- [x] 2.3 `perft.json`: start position depths 1-5 (20, 400, 8902, 197281, 4865609); Kiwipete depths 1-4 (48, 2039, 97862, 4085603); positions 3, 4, 5 from the Chess Programming Wiki with their published counts.
- [x] 2.4 `scenarios.json`: scholar's mate (checkmate), fool's mate, known stalemate FEN, king-side and queen-side castling allowed/blocked/through-check, en passant allowed/expired/pin-illegal, promotion (all four pieces, with capture), 50-move draw FEN, threefold repetition sequence, insufficient-material FENs.

### Phase 3 - C++ rules core (`cpp/core`)
- [x] 3.1 `Types.hpp`: `Color`, `PieceType`, `Piece`, `Square` (0-63), `Move{from,to,promo,flags}`, flag bits (capture, double-push, en-passant, castle-K/Q, promotion).
- [x] 3.2 `Board`: mailbox array, side to move, castling rights, en-passant square, halfmove/fullmove counters; `fromFEN`/`toFEN`; start position helper.
- [x] 3.3 `MoveGen`: pseudo-legal moves per piece type; `isSquareAttacked`; legality by make-move + own-king-attacked test; castling rules; en passant; promotions expand to 4 moves.
- [x] 3.4 `Game`: `makeMove` (updates rights, ep square, counters, captured piece), `undo` (snapshot stack), `legalMoves()`, `legalMovesFrom(sq)`, `status()` enum {Playing, Check, Checkmate, Stalemate, DrawFiftyMove, DrawRepetition, DrawInsufficient}, position hash for repetition.
- [x] 3.5 Keep API UI-friendly: `pieceAt(sq)`, `lastMove()`, `isInCheck()`, `kingSquare(color)`.
- [x] 3.6 SAN + PGN (`Pgn.hpp/.cpp`): `moveToSAN` (piece letter, file/rank disambiguation, `x`, `=Q`, `O-O`/`O-O-O`, `+`/`#` suffixes); `Game::toPGN(headers)` producing the 7 required tags (Event, Site, Date, Round, White, Black, Result) plus `FEN`/`SetUp` tags if the game started from a non-standard position, movetext wrapped at 80 columns, result token (`1-0`, `0-1`, `1/2-1/2`, `*` for unfinished). Player names default to "White"/"Black".
- Exit criterion: library builds with `-Wall -Wextra` clean on MSVC and (if available) g++/clang.

### Phase 4 - C++ tests (`cpp/tests`)
- [x] 4.1 Minimal JSON reader (objects/arrays/strings/numbers only) to load `docs/testdata`; path passed to the test via CMake definition.
- [x] 4.2 `test_perft`: every position/depth in `perft.json` must match exactly.
- [x] 4.3 `test_scenarios`: for each scenario, set FEN, apply moves, assert status and expected legal-move set.
- [x] 4.3b `test_pgn`: `pgn.json` in `docs/testdata` holds move sequences with expected SAN strings (disambiguation, promotion with check, castling, en passant, mate) and full expected PGN text; round-trip sanity check that each SAN resolves to exactly one legal move.
- [x] 4.4 Register with CTest; failing test prints position FEN and the differing counts (perft divide helper for debugging).
- Exit criterion: `ctest` all green.

### Phase 5 - C++ SDL2 app (`cpp/app`)
- [x] 5.1 CMake: `FetchContent` SDL2 (pinned release tag), link `chess_core`; Windows subsystem GUI; copy runtime DLL if shared.
- [x] 5.2 `App`: window (resizable, high-DPI), main loop at 60 fps with delta-time; state machine {Idle, PieceSelected, Animating, PromotionChoice, GameOver}.
- [x] 5.3 `Renderer`: draw 8x8 board, coordinates (a-h, 1-8), highlights (selected, legal dots, last move, check). Layout computed from window size so it works in portrait and landscape.
- [x] 5.4 `PieceArt`: draw all 12 pieces procedurally (polygons/circles with outline) into cached SDL textures at the current square size; re-render on resize.
- [x] 5.5 `Input`: unified handling of mouse click, mouse drag, and touch finger events (SDL_FINGER*) mapped to board squares; click-to-move and drag-and-drop both supported.
- [x] 5.6 `Animator`: tween (ease-in-out) from-square to to-square; knight moves arc optional; castling animates king and rook together; captured piece fades; en-passant removes the right pawn; undo animates in reverse.
- [x] 5.7 `Ui`: side/bottom panel with turn indicator, status text (Check, Checkmate - X wins, Stalemate, Draw reason), New Game and Undo buttons, promotion popup with four piece choices. Text via SDL2_ttf with a bundled open-licence font, or a tiny built-in bitmap font to avoid an asset (decision at implementation; default: bitmap font, no extra dependency).
- [x] 5.7b Save game: "Save PGN" button (any time, also mid-game and after game over). Desktop: native save dialog via a small platform helper (Win32 `GetSaveFileName`, fallback auto-named file `chess_YYYY-MM-DD_HHMM.pgn` in the user's Documents/CHESS folder); mobile: write to the app's documents dir (SDL_GetPrefPath) and show the path/share sheet. Toast confirms location. "Copy FEN" button copies current position to clipboard (`SDL_SetClipboardText`). Optional auto-save of the last game on exit.
- [x] 5.8 Polish: minimum window size, escape to cancel selection, game-over overlay, no animation stutter when window resizes.
- Exit criterion: complete game playable start to checkmate with all special moves.

### Phase 6 - Flutter implementation (`flutter/`)
- [x] 6.1 Install Flutter SDK (needs approval); `flutter create` into `flutter/`; platforms windows/android/ios.
- [x] 6.2 Port rules to Dart in `lib/core/`; `test/` runs the same `docs/testdata` JSON.
- [x] 6.2b SAN/PGN export in Dart, tested against `pgn.json`; save via `file_selector`/`share_plus` (works on desktop and mobile).
- [x] 6.3 UI: `CustomPainter` board + `AnimatedPositioned`/`AnimationController` piece moves; same states and layout as `UI_SPEC.md`.
- [x] 6.4 `flutter/README.md`: structure and `flutter test` / `flutter run -d windows|chrome|<android device>`.

### Phase 7 - Godot implementation (`godot/`)
- [ ] 7.1 Install Godot 4.x (needs approval); create project in `godot/`.
- [ ] 7.2 GDScript rules in `scripts/core/`; headless test runner against `docs/testdata`.
- [ ] 7.2b SAN/PGN export in GDScript, tested against `pgn.json`; save to `user://` and via `FileDialog` on desktop.
- [ ] 7.3 UI: Control/Node2D scenes, Tween-based animations, input via `InputEventScreenTouch` and mouse.
- [ ] 7.4 Export presets (Windows, Android, iOS), `godot/README.md`.

### Phase 8 - Documentation (done incrementally; finalised here)
- [x] 8.1 Root `Readme.MD`: what the project is, status table of stacks, project structure tree, prerequisites and exact run/test commands per stack (C++, Flutter, Godot), how platforms map to stacks, link to `docs/PLAN.md`.
- [x] 8.2 `cpp/README.md`: explains `core/`, `tests/`, `app/`, dependency policy, build/run/test commands, Android (NDK + SDL2 Android project) and iOS (Xcode via CMake `-G Xcode`) notes.
- [x] 8.3 `flutter/README.md`, `godot/README.md` as above.
- [ ] 8.4 Every command in the READMEs is executed once to confirm it works as written.

### Phase 9 - Mobile (C++), follow-up, not in first pass
- [ ] 9.1 Android: SDL2 Android template + Gradle wrapping `chess_core`/`app` via NDK CMake.
- [ ] 9.2 iOS: generate Xcode project, signing on a Mac.

### Phase 10 - Python implementation (`python/`)
- [x] 10.1 Rules in `chessapp/core/` (Board, move generation, SAN, Game, PGN export and import); `tests/test_core.py` runs the same `docs/testdata` JSON (perft limited to 200,000 nodes by default, pure Python being slow).
- [x] 10.2 Review in `chessapp/review/`: labels and accuracy (`review.json`), sacrifice, opening book, UCI engine over a process, analyzer with progress and cancel.
- [x] 10.3 pygame-ce UI: board, drag and click, promotion picker, animations, panel, Themes, review screen (badge, arrow, graph, move list); interaction logic in `controller.py` without pygame so tests drive it directly.
- [x] 10.4 `python/README.md`, root `Readme.MD`, `tools/run_tests.ps1 -Python`, pre-push hook for `python/`.

## 6. Progress log (updated as we go)
| Date | Phase/Task | Note |
|---|---|---|
| 2026-10-18 | Plan | Plan written and approved (PGN save added on request) |
| 2026-10-18 | Phase 0 | Folder skeleton, `.gitignore`, this file copied into `docs/`. No commit made (not requested). |
| 2026-10-18 | Phase 1 | **Skipped, not needed**: Visual Studio 2022 Build Tools (MSVC 19.44, CMake, Ninja) were already installed; nothing was installed. `cpp/build.ps1` locates them through vswhere. |
| 2026-10-18 | Phase 2 | `docs/RULES.md`, `docs/UI_SPEC.md`, `docs/testdata/{perft,scenarios,pgn}.json` written. |
| 2026-10-18 | Phase 3-4 | Rules core and tests done. `ctest`: basic, perft (start position depth 5 = 4,865,609; Kiwipete d4; positions 3-6), 25 scenarios, 11 PGN cases - all pass first run, `/W4` clean. |
| 2026-10-18 | Phase 5 | SDL2 app done and built (SDL 2.30.9, static). Verified with screenshots and a scripted `--selftest` (14 checks incl. click-click, drag, promotion picker, undo, castling, flip). The self-test caught a real bug (a dropped piece was animating from its origin); fixed. |
| 2026-10-18 | Phase 6 | Flutter SDK 3.47.7 cloned to `C:/src/flutter` (no official winget package). Project created in `flutter/` (windows, android, ios, web). Dart rules ported; `flutter test` = 70 tests pass (all shared perft incl. depth 5, scenarios, SAN/PGN, plus widget tests for tap, drag, promotion, undo, castling, flip, portrait). `flutter build windows` and `flutter build web` succeed; Windows app launched and screenshot-checked. Not built: Android (SDK not installed), iOS (needs Mac). |
| 2026-10-18 | Logo | Approved: gold pawn/queen-shadow artwork for banners (A), a simplified vector for all app icons (B), and a Halloween icon + banner as the seasonal edition (C). `tools/make_icons.py [halloween]` renders the SVG to PNGs and a multi-size `.ico`; `flutter_launcher_icons` applied to Android/iOS/web/Windows; `cpp/app/res/chess.rc` embeds the icon in `chess.exe` (verified by extracting it). See `docs/logo/README.md`. |
| 2026-10-18 | Git | First commits: C++ (`cpp/`), Flutter (`flutter/`) and shared docs/tools. Not committed on purpose: build output, `docs/logo/source/` (2 MB originals) and generated `docs/logo/final/` (see `.gitignore`). |
| 2026-10-18 | Push hook | `.githooks/pre-push` runs `tools/run_tests.ps1` (C++ build, ctest, app self-test; Flutter pub get, analyze, test) for the stacks that changed, enabled with `git config core.hooksPath .githooks`. No new tests were written: the existing suites already cover rules, UI and PGN. Verified: no-op push skips, a deliberately broken perft value blocks the push, a clean run passes. |
| 2026-10-18 | Phase 8 | Root `Readme.MD`, `cpp/`, `flutter/`, `godot/` READMEs written. Flutter/Godot READMEs say "not started". |
| 2026-10-22 | Phase 10 | Python stack added with pygame-ce: `python -m pytest` = 171 tests pass (shared perft up to depth 4, scenarios, SAN/PGN, review.json, UCI parsing, controller, headless UI flows) and a real-Stockfish review of the Opera Game labels `Qb8+` brilliant. Opening-book parsing was the one slow spot (9 s); caching the position after each shared move prefix and checking legality only for SAN candidates brought it to under 1 s. |

### Deviations from the plan above
- `toPGN` is a free function in `Pgn.hpp` (`toPGN(game, headers)`), not `Game::toPGN`.
- Touch input is handled through SDL's emulated mouse events (single finger = left button) instead of
  separate `SDL_FINGER*` handlers; same behaviour, one code path.
- Text uses the built-in 5x7 pixel font (the planned default); no SDL2_ttf. Text is upper-case only.
- Hidden developer modes added to the app: `--screenshot` and `--selftest` (see `cpp/README.md`).
- Piece and button text are not yet localised/scalable beyond integer font scaling.
- Flutter: UI uses Flutter widgets (real fonts, Material buttons) rather than the pixel font, and pieces are vector paths built from the same shapes as C++. Perft runs in the default `flutter test` (no skipping). Window title/app name set to "Chess".
- Flutter: Save PGN uses `file_selector` (desktop dialog), a download on web and `share_plus` on Android/iOS; the mobile and web paths were not exercised.

### Known gaps / ideas noticed
- The panel's turn indicator disc still shows the side to move when the game is over.
- No move list in the panel (the pixel font has no lowercase, which would make SAN ambiguous, e.g. `Bxc3` vs `bxc3`).
- Not yet tested on Linux/macOS; Android and iOS builds not started (phase 9).
- Phases 7 (Godot) and 9 (mobile builds) are open; they need SDK installs and approval first.

## 7. Verification (end-to-end)
- Core: `cmake -S cpp -B build -G Ninja && cmake --build build && ctest --test-dir build --output-on-failure` - all perft counts exact.
- App: run the built `chess` executable; play scripted games: scholar's mate, both castles, en passant, each promotion piece, stalemate FEN, undo across a castle/capture/promotion, rapid clicking during animation (must not corrupt state), resize and DPI change, touch input where available.
- PGN: play a short game, save it, import the file into Lichess (analysis board > Import PGN) or a desktop GUI with Stockfish (e.g. Arena/Scid); it must load without errors and show the same moves and result.
- Other stacks: `flutter test`, Godot headless runner - same testdata must pass before marking done.
- README check: copy-run each documented command from a clean clone state.

## 8. Risks and mitigations
- No toolchain yet - Phase 1 gated on user approval; fallback MSYS2 if VS Build Tools install is blocked.
- Move-gen bugs are subtle - perft to depth 5 plus Kiwipete catches nearly all; scenario tests cover repetition/50-move which perft does not.
- Text rendering assets - bitmap font avoids licensing and a dependency.
- iOS unbuildable on Windows - documented, deferred.
- Three stacks = triple work - strict ordering: a stack is only started after the previous one passes its tests.

## 9. Future ideas (out of scope now)
AI opponent (minimax/alpha-beta using the C++ core), clocks, PGN/FEN import (loading saved games), built-in UCI engine hook-up, sound, themes, online play, saved games.
