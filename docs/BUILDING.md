# Building Waypoint Engine

## The one command

```bash
python tools/wp.py build           # debug-friendly build of engine + runtime
python tools/wp.py build --release # optimised
python tools/wp.py build --editor  # also build the Qt editor
```

`wp.py` finds CMake and Ninja on `PATH`, then in `~/waypoint-tools/bin`, then in
`<repo>/.waypoint-tools/bin`. If it cannot find them it tells you exactly what to
install.

## Doing it by hand

```bash
cmake --preset default
cmake --build --preset default
```

Presets (in `CMakePresets.json`):

| Preset | What it is |
| --- | --- |
| `default` | RelWithDebInfo, everything on, tests built |
| `release` | Release, no editor |
| `editor` | Debug + Qt 6 editor |
| `headless` | No window, no GPU, no Qt — for containers and CI |

## Options

| CMake option | Default | Meaning |
| --- | --- | --- |
| `WAYPOINT_BUILD_RUNTIME` | ON | build the `waypoint` player |
| `WAYPOINT_BUILD_EDITOR` | OFF | build the Qt 6 editor |
| `WAYPOINT_BUILD_TESTS` | ON | build the test suite |
| `WAYPOINT_ENABLE_RENDER` | ON | bgfx + SDL3 rendering |
| `WAYPOINT_ENABLE_PHYSICS` | ON | Jolt Physics |
| `WAYPOINT_ENABLE_SCRIPTING` | ON | Lua + sol2 |
| `WAYPOINT_WARNINGS_AS_ERRORS` | OFF | treat warnings as errors |
| `WAYPOINT_BUILD_BGFX_TOOLS` | ON | build `shaderc` so shaders compile |
| `WAYPOINT_HEADLESS_BUILD` | OFF | force render/runtime/editor off |
| `WAYPOINT_RENDERER` | `auto` | preferred backend baked in as the default |

Each switch also sets a `WAYPOINT_HAS_*` define so C++ code can ask
`#if WAYPOINT_HAS_PHYSICS` instead of guessing.

Example — engine only, no GPU, no scripts:

```bash
cmake -S . -B build/tiny -G Ninja \
      -DWAYPOINT_HEADLESS_BUILD=ON -DWAYPOINT_ENABLE_SCRIPTING=OFF
cmake --build build/tiny
```

## Prerequisites per platform

**Linux (Debian/Ubuntu)**

```bash
sudo apt install build-essential cmake ninja-build git \
     libx11-dev libxext-dev libxrandr-dev libxinerama-dev libxcursor-dev \
     libxfixes-dev libxi-dev libxss-dev libwayland-dev libxkbcommon-dev \
     libegl1-mesa-dev libgl1-mesa-dev libvulkan-dev
# editor only:
sudo apt install qt6-base-dev
```

**macOS** — `brew install cmake ninja`, and Xcode command line tools. The editor
needs `brew install qt`. bgfx uses Metal.

**Windows** — Visual Studio 2022 (with "Desktop development with C++"), CMake and
Ninja (`winget install Kitware.CMake Ninja-build.Ninja`). Build from a *Developer
Command Prompt* or with `wp.py`, which locates the tools itself.

## Containers and CI

Set `WAYPOINT_HEADLESS_BUILD=1`. Rendering, the player and the editor are turned
off; the engine library and tests still build, and physics/scripting still run.

```bash
WAYPOINT_HEADLESS_BUILD=1 python tools/wp.py test
```

The build is not headless-safe with `WAYPOINT_ENABLE_RENDER=ON`: bgfx.cmake calls
`find_package(X11 REQUIRED)` on Linux even for a tools-only build, so configure
fails before compiling anything.

## Troubleshooting

| Symptom | Fix |
| --- | --- |
| `Could NOT find X11` | install the X11 dev packages, or build headless |
| `shaderc` build takes minutes | expected; it compiles glslang/SPIRV-Tools. Set `-DWAYPOINT_BUILD_BGFX_TOOLS=OFF` if you only need C++ |
| `SDL3::SDL3` not found | `python tools/wp.py deps fetch`, then re-run configure |
| Shader error about `bgfx_shader.sh` | shaderc must be given `INCLUDE_DIRS <bgfx>/src`; that is handled in `cmake/WaypointShaders.cmake` |
| CMake cache errors after moving a virtualenv | delete the `build/` folder and reconfigure |
| Qt not found | Qt is optional: leave `WAYPOINT_BUILD_EDITOR` OFF, or set `CMAKE_PREFIX_PATH` to your Qt install |
