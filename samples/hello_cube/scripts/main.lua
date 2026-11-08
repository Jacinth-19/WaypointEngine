-- hello_cube: the smallest interesting Waypoint project.
--
-- It shows the two hooks every script can use and how to read input. The cube
-- itself is dropped by the physics engine; nothing here has to animate it.

local elapsed = 0.0

function on_start()
    waypoint.log("Hello Cube: physics will drop the cube onto the ground.")
    waypoint.log("Hold the right mouse button to look around, WASD to fly.")
end

function on_update(dt)
    elapsed = elapsed + dt

    -- Proof that input reaches scripts: log a line each whole second.
    if math.floor(elapsed) ~= math.floor(elapsed - dt) then
        waypoint.log(string.format("t = %d s", math.floor(elapsed)))
    end

    if waypoint.input.is_key_pressed("space") then
        waypoint.log("Space pressed - scripts can see key presses, too.")
    end
end
