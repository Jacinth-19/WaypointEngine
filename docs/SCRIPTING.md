# Scripting

Gameplay is written in **Lua 5.4**, embedded with sol2. Scripts live in the
project (`scripts/`), so behaviour changes without recompiling anything.

## Hooks

```lua
function on_start()
    -- called once, after the scene has loaded
end

function on_update(dt)
    -- called every frame; dt is seconds since the last frame
end
```

Both are optional. A script with no hooks loads fine and does nothing.

Errors are caught: a syntax error, a runtime error, or an infinite loop that
trips Lua's error handler is reported in the console as
`error in on_start(): ...` and the game keeps running.

## The `waypoint` API

| Call | Result |
| --- | --- |
| `waypoint.log(...)` / `.warn(...)` / `.error(...)` | write to the engine log; values are tab separated |
| `waypoint.input.is_key_down(name)` | `true` while the key is held |
| `waypoint.input.is_key_pressed(name)` | `true` on the frame the key goes down |
| `waypoint.input.is_mouse_down(button)` | `true` while a mouse button is held |
| `waypoint.input.mouse_delta()` | two numbers: `dx, dy` since the last frame |

### Key names

Names are case-insensitive and ignore `_`, `-` and spaces, so all of these are
the same key:

```
"space"   "SPACE"   "Space"   "s_p-a c e"
```

Aliases exist for the common ones: `return`/`enter`, `esc`/`escape`,
`shift`, `ctrl`/`control`, `alt`, `cmd`/`super`/`meta`, `up`/`arrowup`,
`pageup`/`pgup`, `pgdown`, `del`, `dot`, `backtick`.

Digits can be written `"1"`, `"num1"` or `"digit1"`. Function keys are `"f1"` to
`"f12"`.

An unknown name is **not** an error: it simply reports "not pressed", which keeps
typos from crashing a game.

Mouse buttons: `"left"`, `"right"`, `"middle"`, `"x1"`, `"x2"`.

## Example: a spawner with a cooldown

```lua
local cooldown = 0.0

function on_start()
    waypoint.log("spawner ready")
end

function on_update(dt)
    cooldown = cooldown - dt
    if waypoint.input.is_key_pressed("space") and cooldown <= 0.0 then
        waypoint.log("spawn requested at", os.time())
        cooldown = 1.0
    end
end
```

## Example: reading mouse look

```lua
function on_update(dt)
    local dx, dy = waypoint.input.mouse_delta()
    if dx ~= 0 or dy ~= 0 then
        waypoint.log(string.format("mouse moved %.1f, %.1f", dx, dy))
    end
end
```

## Which script runs?

`waypoint.project.json` names it:

```json
"scriptEntry": "scripts/main.lua"
```

The path is relative to the project folder. If the file is missing, the player
logs a warning and runs the scene without gameplay logic.

## What scripts cannot do (yet)

There is no per-entity script binding in the Lua API yet: `ScriptComponent` is
recorded and loaded, but the runtime currently executes the single project entry
script. Entity-level handles, transform access and prefab spawning are the next
milestone — see `docs/ROADMAP.md`.

## Where the boundary is

`engine/script/LuaScriptEngine.hpp` deliberately hides sol2 behind a pimpl. The
rest of the engine never includes a Lua or sol2 header, and a build with
`WAYPOINT_ENABLE_SCRIPTING=OFF` simply has no `LuaScriptEngine` symbol.
