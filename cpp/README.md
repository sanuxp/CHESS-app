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
│   │   └── Pgn.hpp       SAN conversion and PGN export
│   └── src/              implementations
├── tests/                chess_tests executable, run through CTest
│   ├── json_mini.hpp     tiny JSON reader so tests can load ../docs/testdata
│   └── test_*.cpp        basic, perft, scenarios, pgn
└── app/                  chess: the SDL2 game
    ├── res/              chess.ico (app icon, 7 sizes) + chess.rc that embeds it in chess.exe (Windows)
    └── src/
        ├── main.cpp      entry point + hidden --screenshot / --selftest modes
        ├── App.*         window, event loop, input, game actions, animation start
        ├── State.hpp     all UI state (selection, drag, animation, promotion picker, toast)
        ├── Layout.hpp    board/panel/button geometry for landscape and portrait
        ├── Renderer.*    draws one frame from State
        ├── PieceArt.*    draws the 12 pieces procedurally into textures (no image files)
        ├── Font.*        built-in 5x7 pixel font (no font file)
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
`ctest` runs four groups, all driven by the shared data in `../docs/testdata`:
- `basic` - FEN round trips, rejection of bad FEN, undo behaviour.
- `perft` - move-generation node counts for six well-known positions (start position to depth 5).
- `scenarios` - checkmate, stalemate, castling rules, en passant (incl. pin case), promotion, draws.
- `pgn` - SAN strings (disambiguation, promotion, castling, mate marks) and complete PGN text.

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

## Themes
**Themes** button (or `T`): 8 board colours, 5 piece sets (Engraved, Classic, Shaded, Wood, Royal), 4 interface palettes,
and on/off for coordinates, move hints and animations. Changes are live and saved to `settings.ini` in the app's
preferences folder. Shared spec for all stacks: `../docs/THEMES.md`. Code: `app/src/Theme.*`.

## Notes for mobile
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
