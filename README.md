# Waypoint Engine

A beginner-friendly C++20 game engine for **urban open-world games**: sector
streaming, LOD and cull zones, entity pools with stable handles, external mission
scripts, and timecycle weather. Original code, Apache-2.0 licensed.

> **Version 0.1.0 — early.** The foundation (ECS, scenes, projects, scripting,
> physics) is real and tested. Rendering is a single lit-nothing cube on a grid,
> and the editor is a scene tree plus a console. See [docs/STATUS.md](docs/STATUS.md)
> for exactly what is verified and what is not. Nothing here is finished.

## Quick start

```bash
python tools/wp.py doctor      # is this machine ready?
python tools/wp.py new MyGame  # create a project
python tools/wp.py build       # build the engine + player
python tools/wp.py run MyGame  # play it
python tools/wp.py test        # run the unit tests
```

On a machine with no GPU or window system (containers, CI):

```bash
WAYPOINT_HEADLESS_BUILD=1 python tools/wp.py test   # configure + build + unit tests
python tools/wp.py run samples/hello_cube --smoke-test  # render one frame, no GPU needed
```

`--smoke-test` uses bgfx's noop backend and SDL's dummy video driver: a window
that never appears, running the real frame loop - create shader programs, issue
draw calls, present - and exiting non-zero if no frame was submitted. It is how
CI proves the render path runs on a machine that cannot show anything.

On a machine with a GPU, save what the player actually drew:

```bash
python tools/wp.py run MyGame --screenshot shot.png   # one frame, then exit
```

The extension picks the format (`.png`, `.bmp`, `.tga`, `.jpg`) and missing
folders are created. The noop backend has no back buffer, so a capture there
fails with an explanation rather than writing an empty file.

## Controls (the `waypoint` player)

| Key | Action |
| --- | --- |
| `W` `A` `S` `D` | move |
| `Space` / `LeftControl` | up / down |
| `Shift` | move faster |
| Right mouse (hold) or `F1` | capture the mouse and look around |
| `Escape` | quit |

## What is in the box

```
engine/core       types, logging, files, UUIDs, assertions
engine/ecs        World (EnTT wrapper), components
engine/scene      scene JSON, fly camera system
engine/project    waypoint.project.json load / save / create
engine/platform   SDL-free Input, SDL3 window
engine/render     bgfx renderer (cube + grid)
engine/physics    Jolt Physics wrapper (boxes, fixed 60 Hz)
engine/script     Lua 5.4 + sol2 script engine
engine/app        the game loop
editor/           Qt 6 Widgets editor shell
runtime/          the `waypoint` player
shaders/          vs_basic.sc, fs_basic.sc, varying.def.sc
tools/wp.py       the one command you need
templates/        blank_project, android placeholder
samples/          hello_cube
tests/            doctest suites (one executable)
docs/             building, architecture, scripting, status, roadmap, ...
```

## Project format

A project is a folder with a manifest:

```json
{
  "name": "MyGame",
  "engineVersion": "0.1.0",
  "startScene": "scenes/main.scene.json",
  "scriptEntry": "scripts/main.lua"
}
```

Scenes are plain JSON with `name` and `entities[]`, and each entity has `id`,
`name`, `transform` (`position`, `rotation` as `[w, x, y, z]`, `scale`) and
`components` (`camera`, `flyCamera`, `directionalLight`, `meshRenderer`,
`rigidBody`, `script`). See [docs/ARCHITECTURE.md](docs/ARCHITECTURE.md).

## Requirements

- CMake ≥ 3.24, Ninja, a C++20 compiler
- Linux desktop: `libx11-dev libxext-dev libxrandr-dev libxinerama-dev
  libxcursor-dev libxfixes-dev libxi-dev libxss-dev libwayland-dev
  libxkbcommon-dev libegl1-mesa-dev libgl1-mesa-dev libvulkan-dev`
- Qt 6 Widgets only if you want the editor

Dependencies are pinned in `external/dependencies.lock.json` and fetched by
`python tools/wp.py deps fetch` (or CMake's FetchContent). No git submodules.

## Legal

Waypoint Engine is original code under the [Apache License 2.0](LICENSE).
The `re3` project (a reverse-engineered GTA III) was used only as an
*architecture* reference; no code, assets, formats or trademarked content from it
— or from any Rockstar game — is present in this repository. Third-party
libraries are permissively licensed and listed in [NOTICE](NOTICE).
"GTA" and other game names are trademarks of their owners; Waypoint Engine is
not affiliated with them.
