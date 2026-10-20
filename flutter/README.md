# Chess - Flutter implementation

Same game as the C++ version, written in Flutter/Dart: full rules, animated moves, undo, promotion
picker, Save PGN, Copy FEN, flip mode, chess.com-style themes. Rules follow `../docs/RULES.md`, look and behaviour follow
`../docs/UI_SPEC.md`.

Status: **done** for Windows and web (built and tested). Android and iOS code is generated and uses
only cross-platform packages, but has not been built: the Android SDK is not installed on this machine
and iOS needs a Mac.

## Folder structure
```
flutter/
├── pubspec.yaml            dependencies: file_selector (save dialog), share_plus (share sheet on phones), shared_preferences (saved themes)
├── lib/
│   ├── main.dart           app, page layout (side panel beside the board, or below on tall screens), key shortcuts
│   ├── core/               the rules - plain Dart, no Flutter imports (port of cpp/core)
│   │   ├── types.dart      PieceColor, PieceType, Piece, Move, square helpers, flags
│   │   ├── board.dart      position, FEN in/out, applyMove
│   │   ├── movegen.dart    attack test, legal move generation, perft
│   │   ├── san.dart        move <-> standard algebraic notation
│   │   ├── game.dart       history, undo, status (mate, stalemate, draws), result
│   │   ├── pgn.dart        PGN export (with per-move annotations)
│   │   └── pgn_import.dart read a PGN game (tags, comments, variations, NAGs ignored)
│   ├── review/             game review logic: pure Dart except the engine launcher
│   │   ├── classify.dart   win%, accuracy, move labels
│   │   ├── material.dart   how much material a move sacrifices
│   │   ├── openings.dart   opening book from assets/data/openings.tsv
│   │   ├── review.dart     scores a whole game; PGN annotations
│   │   ├── uci_parser.dart, uci_engine.dart   UCI protocol over a transport
│   │   ├── analyzer.dart   runs a game through the engine, progress, cancel, cache
│   │   └── engine_factory*.dart   finds and starts Stockfish (desktop); stub elsewhere
│   └── ui/
│       ├── game_controller.dart  all interaction state: selection, drag, animation, promotion, toast
│       ├── board_view.dart       board painter, pointer handling, promotion picker, game-over banner
│       ├── piece_art.dart        the 12 pieces as vector paths (same shapes as the C++ app), drawn per piece set
│       ├── app_settings.dart     board colours, piece sets, interface palettes, toggles, saved with shared_preferences
│       ├── themes_view.dart      the Themes dialog (live preview)
│       ├── review_controller.dart  state of one review (setup, progress, result, cursor)
│       ├── review_page.dart      the review screen: setup card, results, graph, move list
│       ├── review_board.dart     read-only board with the label badge and best-move arrow
│       ├── label_style.dart      colours and icons of the move labels
│       ├── side_panel.dart       turn indicator, status, buttons
│       ├── status_text.dart      status wording
│       └── save_pgn.dart         platform-specific saving (dialog / download / share sheet)
├── test/
│   ├── core_test.dart      runs ../docs/testdata: perft, scenarios, SAN/PGN (+ basic checks)
│   ├── ui_test.dart        taps, drags, promotion, undo, castling, flip, portrait layout
│   ├── review_classify_test.dart  review.json, PGN import, opening book, a whole game
│   ├── uci_engine_test.dart   UCI parsing, scripted engine, analyzer; real Stockfish when installed
│   ├── review_ui_test.dart    the review screen with a scripted engine
│   └── support/fake_transport.dart
│   └── settings_test.dart  themes: defaults, cycling, persistence, dialog
├── windows/ android/ ios/ web/   generated platform projects
```

## Requirements
- Flutter 3.47 or newer (Dart 3.13). On this machine it was cloned to `C:\src\flutter`
  (`git clone -b stable https://github.com/flutter/flutter.git C:\src\flutter`); add `C:\src\flutter\bin` to PATH.
- Windows desktop: Visual Studio 2022 (or Build Tools) with "Desktop development with C++".
- Android: Android Studio / SDK. iOS: a Mac with Xcode.
- Check everything with `flutter doctor`.

## Run, test, build
```powershell
cd flutter
flutter pub get
flutter test                      # 164 tests: rules, review logic, UI (about a minute with real Stockfish)
flutter run -d windows            # or: -d chrome, -d <android device or emulator id>
flutter build windows             # build\windows\x64\runner\Release\chess_app.exe
flutter build web                 # build\web
flutter build apk                 # needs the Android SDK
```
Perft to depth 5 is part of `flutter test` (about 20 seconds in total).

## Game review
**REVIEW** opens a full-screen review of the current game (or **Open a PGN file...**): Stockfish analyses every position
and each move is labelled Brilliant, Great, Best, Excellent, Good, Book, Inaccuracy, Mistake, Miss or Blunder. You get
accuracy per player, an evaluation graph, the opening, the engine's preferred move (arrow and text), and a move list; the
arrow keys, Home/End, the graph and the list all navigate. The save button exports the PGN with the labels and
evaluations as comments. Rules for the labels: `../docs/REVIEW.md`; screenshots: `../docs/screenshots/`.

Stockfish is a separate GPL program, not part of this repository. Install it once with
`powershell -ExecutionPolicy Bypass -File ..\tools\get_stockfish.ps1`; the app finds `engine/stockfish.exe` next to
itself or in a parent folder (so `flutter run` works from the repository), or via `CHESS_STOCKFISH`.

| Platform | Review engine |
|---|---|
| Windows (also Linux/macOS in principle) | Stockfish as a child process: **built and tested** |
| Android, iOS | needs Stockfish built in as a native library; **not implemented** (the `UciTransport` interface is ready for it) |
| Web | needs stockfish.js (WebAssembly) in a worker; **not implemented** |

On a platform without an engine the review screen shows a clear message instead of failing.

## Themes
The **THEMES** button opens a dialog (works in wide and narrow layouts) with board colours, piece sets,
interface palettes and the coordinates / move hints / animations toggles, as specified in
`../docs/THEMES.md`. Changes apply live and are stored with `shared_preferences` (option names, so unknown
values fall back to the defaults). The settings code could not be compiled or tested when it was written
(Flutter was not available on that machine): run `flutter pub get` and `flutter test` first.

## Saving a game
**Save PGN** behaves per platform: desktop opens the system save dialog, the browser downloads the file,
Android/iOS open the share sheet (send it to Lichess, Files, mail...). Import the `.pgn` into Lichess
(https://lichess.org/paste), Chess.com, or Arena/Scid with Stockfish.

## App icon
Generated from the logo SVG: see `../docs/logo/README.md`. To regenerate (or switch to the Halloween edition):
`python ../tools/make_icons.py [halloween]` then `dart run flutter_launcher_icons`.
