# Local dependency patches

A pin is supposed to be pristine upstream code. Patching a dependency is
therefore the exception, not the workflow: a pin is only patched when it cannot
compile at all on a supported compiler, and the patch lives here as a tracked
unified diff so it is reviewable, reproducible, and easy to drop.

Both dependency paths apply every file in this directory, so a checkout ends up
byte-identical whether it came from `wp.py deps fetch` or CMake's FetchContent:

- `tools/wp.py deps fetch` applies `cmake/patches/<name>-*.patch` after each
  checkout (and to checkouts that were already present).
- `cmake/WaypointDependencies.cmake` applies the same files in
  `waypoint_apply_dependency_patches()`.

Application is idempotent (`git apply --check --reverse` decides), so re-running
either path is a no-op. `python tools/wp.py deps status` reports the patch count
next to the pin:

```
  OK    sol2         eba86625b707  (v3.3.0)  + 1 local patch
```

## What is patched, and why

### `sol2-optional-reference-emplace.patch` (sol2 v3.3.0)

`sol/include/sol/optional_implementation.hpp` has a reference specialization of
`sol::optional<T&>` whose `emplace()` calls `this->construct(...)`. That member
only exists in the primary template, so the call is ill-formed. GCC defers the
lookup until instantiation and never instantiates `emplace()` for a reference, so
the header compiles by accident. Clang diagnoses the non-existent member while
parsing the header, which means *every* translation unit that includes
`sol/sol.hpp` fails on macOS (AppleClang) and on Windows when CMake picks clang
instead of MSVC:

```
sol/optional_implementation.hpp:2194:10: error: no member named 'construct' in
    'sol::optional<T &>'
```

The patch rebinds to the argument instead, which is the documented behaviour of
`optional<T&>` ("assignment has rebind semantics rather than assign-through
semantics"), and static-asserts that the argument is an lvalue so a reference
can never bind to a temporary.

**When to delete it:** if the sol2 pin is ever moved forward, drop this file,
clear the dependency checkout, and let the pin speak for itself. Do not keep a
patch for a pin it does not apply to — `git apply` would fail loudly, which is
the intended behaviour.
