# Chess - Python implementation

Same game as the C++ and Flutter versions, written in Python with [pygame-ce](https://pyga.me/): full rules, animated
moves, undo, promotion picker, Save PGN, Copy FEN, flip mode, chess.com-style themes and a Stockfish game review.
Rules follow `../docs/RULES.md`, look and behaviour follow `../docs/UI_SPEC.md`, `../docs/THEMES.md` and
`../docs/REVIEW.md`.

Status: **playable**, game review included, 171 tests pass. Desktop only (Windows tested). Python has no good route to
phones or the browser, so for those use the Flutter version.

## Run
```powershell
cd python
python -m pip install -r requirements.txt     # pygame-ce, and pytest for the tests
python -m chessapp                            # start the game
python -m pytest -q                           # about 20 seconds
```
Written for Python 3.9 or newer, tested on 3.11 only. Tkinter, which ships with Python, provides the save and open dialogs.
For the game review, install Stockfish once with `powershell -ExecutionPolicy Bypass -File ..\tools\get_stockfish.ps1`;
the app finds `engine\stockfish.exe` in the repository, or a program named in the `CHESS_STOCKFISH` variable, or `stockfish`
on the PATH.

## Folder structure
```
python/
├── requirements.txt
├── chessapp/
│   ├── __main__.py         python -m chessapp
│   ├── core/               the rules - plain Python, no pygame (port of the Dart/C++ rules)
│   │   ├── types.py        colours, piece codes, Move, square helpers, flags
│   │   ├── board.py        position, FEN in/out, apply_move
│   │   ├── movegen.py      attack test, legal move generation, perft
│   │   ├── san.py          move <-> standard algebraic notation
│   │   ├── game.py         history, undo, status (mate, stalemate, draws), result
│   │   └── pgn.py          PGN export (with per-move annotations) and import
│   ├── review/             game review: pure Python except the engine launcher
│   │   ├── classify.py     win%, accuracy, move labels, thresholds (ReviewConfig)
│   │   ├── material.py     how much material a move sacrifices
│   │   ├── openings.py     opening book from ../docs/data/openings.tsv
│   │   ├── review.py       scores a whole game; PGN annotations
│   │   ├── uci.py          UCI parsing, process transport, fixed-depth multi-line analysis, finding Stockfish
│   │   └── analyzer.py     runs a game through the engine, progress, cancel, cache
│   └── ui/
│       ├── app.py          window, main loop, switching between screens
│       ├── controller.py   all interaction state: selection, drag, promotion, animation, undo, toast (no pygame)
│       ├── game_screen.py  board and side panel, layout for wide and tall windows
│       ├── board_view.py   board drawing, move hints, promotion picker, game-over banner
│       ├── pieces.py       the 12 pieces as vector shapes (same geometry as the other stacks), cached per size
│       ├── theme.py        board colours, piece sets, interface palettes, saved settings
│       ├── themes_screen.py   the Themes view
│       ├── review_screen.py   the review screen: setup card, progress, results, graph, move list
│       ├── badges.py       label colours, badges and the best-move arrow
│       ├── files.py        save/open dialogs and clipboard
│       └── gfx.py          fonts, text, buttons, translucent shapes
└── tests/
    ├── test_core.py        runs ../docs/testdata: perft, scenarios, SAN/PGN (+ basic checks)
    ├── test_review.py      review.json, UCI parsing, scripted engine, opening book; real Stockfish when installed
    ├── test_controller.py  clicks, drags, promotion, castling, undo, flip, status text
    └── test_ui.py          the real screens driven by synthetic events (headless): panel, themes, review flow
```

## Notes
- **Speed.** Pure Python is far slower than C++. Perft is run only up to 200,000 leaf nodes by default (start position to
  depth 4, not 5); raise it with `CHESS_PERFT_MAX=5000000 python -m pytest tests/test_core.py`. Play is not affected, and the
  review is limited by Stockfish, not by Python.
- **Review.** Press **Review** in the side panel (or open a saved `.pgn` from the review's setup card). The analysis runs on a
  background thread, so the window stays responsive and **Cancel** / `Esc` work at any time. A real-Stockfish test of the
  Opera Game (about a minute) runs with `CHESS_SLOW_TESTS=1`.
- **Settings** (board, pieces, interface, switches, review depth) are stored in `%APPDATA%\chess_python\settings.json`
  (`~/.config/chess_python/` elsewhere).
- **Keys.** Game: `Esc` cancels selection or promotion, `U` or `Ctrl+Z` undoes. Review: Left/Right step, Home/End jump,
  `F` flips, `Esc` goes back (cancelling a running review first). Themes: Up/Down choose a row, Left/Right change it.
