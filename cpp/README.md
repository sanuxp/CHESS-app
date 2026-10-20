# Chess - C++ implementation

Reference implementation: a dependency-free rules library plus a small SDL2 desktop app.
Status: **done** for Windows (core, tests, app). Linux/macOS should build with the same CMake files but
have not been tried. Android and iOS are prepared for (SDL2, touch via emulated mouse) but not built yet.

## Folder structure
```
cpp/
├── CMakeLists.txt        top-level build: core, tests, app
├── build.ps1             Windows helper: finds Visual Studio's compiler, configures, builds, tests, runs
├── core/                 chess_core: the rules. No third-party code, no graphics.
│   ├── include/chess/
│   │   ├── Types.hpp     Square, Color, PieceType, Piece, Move, flags, castling bits
│   │   ├── Board.hpp     position + FEN in/out + applyMove (rights, en passant, clocks)
│   │   ├── MoveGen.hpp   attack test, pseudo-legal and legal move generation, perft
│   │   ├── Game.hpp      game in progress: history, undo, status (mate, stalemate, draws), results
│   │   ├── Pgn.hpp       SAN conversion and PGN export (with optional per-move annotations)
│   │   └── PgnImport.hpp read a PGN game (tags, comments, variations, NAGs ignored)
│   └── src/              implementations
├── review/               chess_review: game review (no graphics). Needs only chess_core and threads.
│   ├── include/chess/
│   │   ├── Classify.hpp  win%, accuracy, move labels (brilliant, great, best ... blunder)
│   │   ├── Material.hpp  how much material a move sacrifices
│   │   ├── Openings.hpp  opening book from docs/data/openings.tsv
│   │   ├── Review.hpp    scores a whole game from engine evaluations; PGN annotations
│   │   ├── Uci.hpp       UCI parser, child-process transport, engine client
│   │   └── Analyzer.hpp  runs a game through the engine with progress and cancel
│   └── src/
├── tests/                chess_tests executable, run through CTest
│   ├── json_mini.hpp     tiny JSON reader so tests can load ../docs/testdata
│   └── test_*.cpp        basic, perft, scenarios, pgn, review (+ import, openings, engine)
└── app/                  chess: the SDL2 game
    ├── res/              chess.ico (app icon, 7 sizes) + chess.rc that embeds it in chess.exe (Windows)
    └── src/
        ├── main.cpp      entry point + hidden --screenshot / --selftest modes
        ├── App.*         window, event loop, input, game actions, animation start
        ├── State.hpp     all UI state (selection, drag, animation, promotion picker, toast)
        ├── Layout.hpp    board/panel/button geometry for landscape and portrait
        ├── Renderer.*    draws one frame from State
        ├── PieceArt.*    draws the 12 pieces procedurally into textures (no image files)
        ├── ReviewSession.*  one game review: setup, background worker that runs Stockfish, result, cursor
        ├── ReviewUi.*    the review screen: layout, drawing, input
        ├── Font.*        built-in 5x7 pixel font (no font file); drawExact() keeps lower-case for chess notation
        ├── Theme.*       board colours, piece sets, interface palettes, settings.ini load/save
        └── Platform.*    Save-PGN dialog (Windows) / documents folder, dates
```
`core` knows nothing about SDL. Any front end (SDL, Qt, a terminal, an engine) can use it.

## Requirements (Windows)
- Visual Studio 2022 **Build Tools** with "Desktop development with C++" (includes CMake and Ninja).
- Git and an internet connection the first time (CMake downloads SDL2 and builds it statically).

## Build, test, run
From the repository root in PowerShell:
```powershell
powershell -ExecutionPolicy Bypass -File cpp\build.ps1 -Test    # build + run all tests
powershell -ExecutionPolicy Bypass -File cpp\build.ps1 -Run     # build + start the game
build\app\chess.exe                                             # start the game later without rebuilding
```
Manual equivalent (from a "x64 Native Tools Command Prompt" or any shell with CMake, Ninja and a compiler):
```
cmake -S cpp -B build -G Ninja
cmake --build build
ctest --test-dir build --output-on-failure
```
Linux/macOS: install SDL2's build prerequisites, then the same three commands (not yet verified).
Options: `-DCHESS_BUILD_APP=OFF` builds only core + tests (no SDL2 download); `-DCHESS_BUILD_TESTS=OFF`.

## Tests
`ctest` runs eight groups, all driven by the shared data in `../docs/testdata`:
- `basic` - FEN round trips, rejection of bad FEN, undo behaviour.
- `perft` - move-generation node counts for six well-known positions (start position to depth 5).
- `scenarios` - checkmate, stalemate, castling rules, en passant (incl. pin case), promotion, draws.
- `pgn` - SAN strings (disambiguation, promotion, castling, mate marks) and complete PGN text.
- `review` - `review.json`: win percentage, accuracy, move labels, sacrifice detection, a whole game, annotated PGN.
- `import` - reading PGN text (comments, variations, custom start position, illegal moves).
- `openings` - the opening book loads (3,865 lines) and recognises the Ruy Lopez.
- `engine` - UCI parsing and a scripted engine; and, when `engine/stockfish.exe` is installed, real Stockfish reviews
  of Fool's Mate and the Opera Game (about 45 s). Without Stockfish only the engine-free parts run.

Extra checks of the app itself (no human needed):
```powershell
build\app\chess.exe --selftest                  # scripted clicks/drags; results in selftest.log
build\app\chess.exe --screenshot out.bmp --moves "e2e4 e7e5" --select g1 --size 1000x720
```
Screenshot options: `--animate 0.5` (freeze the last move half-way), `--undo`, `--promotion`, `--flip`.

## Playing
Click a piece, then a highlighted square - or drag it. Buttons: New Game, Undo, **Save PGN** (opens a save
dialog; load the file in Lichess "Import PGN" or any engine GUI such as Arena or Scid with Stockfish),
Copy FEN, Flip. Keys: `Esc` cancel, `U` / `Ctrl+Z` undo, `T` themes.

## Game review
**REVIEW** (in the side panel) analyses the game with Stockfish and labels every move: Brilliant, Great, Best, Excellent,
Good, Book, Inaccuracy, Mistake, Miss, Blunder. It shows accuracy per player, an evaluation graph, the opening name, the
engine's preferred move as an arrow, and a move list with a badge per move. Click a move or the graph, or use the arrow
keys; `F` flips the board, `Esc` goes back. **SAVE PGN** exports the game with the labels and evaluations as comments
(`$3` !!, `$1` !, `$6` ?!, `$2` ?, `$4` ??, `[%eval 0.27]`). **OPEN A PGN FILE** (Windows) reviews any saved game.

Stockfish is a separate GPL program that is not part of this repository. Install it once:
```powershell
powershell -ExecutionPolicy Bypass -File tools\get_stockfish.ps1      # downloads and verifies engine\stockfish.exe
```
The app looks for `engine/stockfish(.exe)` next to itself and in parent folders, then the `CHESS_STOCKFISH` variable.
Depth: quick 12, standard 16 (default), deep 20. One search thread: multi-threaded Stockfish was about 30x slower on the
development machine, and several engines at once were no faster. How the labels are decided: `../docs/REVIEW.md`.

Screenshots without a human: `chess.exe --screenshot out.bmp --review "e4 e5 Nf3 ..." --depth 0 --cursor 5`
(runs Stockfish and waits), plus `--review-setup` for the setup card.

## Themes
**Themes** button (or `T`): 8 board colours, 5 piece sets (Engraved, Classic, Shaded, Wood, Royal), 4 interface palettes,
and on/off for coordinates, move hints and animations. Changes are live and saved to `settings.ini` in the app's
preferences folder. Shared spec for all stacks: `../docs/THEMES.md`. Code: `app/src/Theme.*`.

## Notes for mobile
- The game review runs Stockfish as a child process, which phones do not allow; mobile needs it built in as a library.
- The app uses only SDL2 and the C++ standard library, and touch is handled as mouse events, which is what
  SDL's Android and iOS backends deliver.
- **Android**: use SDL2's Android project template (Gradle + NDK) and point its CMake at `core/` and `app/src`.
  Save-PGN already falls back to `SDL_GetPrefPath` where there is no file dialog.
- **iOS**: `cmake -G Xcode` with the iOS toolchain on a Mac; needs signing.
Neither has been built yet (see `../docs/PLAN.md`, phase 9).

## App icon
`app/res/chess.ico` is generated from the logo SVG by `../tools/make_icons.py` (see `../docs/logo/README.md`, which
also explains the Halloween edition). On Windows it is embedded in `chess.exe`; Linux/macOS would additionally need
`SDL_SetWindowIcon`.
