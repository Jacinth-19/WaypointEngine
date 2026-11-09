# Changelog

All notable changes to Waypoint Engine. Format follows
[Keep a Changelog](https://keepachangelog.com/en/1.1.0/); versions follow
[Semantic Versioning](https://semver.org/).

## [Unreleased]

### Fixed
- Per-entity scripts now run. A scene's `ScriptComponent.path` was loaded from
  JSON and written back out, but nothing ever resolved or executed it - only the
  manifest's single `scriptEntry` ran, so a scene author's per-entity Lua was
  silently dead. Each scripted entity now gets its own Lua environment, and both
  hooks receive the entity's handle: `on_start(handle)` and
  `on_update(dt, handle)`. Scripts that declare fewer arguments keep working,
  because Lua discards extras.

### Added
- Meshes named by a scene are loaded and drawn. `MeshRendererComponent.mesh`
  accepts `builtin:cube`, `builtin:grid` or a glTF path relative to the project
  (`assets/cube.glb`, or `assets/cube.glb#2` for one primitive). Files are
  uploaded once and cached for the renderer's lifetime; a reference that cannot
  be loaded is reported once and the entity is skipped, rather than quietly
  drawing a cube in its place. hello_cube now draws its own `assets/cube.glb`,
  so CI's smoke test exercises the whole path - file, upload, draw call.
- `engine/render/MeshGeometry.{hpp,cpp}`: the vertex format and the
  glTF-to-vertex conversion, outside the renderer so the headless suite can
  check that positions and colours survive the trip (6 new cases, 76 total).
- glTF geometry loading: `engine/assets/GltfLoader.{hpp,cpp}` reads `.glb` and
  `.gltf` files into positions, normals, COLOR_0 and 32-bit indices, applies the
  node hierarchy's transform so an asset lands where it was placed, and reports
  missing files, unreadable accessors and out-of-range indices instead of
  throwing. CPU-side only: turning these arrays into GPU buffers is the next
  step, and the headless suite loads a real `.glb` (5 new cases, 70 total).
- `samples/hello_cube/assets/cube.glb` — a hand-written glTF binary cube, 24
  vertices and 36 indices, used by those tests.
- `waypoint <project> --screenshot FILE` (and `wp.py run <project> --screenshot FILE`)
  renders a few frames, saves one to FILE and exits. The extension chooses the
  format - .png, .bmp, .tga, .jpg - and parent folders are created on demand, so
  a screenshot can be dropped straight into the README.
- `engine/core/ImageWrite.{hpp,cpp}`: the RGBA-to-file writer, kept out of the
  renderer so the headless suite can round-trip an image (write, decode with
  stb_image, compare every byte) on all three platforms without a GPU.

### Fixed
- The player looked for `shaders/<profile>/vs_basic.bin`, but bgfx's shaderc
  writes the output after the *source* name - `vs_basic.sc.bin`. No shader was
  ever loaded on any backend, so the renderer always fell back to its "shaders
  missing" screen and the cube never appeared. Found by making the smoke test
  assert that a shader program was created, which it now does.
- `shaderFolderFor()` mapped both D3D11 and D3D12 to `dxil`. D3D11 uses `dxbc`
  and `dxil` is only compiled with `-DWAYPOINT_COMPILE_DXIL=ON`, so the cube
  silently vanished on the default Windows backend.

### Added
- `waypoint <project> --smoke-test` (and `wp.py run <project> --smoke-test`):
  renders exactly one frame through bgfx's noop backend with SDL's dummy video
  driver and exits non-zero if no frame was submitted. CI runs it on all three
  desktop jobs, which is the first automated proof that the frame loop works:
  window created, shader programs built, draws submitted, frame presented.
- `WAYPOINT_RENDERER=noop` selects that backend explicitly.
- `JoltPhysicsWorld`: fixed 60 Hz simulation, box bodies, static/dynamic/kinematic
  motion, transform write-back, body removal, and Jolt trace/assert routing into
  the engine log.
- Tests for physics: falling box lands on static ground, substep cap, invalid
  settings, replace-on-re-add.
- `docs/STATUS.md` separating verified from implemented-but-unverified work.
- `cmake/patches/`: tracked local patches applied identically by `wp.py deps
  fetch` and CMake, currently one fix so pinned sol2 v3.3.0 compiles with Clang.

### CI
- `Build` is green on all six jobs: core + tests on ubuntu (gcc), macOS
  (AppleClang) and Windows (MSVC), and the desktop runtime — bgfx, SDL3, shaderc,
  Jolt, Lua, compiled shaders — on all three. `Editor (Qt 6)` is green too.
- A failing job now posts its error lines as check-run annotations and a commit
  comment, so failures are readable without downloading the log artifact.
- Documentation-only pushes skip the build matrix.

### Fixed
- One CRT for the whole build (`CMAKE_MSVC_RUNTIME_LIBRARY`), because Jolt ships
  the static MSVC runtime and mixing runtimes is an LNK2038 link error.
- Shader profiles are chosen per platform explicitly; dxil (D3D12) is opt-in via
  `WAYPOINT_COMPILE_DXIL` because DXC rejects the vertex shader's varying write.
- Tests no longer assume POSIX absolute paths, so they pass on Windows.
- Missing `<functional>`/`<string_view>` includes that libstdc++ provided
  transitively but libc++ and MSVC do not.
- `sol::type::nil` only exists when the platform defines no `nil` macro; the
  engine now uses the always-present `sol::type::lua_nil`.
- CI on Windows now builds with MSVC, which is the toolchain the docs promise,
  instead of whatever compiler CMake found first on `PATH`.
- Failing CI jobs now publish their error lines as check-run annotations and a
  commit comment, because raw job logs are only reachable as an artifact.

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
