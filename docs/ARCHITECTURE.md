# Architecture

Short version: **small modules, plain data, one direction of dependency.**

```
runtime/  editor/            (executables)
     \       /
      engine/app Application (game loop)
           |
  +--------+--------+----------+----------+
  |        |        |          |          |
scene   render   physics    script     platform
  |        |        |          |          |
  +--------+--------+----------+----------+
           |
        ecs  (World, components)
           |
         core  (types, log, files, uuid)
```

Nothing below depends on anything above it. `core` and `ecs` have no window, GPU
or scripting dependency, which is why the test suite runs in a container.

## Modules

| Module | Responsibility | Notable types |
| --- | --- | --- |
| `engine/core` | scalars, maths aliases, logging, file helpers, UUIDs, assertions | `Transform`, `Log`, `Uuid`, `FileSystem` |
| `engine/ecs` | entities, components, stable handles | `World`, `TransformComponent`, `MeshRendererComponent`, ... |
| `engine/scene` | scene JSON load/save, per-frame systems | `Scene`, `FlyCameraSystem` |
| `engine/project` | `waypoint.project.json` | `Project`, `createProject` |
| `engine/platform` | input state (SDL-free), SDL3 window | `Input`, `Key`, `SdlWindow` |
| `engine/render` | bgfx device, camera, cube, grid | `BgfxRenderer` |
| `engine/physics` | Jolt wrapper, fixed-step boxes | `JoltPhysicsWorld` |
| `engine/script` | Lua 5.4 via sol2 | `LuaScriptEngine` |
| `engine/app` | window + loop + subsystem order | `Application` |

## Entities and handles

An *entity* is an id. Its data lives in components. `World` wraps EnTT and hands
out `EntityHandle` (a 64-bit number) that:

- never changes for the entity's lifetime,
- is never reused after the entity is destroyed,
- can be stored in scene files (via `UuidComponent`) and in the physics layer.

```cpp
EntityHandle crate = world.createEntity("Crate");
world.add<MeshRendererComponent>(crate);
world.add<RigidBodyComponent>(crate);
if (Transform* t = world.transform(crate)) { t->position = Vec3(0, 4, 0); }
```

Every entity automatically gets `HandleComponent`, `NameComponent`,
`UuidComponent` and `TransformComponent`.

## Scenes are data

```json
{
  "name": "main",
  "engineVersion": "0.1.0",
  "entities": [
    {
      "id": "6a1f2b90-3c4d-4e5f-8a7b-1c2d3e4f5061",
      "name": "Cube",
      "transform": {
        "position": [0, 6, 0],
        "rotation": [1, 0, 0, 0],
        "scale": [1, 1, 1]
      },
      "components": {
        "meshRenderer": {"mesh": "builtin:cube", "color": [0.92, 0.55, 0.22]},
        "rigidBody": {"motion": "dynamic", "halfExtents": [0.5, 0.5, 0.5]}
      }
    }
  ]
}
```

Rules the loader enforces, so a broken file is reported instead of guessed at:

- `rotation` is `[w, x, y, z]` and is normalised; an unusable rotation becomes
  identity with a warning.
- `rigidBody.halfExtents` must be positive; `mass` must be positive; `motion` is
  `static`, `dynamic` or `kinematic`.
- `camera` clip planes and field of view must be sane.
- Unknown component names are ignored with a warning, so an older engine can
  open a newer file.

## Coordinates and conventions

- Right-handed, **+Y up**. Distances are metres, angles are degrees in JSON.
- An object's front is its local **+Z** axis (`Transform::forward()`).
- A **camera looks along its local −Z** axis — the standard right-handed
  convention, and what makes "+X is screen-right" true. `FlyCameraSystem`
  builds that rotation from yaw/pitch, pitch clamped to ±89°.
- Clip depth is chosen from `bgfx::getCaps()->homogeneousDepth`:
  `perspectiveRH_NO` for [-1, 1] (OpenGL, Metal), `perspectiveRH_ZO` for [0, 1]
  (Vulkan, D3D).

## Frame order

```
input.beginFrame()            clears "pressed this frame" edges, mouse deltas
window.pollEvents(input)      SDL3 events -> Input
scripts / camera              FlyCameraSystem, Lua on_update(dt)
physics.step(dt)              fixed 1/60 s, at most 4 substeps
physics.syncTransforms(world) dynamic bodies write back into TransformComponent
renderer.beginFrame(camera)   clear, view/projection, grid
renderer.drawWorld(world)     entities with MeshRendererComponent
renderer.endFrame()           bgfx::frame()
```

Physics ordering matters: scripts move kinematic bodies, physics resolves
collisions, then transforms are read back, so rendering always shows the state
that was simulated this frame.

## Adding a component (the usual extension)

1. Add the struct to `engine/ecs/Components.hpp`.
2. Parse and serialise it in `engine/scene/Scene.cpp` (`readEntity` /
   `entityToJson`).
3. Add a test in `tests/test_scene.cpp` for the round-trip.
4. Document it in this file and in `docs/SCRIPTING.md` if scripts can see it.

Component data is a plain struct with defaults; there is no reflection, no
registration step and no base class.
