# Roadmap

Ordered so that each step is playable or testable before the next begins.
Anything not listed in `docs/STATUS.md` as verified is still ahead of us.

## 0. Foundation — mostly done

- [x] CMake build with per-subsystem switches, presets, presets-based CLI
- [x] Core / ECS / scene / project modules with tests
- [x] Lua scripting hooks and input API
- [x] Jolt physics wrapper with fixed-step simulation
- [x] SDL3 window and SDL-free input layer
- [x] bgfx renderer: cube, grid, camera, shader-missing fallback
- [x] `wp.py` CLI: doctor, new, build, run, test, export, deps
- [x] 15 pinned dependencies, lockfile, FetchContent fallback
- [x] **Green CI on ubuntu/windows/macos** — all six jobs, plus the Qt 6 editor
- [~] First frame proven on real hardware, with a screenshot in the README
      ← current priority. The frame *loop* is proven: every desktop job runs
      `waypoint samples/hello_cube --smoke-test` and reports 1 frame, 3 draw
      calls, shaders loaded. What is missing is a GPU: noop rasterises nothing,
      and no window has ever been mapped. `--screenshot shot.png` is ready for
      whoever has hardware.

## 1. Assets

- [x] glTF loading with tinygltf (CPU-side geometry, node transforms, clear errors)
- [~] vertex/index buffers per primitive — the arrays are there; the renderer still
      builds its own cube and does not consume them yet
- [ ] Mesh registry replacing the hardcoded cube path
- [ ] Material assets (metallic/roughness), texture loading via stb/bimg
- [ ] Asset paths resolved relative to the project, with clear errors
- [ ] Test: load a .glb in the headless suite (geometry only)

## 2. Lighting

- [ ] Directional light with cascaded shadow maps
- [ ] PBR shading (metallic/roughness, Cook–Torrance)
- [ ] HDR framebuffer + tonemap
- [ ] Image-based lighting / reflection probes
- [ ] Clustered forward lighting for many local lights
- [ ] Timecycle: time-of-day blend driving sun colour and sky

## 3. Editor

- [ ] Inspector for Transform / Camera / Light / MeshRenderer / RigidBody / Script
- [ ] Undo/redo command stack
- [ ] Project browser (scenes, scripts, assets)
- [ ] Play/stop in-editor
- [ ] 3D viewport sharing the engine renderer

## 4. Open-world foundation

- [ ] Sector streaming (load/unload cells around the player)
- [ ] LOD chain switching and cull zones
- [ ] Prefabs (entity templates) with stable ids
- [ ] Entity pools for traffic/pedestrian-style crowds
- [ ] Character controller, then vehicle controller
- [ ] Audio (miniaudio or SDL audio) and a UI layer
- [ ] Mission scripting: per-entity `ScriptComponent` instances and events

## 5. Export

- [ ] Asset cooking (shrink + pack textures, meshes)
- [ ] Desktop packaging polish (icons, version info, code signing notes)
- [ ] Android APK/AAB beyond the placeholder template

## Deliberately out of scope for 0.x

- Networking / multiplayer
- Console platforms
- In-engine animation authoring
- Anything requiring copied game content or formats

## How items get added

An item moves out of this file only when `docs/STATUS.md` can say how it was
verified. "Implemented" is not "done".
