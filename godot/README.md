# Chess - Godot implementation

Status: **not started** (planned, see `../docs/PLAN.md` phase 7). Godot is not installed on this machine yet.

## What will be here
```
godot/
├── project.godot
├── scripts/
│   ├── core/        GDScript port of the rules (board, move generation, game, SAN/PGN)
│   └── ui/          board scene, Tween-based move animation, panel, promotion picker
├── scenes/
└── tests/           headless runner for ../docs/testdata
```
Targets: Windows/Linux/macOS, Android, iOS through Godot's export presets. Behaviour and look follow
`../docs/UI_SPEC.md`; rules follow `../docs/RULES.md`.

## How to run (once implemented)
```
godot --path godot                           # run the game (or open the folder in the Godot editor and press F5)
godot --headless --path godot -s tests/run.gd   # shared test data must pass
```
Mobile: Project > Export, add the Android or iOS preset, and follow Godot's export templates guide.
