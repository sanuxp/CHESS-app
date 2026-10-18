# Rules every implementation must satisfy

Each stack (C++, Flutter, Godot) re-implements these rules on its own and proves it by passing the
shared data in [`testdata/`](testdata/): `perft.json`, `scenarios.json` and `pgn.json`.

## Board and notation
- Squares: `a1` (White's left-hand corner) to `h8`. Internally 0-63 with `a1 = 0`, `h1 = 7`, `a8 = 56`.
- Positions are exchanged as FEN, moves as UCI (`e2e4`, `e7e8q`) in the test data, and recorded as SAN
  (`Nf3`, `exd5`, `O-O`, `e8=Q#`) in games and PGN.

## Movement
| Piece | Moves |
|---|---|
| Pawn | One step forward onto an empty square; two steps from its start rank if both squares are empty; captures one step diagonally forward. |
| Knight | L-shape (2+1), jumps over pieces. |
| Bishop | Any distance diagonally, blocked by pieces. |
| Rook | Any distance along ranks and files, blocked by pieces. |
| Queen | Bishop + rook. |
| King | One step in any direction. |

A move is **legal** only if it does not leave the mover's own king attacked. This covers pins, moving
into check, and having to answer a check (capture the checker, block, or move the king).

## Special moves
- **Castling**: allowed when the king and that rook have never moved, every square between them is
  empty, the king is not in check, and neither the square it crosses nor the square it lands on is
  attacked. King goes two squares towards the rook, the rook jumps to the other side of the king.
  Rights are lost when the king moves, a rook moves, or a rook is captured on its home square.
- **En passant**: right after an enemy pawn advances two squares and lands beside your pawn, you may
  capture it as if it had advanced one square. Only on the very next move. The capture is illegal if it
  would expose your own king (for example along the rank).
- **Promotion**: a pawn reaching the last rank must become a queen, rook, bishop or knight (player's
  choice). Each choice is a separate legal move; captures while promoting are allowed.

## End of game
Checked after every move, in this order:
1. **Checkmate** - side to move is in check and has no legal move. The other side wins.
2. **Stalemate** - side to move is not in check and has no legal move. Draw.
3. **Insufficient material** - K v K, K+B v K, K+N v K, or only bishops that all stand on the same
   colour of square. Draw.
4. **Threefold repetition** - the same position (placement, side to move, castling rights, and en
   passant only if a capture is actually possible) has occurred three times. Draw, declared
   automatically.
5. **Fifty-move rule** - 100 half-moves without a capture or pawn move. Draw, declared automatically.
6. Otherwise **Check** (side to move is attacked) or **Playing**.

Draws by repetition and the fifty-move rule are applied automatically (a real game would require a
claim); this keeps the app simple for two casual players.

## Game controls
- Undo takes back the last move and restores everything (rights, en passant square, move counters,
  repetition history).
- New game resets to the starting position.

## Saving
- PGN export: seven-tag roster (Event, Site, Date, Round, White, Black, Result), `SetUp`/`FEN` tags for
  games that did not start from the standard position, movetext wrapped at 80 columns, result token
  `1-0`, `0-1`, `1/2-1/2` or `*` for unfinished games. Importable by Lichess, Chess.com, Arena, Scid and
  Stockfish front ends.
- FEN of the current position can be copied to the clipboard.
