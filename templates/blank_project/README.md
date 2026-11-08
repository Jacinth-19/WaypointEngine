# __PROJECT_NAME__

A Waypoint Engine project.

```bash
python tools/wp.py run .      # play it
python tools/wp.py export .   # package it
```

## What is where

- `waypoint.project.json` — the project manifest: name, start scene and start script.
- `scenes/main.scene.json` — the level. Entities, transforms and components are plain JSON.
- `scripts/main.lua` — gameplay code. `on_start()` runs once, `on_update(dt)` every frame.

## Controls

| Key | Action |
| --- | --- |
| W A S D | move |
| Space / Left Ctrl | up / down |
| Shift | move faster |
| Right mouse (hold) or F1 | capture the mouse for looking |
| Escape | quit |

## Next steps

1. Open `scenes/main.scene.json` and move the `Cube` to a different position.
2. Add `waypoint.log("hello")` to `on_update` and watch the console.
3. Copy the `Cube` entry to add a second box, giving it a fresh `id`.
