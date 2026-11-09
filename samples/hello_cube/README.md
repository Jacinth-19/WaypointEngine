# hello_cube

The smallest interesting Waypoint project: a camera, a sun, a ground slab and one
cube that falls under gravity.

```bash
python tools/wp.py run samples/hello_cube
```

## Controls

| Key | Action |
| --- | --- |
| W / A / S / D | fly |
| Space / Left Control | up / down |
| Shift | boost |
| Right mouse (hold) or F1 | capture the mouse and look |
| Escape | quit |

## Scripts

Two kinds, and they are different things:

- `scripts/main.lua` is the **entry script**, named by `scriptEntry` in
  `waypoint.project.json`. One per project.
- `scripts/cube.lua` is attached to a single entity by the scene's `script`
  component. Every entity that names a file gets its own copy of that file's
  globals, and both hooks receive the entity's handle:

```lua
function on_start(handle)          -- once, after the scene loads
function on_update(dt, handle)     -- every frame, dt in seconds
```

Two entities can share a file and still tell themselves apart, which is the
point: a script that counted its own frames would count one cube's frames, not
both. Declaring fewer arguments still works, because Lua discards the rest.

## Assets

`assets/cube.glb` is a real glTF 2.0 binary: 24 vertices, 36 indices, flat normals,
one primitive. It is written by hand (a small script, kept out of the tree) rather
than exported, so it stays small enough to read and review, and the engine's
loader test loads it on every platform - positions, normals, index range and
triangle winding against the vertex normal.

All three mesh entities in `scenes/main.scene.json` point at it:

```json
"meshRenderer": { "mesh": "assets/cube.glb", "color": [0.95, 0.55, 0.2] }
```

`builtin:cube` and `builtin:grid` still work, and a path may end in `#2` to pick
one primitive out of a file with several. Paths are relative to the project
folder, so the project can be moved without editing scenes.

## What to look for

1. The grid: red is the **X** axis, blue is the **Z** axis.
2. The cube drops and settles on the ground — Jolt runs at a fixed 60 Hz, so the
   landing looks the same on a fast and a slow machine.
3. The console prints `t = N s` once a second, proving Lua `on_update(dt)` runs.
4. Press and release Space to see `waypoint.input.is_key_pressed` fire once.

## Where to poke

- `scenes/main.scene.json` — move the cube, change its colour, add another.
- `scripts/main.lua` — the two hooks and the input API.
- `waypoint.project.json` — project name and which scene starts.
