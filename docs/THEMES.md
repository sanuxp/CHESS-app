# Themes and customization (all stacks)

Every stack (C++, Flutter, Godot) offers the same customization, chess.com style. A **Themes** button in the
side panel opens a settings view; changes apply live and are remembered between runs.

## Settings

| Setting | Options | Default |
|---|---|---|
| Board | Brown, Green, Blue, Purple, Slate, Ice, Coral, Walnut | Brown |
| Pieces | Engraved, Classic, Shaded, Wood, Royal | Engraved |
| Interface | Dark, Midnight, Light, Sepia | Dark |
| Coordinates | On / Off | On |
| Move hints | On / Off | On |
| Animations | On / Off | On |

Index order above is the persisted order (store the option **name**, fall back to the default if unknown).

## Board colours (light square / dark square)

| Name | Light | Dark |
|---|---|---|
| Brown | #F0D9B5 | #B58863 |
| Green | #EEEED2 | #769656 |
| Blue | #DEE3E6 | #8CA2AD |
| Purple | #EFEFEF | #8C77B5 |
| Slate | #DCDCDC | #828282 |
| Ice | #E8F0F7 | #7898BA |
| Coral | #F5E1D7 | #C46E5F |
| Walnut | #C8AA82 | #6E4B32 |

## Piece sets

Silhouettes are the same in every set (see `cpp/app/src/PieceArt.cpp`); a set is a rendering mode plus colours.

| Name | Mode | White: body / lines | Black: body / lines | Outline |
|---|---|---|---|---|
| Engraved | horizontal hatch lines, heavier towards the right edge (light from upper left) | #FAF8F2 / #1E1C1C | #1A1A1E / #D6D6DC | white #161414, black #08080A |
| Classic | flat fill | #F8F6EE | #4A4C56 | #26221F / #0E0E12 |
| Shaded | smooth horizontal gradient, light left to dark right | #FFFFFF to #B9B4A8 | #5A5C68 to #101014 | #26221F / #08080A |
| Wood | gradient like Shaded | #EBC98F to #B98A4E | #7A4A2A to #3A2012 | #4A2E14 / #1E0F06 |
| Royal | gradient like Shaded | #FBF1D0 to #D2A94A | #3A4F8F to #141F44 | #6B5314 / #0A1230 |

## Interface palettes

| Name | Background | Panel | Text | Muted | Button | Button hover | Button disabled | Button text |
|---|---|---|---|---|---|---|---|---|
| Dark | #1E1E22 | #2C2C34 | #EBEBF0 | #9696A5 | #465270 | #5C6E96 | #34343C | #EBEBF0 |
| Midnight | #0B1020 | #141C33 | #E3E9FA | #8091B8 | #2B4A8C | #3C62B3 | #1B2640 | #E3E9FA |
| Light | #ECEBE8 | #FFFFFF | #22252B | #6A6F7A | #D5DAE5 | #C0C9DB | #EBEDF1 | #22252B |
| Sepia | #2B2118 | #3A2D20 | #F1E3CC | #B69F80 | #7A5A38 | #946F45 | #4A3A2A | #F6EBD8 |

## Fixed colours (not themeable)
Last-move tint rgba(205,210,106,0.47), selection rgba(246,246,105,0.59), check disc rgba(220,50,50,0.59),
hint dots rgba(40,40,40,0.4).

## Settings view behaviour
- Rows `BOARD`, `PIECES`, `INTERFACE` each show `< name >`: left/right arrows (or clicking the row) cycle the option,
  with a small swatch preview (two squares / a pawn / panel colours).
- Rows `COORDINATES`, `MOVE HINTS`, `ANIMATIONS` toggle `ON`/`OFF`.
- `DONE` closes the view (`Esc` too). `RESET` restores defaults.
- Persist to a small file/preferences store (C++: `SDL_GetPrefPath`, Flutter: `shared_preferences`,
  Godot: `user://settings.cfg`).
