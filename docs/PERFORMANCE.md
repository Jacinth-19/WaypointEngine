# Performance

Target: **60 FPS at 1080p on integrated GPUs**, reached by giving players knobs
instead of demanding new hardware. Nothing in this document is a measured
promise yet — the engine is at its first frame, so treat the numbers as budgets to
hit, not results.

## Frame budget at 60 FPS

| Slice | Budget | Notes |
| --- | --- | --- |
| Input + events | 0.5 ms | SDL3 event drain |
| Scripts | 1.0 ms | Lua 5.4, one entry script today |
| Physics | 2.0 ms | 1/60 s fixed step, up to 4 substeps |
| Culling + draw submission | 2.0 ms | grows with entity count |
| GPU | 11.0 ms | fill-rate bound on integrated GPUs |
| Headroom | ~0.5 ms | |

16.6 ms total. On integrated GPUs, fill rate is almost always the limiter, which
is why the scalability knobs below all reduce pixels or samples.

## Why the frame is structured this way

- **Fixed physics timestep (1/60 s, max 4 substeps).** Simulation stays
  repeatable at any frame rate. A 20 FPS frame runs three steps; a 240 FPS frame
  runs none. `step()` clamps accumulated time to the substep budget, so a hitch
  cannot snowball into a freeze.
- **Delta clamping in the loop** (0.1 s) stops a debugger pause from teleporting
  the player.
- **One static library, no exceptions across module boundaries** in hot paths;
  components are plain structs stored in EnTT pools (contiguous, cache friendly).
- **Stable handles** avoid the usual "entity id reused after destroy" bugs and
  let the physics layer key bodies by a u64 without re-validation.

## Scalability knobs

Planned (and the shape they will take), in order of impact on integrated GPUs:

1. **Render scale / resolution** — 0.5–1.0 of the window, the biggest lever.
2. **Shadow map resolution** and cascade count (off → 1024 → 2048).
3. **Draw distance / far clip** — in a city, halves the visible sector count.
4. **LOD bias** — shifts the switch distance for all meshes at once.
5. **VSync toggle** — free CPU headroom, at the cost of tearing.
6. **Physics body count / sleeping** — Jolt already sleeps idle bodies; cap
   dynamic bodies per sector.

## Current known costs

- The grid is a line list of ~1,600 vertices submitted every frame; negligible,
  but it is a full draw call.
- Each mesh entity is its own `bgfx::submit`. Sorting/batching by mesh arrives
  with the mesh registry; at a few dozen entities this is not the bottleneck.
- `stderr` logging is synchronous. Debug builds that log per frame will show up
  in profiles; use `Log::setLevel(LogLevel::Warn)` when measuring.

## How to measure

```bash
python tools/wp.py build --release
```

- **CPU:** build RelWithDebInfo and use `perf record` (Linux), Instruments (macOS)
  or Superluminal/PIX (Windows) on `waypoint`.
- **GPU:** bgfx's debug text (`F1`-style overlay once wired up) or RenderDoc.
  RenderDoc captures work with bgfx on Vulkan/D3D; capture one frame of
  `samples/hello_cube`.
- **Frame time in engine terms:** the loop already measures `dt`; a rolling
  average HUD is a planned addition to `BgfxRenderer`.

## Rules for contributions

- Physics and rendering work happens in fixed-size, allocation-free loops where
  practical.
- No per-frame `std::string` churn in the hot path; log at `debug` only.
- Every new subsystem must have an off switch (`WAYPOINT_ENABLE_*`) so
  low-end targets can drop it rather than suffer it.
