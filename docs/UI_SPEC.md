# UI specification (all implementations)

The goal is that the C++, Flutter and Godot versions feel the same. Values below are the reference
(taken from the C++ app) and can be tuned per stack where the platform needs it.

## Layout
- The board is a square, always fully visible, 8x8 equal cells, White at the bottom unless flipped.
- **Wide window** (width >= 1.1 x height): board on the left, 26 % wide side panel on the right.
- **Tall window** (phone in portrait): board on top, panel underneath.
- Everything is recomputed from the window size every frame (no fixed resolutions); min window 320x400.
- Outer margin 16 px at a 720 px reference size, scaled with the window.

## Colours
| Item | Colour |
|---|---|
| Light square | `#F0D9B5` |
| Dark square | `#B58863` |
| Last move (both squares) | `#CDD26A` at 47 % |
| Selected square | `#F6F669` at 59 % |
| King in check | red disc `#DC3232` at 59 % behind the king |
| Legal move (empty square) | dark dot, 16 % of cell radius |
| Legal move (capture) | dark ring, 38-46 % of cell |
| Panel | `#2C2C34`, background `#1E1E22` |

## Pieces
Two sets (white and black), each with a dark outline. The C++ version draws them procedurally; other
stacks may use vector or image assets as long as all twelve are clearly distinguishable.

## Interaction
- **Click/tap a piece** of the side to move: it is selected and its legal destinations are shown.
- **Click/tap a destination**: the move is played with an animation.
- **Drag a piece** and drop it on a legal destination: played immediately (no flight animation, it is
  already there). Dropping elsewhere returns the piece. Dragging starts after the pointer moves more
  than 6 px.
- Click another own piece to switch selection; click an empty/illegal square or outside to deselect.
- `Esc` cancels selection/promotion; `U` or `Ctrl+Z` undoes.
- Input is ignored while a move animation is playing.
- Touch behaves like the mouse (single finger = left button).

## Animation
- Move: piece slides from origin to target, **220 ms**, ease-in-out (smoothstep).
- Capture: the captured piece fades out over the same 220 ms (en passant: at the pawn's real square).
- Castling: king and rook slide together.
- Promotion: the pawn slides, then becomes the chosen piece when it lands.
- Undo: the same animation in reverse; a captured piece fades back in.
- The board never blocks on rendering; layout changes mid-animation are fine.

## Promotion picker
A dimmed board with a centred bar of four pieces (queen, rook, bishop, knight) in the mover's colour.
Hover highlights; clicking one plays the move; clicking elsewhere cancels.

## Side panel
- Title (wide layout only), a disc in the colour to move and "WHITE TO MOVE" / "BLACK TO MOVE".
- Status line: `CHECK!`, `CHECKMATE - WHITE WINS`, `STALEMATE - DRAW`, `DRAW BY 50 MOVE RULE`,
  `DRAW BY REPETITION`, `DRAW - INSUFFICIENT MATERIAL`.
- Buttons, in this order: **New Game**, **Undo** (disabled when there is nothing to undo),
  **Save PGN**, **Copy FEN**, **Flip: On/Off**.
- Toast text for confirmations ("SAVED chess_2026-10-18_1530.pgn", "FEN COPIED..."), visible 3 s then
  fading for 1 s.
- When the game ends a banner with the result is drawn across the middle of the board.

## Flip mode
Off by default (White always at the bottom). When on, the board turns to face the side to move after
each move, for two people sharing one screen.

## Save PGN
Desktop opens the system save dialog with a name like `chess_YYYY-MM-DD_HHMM.pgn`. Mobile writes into
the app's documents folder (and should offer the system share sheet where the stack supports it).
