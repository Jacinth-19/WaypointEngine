# Changelog

All notable changes to Waypoint Engine. Format follows
[Keep a Changelog](https://keepachangelog.com/en/1.1.0/); versions follow
[Semantic Versioning](https://semver.org/).

## [Unreleased]

### Added
- `JoltPhysicsWorld`: fixed 60 Hz simulation, box bodies, static/dynamic/kinematic
  motion, transform write-back, body removal, and Jolt trace/assert routing into
  the engine log.
- Tests for physics: falling box lands on static ground, substep cap, invalid
  settings, replace-on-re-add.
- `docs/STATUS.md` separating verified from implemented-but-unverified work.

## [0.1.0] — 2026-11-08

The first scaffold: enough engine to open a window, move a camera, drop a cube,
and script it.

### Added
- CMake build with per-subsystem switches (`WAYPOINT_ENABLE_RENDER`,
  `WAYPOINT_ENABLE_PHYSICS`, `WAYPOINT_ENABLE_SCRIPTING`, `WAYPOINT_HEADLESS_BUILD`)
  and `default` / `release` / `editor` / `headless` presets.
- 15 pinned dependencies resolved from `external/dependencies.lock.json`, via
  local `external/` checkouts or FetchContent at the same commit. No submodules.
- `engine/core`: types, logging (spdlog), file helpers, UUIDs, assertions.
- `engine/ecs`: `World` over EnTT with stable handles; components for transform,
  camera, fly camera, directional light, mesh renderer, rigid body, script.
- `engine/scene`: scene JSON load/save with validation and round-trip tests.
- `engine/project`: `waypoint.project.json` load/save/create.
- `engine/platform`: SDL-free `Input` with case-insensitive key names, plus an
  SDL3 window with relative mouse mode and native handle plumbing.
- `engine/render`: bgfx renderer (vertex-coloured cube, axis-tinted grid, correct
  depth convention per backend, debug-text fallback when shaders are missing).
- `engine/script`: Lua 5.4 + sol2 engine with `on_start`/`on_update`, protected
  hooks, `waypoint.log/warn/error`, `waypoint.input.*`.
- `engine/app`: game loop with clamped delta, fly camera, fixed-step physics and
  script hooks.
- `runtime`: the `waypoint` player with `--scene`, `--version`, `--help`.
- `editor`: Qt 6 shell — new/open project, scene tree, add/delete entity, save
  scene, console dock.
- `tools/wp.py`: `doctor`, `new`, `build`, `run`, `test`, `export`,
  `deps status`, `deps fetch` (Python standard library only).
- Templates (`blank_project`, `android` placeholder), `samples/hello_cube`.
- Four GitHub Actions workflows: build, editor (manual), release (on `v*` tags),
  android (manual).
- Docs: BUILDING, ARCHITECTURE, SCRIPTING, DEPENDENCIES, PERFORMANCE, ANDROID,
  RELEASING, ROADMAP, STATUS.

### Verified
- Headless configure, build and 58 doctest cases / 2185 assertions passing on
  Linux with scripting and physics enabled.
- Renderer, window, application and runtime sources pass `-fsyntax-only` against
  the pinned bgfx, SDL3, Jolt, Lua, sol2, glm, EnTT and spdlog headers.

### Not verified
- Windowed run, shader compilation, Windows/macOS builds, the Qt editor build,
  Android, and any CI execution. See `docs/STATUS.md`.
