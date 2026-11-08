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
