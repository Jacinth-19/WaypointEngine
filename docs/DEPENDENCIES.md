# Dependencies

Every third-party library is pinned by commit in
[`external/dependencies.lock.json`](../external/dependencies.lock.json). That file
is the single source of truth: CMake reads it, `wp.py` reads it, and this
document is written from it.

## The 15 pins

| Name | Repository | Tag | Commit | Licence | Why |
| --- | --- | --- | --- | --- | --- |
| entt | skypjack/entt | v3.15.0 | `d4014c74dc3793aba95ae354d6e23a026c2796db` | MIT | entity component system |
| glm | g-truc/glm | 1.0.1 | `0af55ccecd98d4e5a8d1fad7de25ba429d60e863` | MIT | vectors, matrices, quaternions |
| json | nlohmann/json | v3.12.0 | `55f93686c01528224f448c19128836e7df245f72` | MIT | scene and project JSON |
| spdlog | gabime/spdlog | v1.15.3 | `6fa36017cfd5731d617e1a934f0e5ea9c4445b13` | MIT | logging |
| doctest | doctest/doctest | v2.4.12 | `1da23a3e8119ec5cce4f9388e91b065e20bf06f5` | MIT | tests |
| sdl | libsdl-org/SDL | release-3.2.10 | `877399b2b2cf21e67554ed9046410f268ce1d1b2` | Zlib | window, keyboard, mouse |
| lua | lua/lua | v5.4.7 | `1ab3208a1fceb12fca8f24ba57d6e13c5bff15e3` | MIT | scripting runtime |
| sol2 | ThePhD/sol2 | v3.3.0 | `eba86625b707e3c8c99bbfc4624e51f42dc9e561` | MIT | C++ ↔ Lua bindings |
| jolt | jrouwe/JoltPhysics | v5.3.0 | `0373ec0dd762e4bc2f6acdb08371ee84fa23c6db` | MIT | rigid body physics |
| bgfx_cmake | bkaradzic/bgfx.cmake | v1.167.9614-607 | `947910c474d6ed7e34bf0152bff611ea54895e41` | CC0-1.0 | CMake build of bgfx |
| bgfx | bkaradzic/bgfx | master | `fbceed8df882df4f13d629ee5c4aa4a096a1addf` | BSD-2-Clause | rendering |
| bx | bkaradzic/bx | master | `f859f66472aabacc2847e7d7b04168e7146e9f98` | BSD-2-Clause | bgfx base library |
| bimg | bkaradzic/bimg | master | `388a5e637c63e472158c940317c68c735bca4795` | BSD-2-Clause | image loading |
| stb | nothings/stb | master | `4e9b368bfc071e97206ea5c6adba68dd0cbf8897` | MIT | image decode |
| tinygltf | syoyo/tinygltf | v2.9.5 | `b956fa3e9deeb7ee7f0f22c08bda9b3a684a98dc` | MIT | glTF models |

Licence texts and attribution are repeated in [NOTICE](../NOTICE).

## How they are resolved

```
python tools/wp.py deps fetch        ->  external/<name>   (one checkout per pin)
                 otherwise CMake     ->  .cache/waypoint-deps  (FetchContent)
```

CMake's order of preference in `cmake/WaypointDependencies.cmake`:

1. `-DWAYPOINT_DEP_<NAME>_DIR=<path>` — explicit override, e.g. a local patch.
2. `external/<name>/.git` — what `wp.py deps fetch` produces.
3. `FetchContent` at the pinned commit, into `.cache/waypoint-deps`
   (`FETCHCONTENT_BASE_DIR`, which CI caches).

`external/*/` is git-ignored; only the lockfile and `external/README.md` are
tracked. Build artifacts stay out of git: `build/`, `dist/`, `.cache/`.

## Why not git submodules

The original scaffold declared 15 submodule paths in `.gitmodules` but contained
**no gitlinks**. `git submodule update --init` therefore did nothing, and a
`--recursive` clone produced a tree that could not build.

We chose to **remove submodule support entirely** and rely on
`wp.py deps fetch` + FetchContent, because that is what the tree actually did.
The alternative — registering real submodules at the pinned commits — was
rejected: 15 submodules make the first clone slow and add a command beginners
must learn, while the lockfile gives identical reproducibility with a readable
diff.

There is no `.gitmodules` in this repository. If one is ever added, real
gitlinks must land in the same commit.

## Rules

- **Permissive licences only** (MIT, BSD, Zlib, CC0, public domain). No GPL, no
  unlicensed code. re3 was an *architecture* reference only and none of its
  source is present.
- Pins are commits, never floating branches, so a rebuild in a year produces the
  same code. `tag` is recorded for humans.
- Adding a dependency means: lockfile entry + `NOTICE` line + a `waypoint_dependency_*`
  function in `cmake/WaypointDependencies.cmake` + a row in the table above.

## Local patches

One pin carries a local patch, in tracked form, applied identically by
`wp.py deps fetch` and by CMake:

| Dependency | Patch | Why |
| --- | --- | --- |
| sol2 v3.3.0 | `cmake/patches/sol2-optional-reference-emplace.patch` | `sol::optional<T&>::emplace()` calls a `construct` member the reference specialization does not have. GCC never instantiates it; Clang rejects the whole header, so macOS could not build at all. |

Details and removal instructions: [`cmake/patches/README.md`](../cmake/patches/README.md).
A patch is only acceptable when the pin cannot compile on a supported compiler;
if a pin is ever moved, its patches are deleted in the same change.

`python tools/wp.py deps status` shows the count, so an unpatched checkout is
visible instead of surfacing later as a compiler error:

```
  OK    sol2         eba86625b707  (v3.3.0)  + 1 local patch
```

## Checking state

```bash
python tools/wp.py deps status
#   15/15 dependencies match the lockfile
```

`status` compares each checkout's `HEAD` with the lockfile, so a checkout that
drifted is visible before it becomes a mysterious build failure.

## Gotchas that cost us time

- **bgfx.cmake** calls `find_package(X11 REQUIRED)` on Linux even for a
  tools-only build; headless CI must disable rendering.
- **bgfx headers are split** across bgfx/bx/bimg checkouts; bgfx links bx
  `PRIVATE`, so bx headers must be added to the interface if you call `bx::`.
- **Lua's sources are at the repo root.** `lua.c`, `luac.c` and `ltests.c` are
  standalone programs/tests and must be excluded from the library.
- **Jolt** needs `RegisterDefaultAllocator()` and a valid `JPH::Trace` before
  `RegisterTypes()`, otherwise its own assertion path crashes.
- **sol2 v3.3.0**: `state.create_named_table("waypoint")` on the state, then
  `table.create_named("input")` on the table. (`create_named` does **not** exist
  on `sol::state` in this version.)
- **Skinny clones**: `git clone --depth 1` cannot target an arbitrary commit when
  the server refuses SHA fetches; `wp.py deps fetch` falls back to a full fetch.
