-- hello_cube: per-entity scripting.
--
-- Attached to "Falling Cube" through the scene's `script` component, so this file
-- runs once for that entity and receives its handle. Two entities can share the
-- file and still tell themselves apart - each gets its own globals, so `falls`
-- below belongs to one cube, not to all of them.
--
--   function on_start(handle)         -- once, after the scene loads
--   function on_update(dt, handle)    -- every frame, dt in seconds
--
-- A script that declares fewer arguments keeps working: Lua discards the rest.

local falls = 0

function on_start(handle)
    waypoint.log("cube " .. tostring(handle) .. " is ready to fall")
end

function on_update(dt, handle)
    falls = falls + dt

    -- Announce the landing once, the first time the cube has been falling for a
    -- second. The flag lives in this entity's environment, so a second entity
    -- running the same file would announce its own landing separately.
    if falls > 1.0 and not announced then
        announced = true
        waypoint.log("cube " .. tostring(handle) .. " has been falling for a second")
    end
end
