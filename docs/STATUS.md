# Status — 2026-11-08

Honest separation of **verified** (actually executed) from **implemented but
unverified**. If it is not in the first table, do not assume it works.

## Verified

Environment: Debian 12 container, gcc 12.2 and Clang 21 (via zig's libc++),
CMake 4.4.4, Ninja 1.13, Python 3.11. No X11, OpenGL, Vulkan or Qt installed — so
no window can ever open here. Anything needing a driver, a display or a
different OS is verified on GitHub runners instead, and the CI table below is the
authority for those.

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

### CI (GitHub Actions, commit `02098c9`)

Every job below ran to completion on GitHub's own runners:

| Job | Runner | Result |
| --- | --- | --- |
| Core + tests | ubuntu-latest, gcc | **pass** — configure, build, `ctest`, `doctor`, `deps status` |
| Core + tests | macos-latest, AppleClang | **pass** |
| Core + tests | windows-latest, MSVC 14.51 (static CRT) | **pass** |
| Desktop runtime | ubuntu-latest — bgfx + SDL3 + shaderc + Jolt + Lua, shaders compiled | **pass** |
| Desktop runtime | macos-latest — spirv/glsl/essl/metal | **pass** |
| Desktop runtime | windows-latest — spirv/glsl/essl/dxbc | **pass** |
| **One frame rendered** | all three desktop jobs: `waypoint samples/hello_cube --smoke-test` with `WAYPOINT_RENDERER=noop` and `SDL_VIDEODRIVER=dummy` | **pass** — a window was created, shader programs built, draw calls submitted and the frame presented, verified by the player exiting 0 and logging the backend. The player asserts all three itself, so a "shaders missing" fallback now fails the job instead of passing quietly. |
| Editor (Qt 6) | ubuntu-latest — build + offscreen start | **pass** |

## Implemented but unverified

Anything here compiles and links, but has never been **run** by a human or by
CI, and is not promised to work:

| Area | Why unverified |
| --- | --- |
| Pixels on a real GPU | the frame is proven through bgfx's noop backend, which validates the submit path but rasterises nothing. No CI runner has a GPU, and nobody has looked at the cube on a machine that does. |
| Shader execution | the `.bin` files are loaded and handed to bgfx, but no backend has ever compiled or run them |
| glTF on a real backend | `samples/hello_cube` draws `assets/cube.glb`, and CI's smoke test proves the file is parsed, uploaded and drawn - through noop, which rasterises nothing |
| A real GPU backend | `WAYPOINT_RENDERER` selects vulkan / opengl / opengles / d3d11 / d3d12 / metal, but CI can only run `noop`. Only `dxbc` and `spirv`/`glsl`/`essl`/`metal` shaders are compiled by default, and no GPU has executed them. |
| A windowed session | the smoke test uses SDL's dummy video driver; no window has ever been mapped on a real desktop |
| Runtime on Windows/macOS | the desktop jobs build, link *and* run the runtime (one noop frame), but nothing interactive has been tried |
| Qt 6 editor interaction | CI starts the editor offscreen and checks it does not crash; nobody has clicked a button |
| `wp.py export` output | needs a real runtime binary and is refused under headless; packaging is unverified |
| `--screenshot` capture | the writer is tested (write, decode, compare bytes) but no GPU has ever delivered pixels to it; noop has no back buffer and says so |
| Android NDK build | workflow exists but has never been run |
| dxil / D3D12 shaders | not compiled by default (see below) |

## Known gaps

- No glTF/mesh loading: `meshRenderer.mesh` accepts `builtin:cube` only, other
  names fall back to a cube with a debug log.
- No lighting or materials pass: the cube is vertex-coloured with a per-entity
  tint. `directionalLight` is parsed and stored but does not light anything.
- No shadows, HDR, tonemap, IBL or clustered lighting yet.
- No textures, audio, asset cooking, or sector streaming.
- Editor: new/open project, scene tree, console, add/delete entity, save scene.
- **d3d12 backend**: dxil shader bytecode is not built. DXC rejects our vertex
  shader through the spirv-cross HLSL path
  (`hlsl.hlsl:624:101: error: expression result unused`), while the same shader
  compiles for dxbc and the fragment stage compiles for dxil. `-DWAYPOINT_COMPILE_DXIL=ON`
  re-enables the attempt; the D3D11 (dxbc) backend on Windows is unaffected.
  No inspector, viewport, undo/redo or play mode.
- Scripts are project-level: `ScriptComponent` is loaded but per-entity script
  instances are not created yet.
- No character controller; the camera is a free-fly camera only.
- Android project template is a placeholder.

## How this file stays honest

Update it in the same commit as the change it describes. A green local test run
is not evidence about a platform you did not run it on; keep the two tables
separate and say which machine produced the numbers.
