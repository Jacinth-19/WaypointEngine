# Status — 2026-11-08

Honest separation of **verified** (actually executed) from **implemented but
unverified**. If it is not in the first table, do not assume it works.

## Verified

Environment: Debian 12 container, gcc 12.2, CMake 4.4.4, Ninja 1.13, Python
3.11. No X11, OpenGL, Vulkan or Qt installed — so no window can ever open here.

| What | How | Result |
| --- | --- | --- |
| Headless configure | `cmake -B build/headless -DWAYPOINT_HEADLESS_BUILD=ON` (scripting + physics ON) | OK |
| Headless build | `WAYPOINT_HEADLESS_BUILD=1 python tools/wp.py test` (fresh configure from `external/` checkouts) | OK, ctest 1/1 passed |
| Test suite | `./build/headless/bin/waypoint_tests` | **58 cases, 2185 assertions, 0 failures** |
| CLI: doctor / new / deps / export refusal | `python tools/wp.py ...` | works; export correctly refuses under headless with exit code 1 |
| Workflow YAML | parsed with PyYAML | all 4 workflows parse |
| Dependency pins | `python tools/wp.py deps fetch` then `deps status` | **15/15 match the lockfile** |
| Lua scripting | `tests/test_scripting.cpp` — hooks, `waypoint.log/warn/error`, input queries, syntax/runtime errors, missing files, reset | passing |
| Jolt physics | `tests/test_physics.cpp` — falling box lands on static ground, fixed 60 Hz stepping, substep cap, body add/remove | passing |
| Scene JSON | round-trip, defaults, normalisation, 7 failure modes | passing |
| Project manifests | create/load/save, missing + malformed + wrong-typed fields | passing |
| Shipped content | every `waypoint.project.json` under `samples/` and `templates/` loads, has a camera and a light | passing |
| Second compiler | full headless build with Clang 21 + libc++ (as a local stand-in for AppleClang) | builds, **58 cases / 2185 assertions pass** |
| Release build (asserts off) | clang, `CMAKE_BUILD_TYPE=Release`, headless | `waypoint_engine` builds warning-free |
| Renderer/window sources | `g++ -std=c++20 -fsyntax-only` against pinned bgfx, bx, bimg, SDL3, Jolt, Lua, sol2, glm, EnTT, spdlog headers | `BgfxRenderer.cpp`, `SdlWindow.cpp`, `Application.cpp`, `runtime/main.cpp` all clean |

### CI matrix (GitHub Actions, run on commit `e32d249`)

| Job | Result |
| --- | --- |
| Core + tests (ubuntu-latest, gcc) | **pass** |
| Core + tests (macos-latest, AppleClang) | **pass** |
| Desktop runtime (ubuntu-latest) — bgfx + SDL3 + shaderc + Jolt + Lua, shaders compiled to `.bin` | **pass** |
| Desktop runtime (macos-latest) — same, metal/spirv/glsl/essl profiles | **pass** |
| Editor (Qt 6) | **pass** |
| Core + tests (windows-latest, MSVC) | fail: `<ostream>`-less `string_view` streaming in a test (fixed since) |
| Desktop runtime (windows-latest) | fail: HLSL (dxil) shader compile |

## Implemented but unverified

These compile in the sense above but have **never been linked or run** here, and
some have never been configured at all:

| Area | Why unverified |
| --- | --- |
| bgfx window + first frame | the CI desktop jobs link and compile shaders, but no job opens a window or renders a frame — there is no GPU on the runners and no smoke run yet |
| Runtime run on Windows | Windows desktop job still fails to compile the dxil (D3D12) shader profile |
| Qt 6 editor *running* | Qt 6 is not installed here; CI builds and starts the editor offscreen, this environment cannot |
| Windows core tests passing | last run failed at compile (see matrix), so no Windows test run has completed |
| Android NDK build | not installed |
| `wp.py export` output | needs a real runtime binary; refused under headless (verified refusal only) |
| GitHub Actions workflows | written, YAML-checked, **not executed** |

## Known gaps

- No glTF/mesh loading: `meshRenderer.mesh` accepts `builtin:cube` only, other
  names fall back to a cube with a debug log.
- No lighting or materials pass: the cube is vertex-coloured with a per-entity
  tint. `directionalLight` is parsed and stored but does not light anything.
- No shadows, HDR, tonemap, IBL or clustered lighting yet.
- No textures, audio, asset cooking, or sector streaming.
- Editor: new/open project, scene tree, console, add/delete entity, save scene.
  No inspector, viewport, undo/redo or play mode.
- Scripts are project-level: `ScriptComponent` is loaded but per-entity script
  instances are not created yet.
- No character controller; the camera is a free-fly camera only.
- Android project template is a placeholder.

## How this file stays honest

Update it in the same commit as the change it describes. A green local test run
is not evidence about a platform you did not run it on; keep the two tables
separate and say which machine produced the numbers.
