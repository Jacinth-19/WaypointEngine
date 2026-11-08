# external/ — dependency checkouts

`external/` is where third-party source lives. Nothing in here is committed:
each dependency folder is git-ignored, and only these two files are tracked:

- `dependencies.lock.json` — the single source of truth for every pin
  (repository, tag, commit, licence, purpose).
- this README.

## How dependencies arrive

```bash
python tools/wp.py deps fetch     # clones every pin into external/<name>
python tools/wp.py deps status    # 15/15 OK means you are ready to build
```

If a folder is missing, CMake fetches the same pinned commit with FetchContent
into `.cache/waypoint-deps` instead, so a plain `cmake --preset default` also
works — it just downloads on first configure.

## Why there are no git submodules

The scaffold this engine grew from declared 15 submodule paths in `.gitmodules`
but the tree contained **no gitlinks**, so `git submodule update --init` did
nothing and `git clone --recursive` produced a broken build. Two consistent
options existed; we picked the one that matches how the tree actually behaves:

1. **No submodules** (chosen) — `wp.py deps fetch` plus the FetchContent
   fallback, with every pin in the lockfile.
2. Real submodules pinned to the same commits — rejected because 15 submodules
   make a beginner's first clone slow and fragile, `git submodule update` is one
   more command to learn, and the lockfile gives the same reproducibility with
   a readable diff.

There is deliberately no `.gitmodules` in this repository. If you add one, add
real gitlinks in the same commit — a declared-but-missing submodule is the exact
bug this section exists to prevent.

## Overriding a checkout

Point CMake at any folder, ignoring the lockfile:

```bash
cmake --preset default -DWAYPOINT_DEP_GLM_DIR=/path/to/glm
```

Useful when you are patching a dependency locally. `wp.py deps status` will
still report the mismatch, which is the point: it should be obvious that you are
no longer building the pinned code.
