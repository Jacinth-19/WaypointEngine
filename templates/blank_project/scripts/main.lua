-- main.lua - the script the engine runs when your project starts.
--
-- Two functions are special:
--   on_start()        called once, after the scene has loaded
--   on_update(dt)     called every frame; dt is the seconds since last frame
--
-- Delete either one and nothing breaks; the engine simply has less to do.

local elapsed = 0.0

function on_start()
    waypoint.log("__PROJECT_NAME__ is running")
    waypoint.log("Controls: WASD to move, Space/Ctrl up and down, Shift to boost,")
    waypoint.log("          hold the right mouse button to look, Escape to quit.")
end

function on_update(dt)
    elapsed = elapsed + dt

    -- Something to read in the console while you get your bearings.
    if math.floor(elapsed) ~= math.floor(elapsed - dt) then
        waypoint.log(string.format("%d second(s) in __PROJECT_NAME__", math.floor(elapsed)))
    end
end
