// Tests for the fly camera system.
// SPDX-License-Identifier: Apache-2.0
#include "TestDoctest.hpp"

#include <cmath>

#include "ecs/Components.hpp"
#include "platform/Input.hpp"
#include "scene/Systems.hpp"

using namespace waypoint;

TEST_CASE("FlyCamera: yaw and pitch build a normalised rotation") {
    const Quat identity = FlyCameraSystem::rotationFromYawPitch(0.0f, 0.0f);
    CHECK(identity.w == doctest::Approx(1.0f));

    // A camera looks down its local -Z axis. Turning left by 90 degrees points
    // the view towards world -X (because +X is screen-right when facing -Z).
    const Quat turned = FlyCameraSystem::rotationFromYawPitch(90.0f, 0.0f);
    const Vec3 forward = turned * Vec3(0.0f, 0.0f, -1.0f);
    CHECK(forward.x == doctest::Approx(-1.0f));
    CHECK(forward.y == doctest::Approx(0.0f).epsilon(0.001));
    CHECK(glm::length(turned) == doctest::Approx(1.0f));

    // Positive pitch looks up, and is clamped so the camera cannot flip over.
    const Quat level = FlyCameraSystem::rotationFromYawPitch(0.0f, 30.0f);
    CHECK((level * Vec3(0.0f, 0.0f, -1.0f)).y ==
          doctest::Approx(std::sin(glm::radians(30.0f))).epsilon(0.001));
    const Quat steep = FlyCameraSystem::rotationFromYawPitch(0.0f, 400.0f);
    CHECK((steep * Vec3(0.0f, 0.0f, -1.0f)).y ==
          doctest::Approx(std::sin(glm::radians(89.0f))).epsilon(0.001));
}

TEST_CASE("FlyCamera: movement direction is normalised so diagonals are not faster") {
    Input input;
    const Quat facing = FlyCameraSystem::rotationFromYawPitch(0.0f, 0.0f);

    CHECK(glm::length(FlyCameraSystem::movementDirection(input, facing)) == doctest::Approx(0.0f));

    input.setKeyDown(Key::W, true);
    const Vec3 forward = FlyCameraSystem::movementDirection(input, facing);
    CHECK(forward.z == doctest::Approx(-1.0f));  // facing -Z

    input.setKeyDown(Key::D, true);
    const Vec3 diagonal = FlyCameraSystem::movementDirection(input, facing);
    CHECK(glm::length(diagonal) == doctest::Approx(1.0f));
    CHECK(diagonal.x == doctest::Approx(0.7071f).epsilon(0.001));
    CHECK(diagonal.z == doctest::Approx(-0.7071f).epsilon(0.001));

    // W and S cancel out, leaving only the strafe.
    input.setKeyDown(Key::S, true);
    const Vec3 cancelled = FlyCameraSystem::movementDirection(input, facing);
    CHECK(cancelled.x == doctest::Approx(1.0f));
    CHECK(cancelled.z == doctest::Approx(0.0f).epsilon(0.001));
}

TEST_CASE("FlyCamera: update moves the entity and respects boost") {
    World world;
    const EntityHandle camera = world.createEntity("Camera");
    auto& fly = world.add<FlyCameraComponent>(camera);
    fly.moveSpeed = 6.0f;
    fly.boostMultiplier = 4.0f;

    Input input;
    input.setKeyDown(Key::W, true);
    CHECK(FlyCameraSystem::update(world, input, 0.5f, false) == 1);
    CHECK(world.transform(camera)->position.z == doctest::Approx(-3.0f));  // forward is -Z

    // Holding shift multiplies speed by boostMultiplier (6 * 4 * 0.5).
    input.setKeyDown(Key::LeftShift, true);
    FlyCameraSystem::update(world, input, 0.5f, false);
    CHECK(world.transform(camera)->position.z == doctest::Approx(-3.0f - 12.0f));

    // Space and ctrl move vertically.
    input.clearKeys();
    input.setKeyDown(Key::Space, true);
    FlyCameraSystem::update(world, input, 1.0f, false);
    CHECK(world.transform(camera)->position.y == doctest::Approx(6.0f));
}

TEST_CASE("FlyCamera: mouse look only applies when the mouse is captured") {
    World world;
    const EntityHandle camera = world.createEntity("Camera");
    auto& fly = world.add<FlyCameraComponent>(camera);
    fly.lookSensitivity = 0.5f;

    Input input;
    input.addMouseMotion(10.0f, 0.0f);

    // Free mouse: the view must not spin while the cursor moves for the UI.
    FlyCameraSystem::update(world, input, 0.016f, false);
    CHECK(fly.yaw == doctest::Approx(0.0f));

    // Captured mouse: yaw follows the horizontal motion.
    FlyCameraSystem::update(world, input, 0.016f, true);
    CHECK(fly.yaw == doctest::Approx(-5.0f));  // 10 px * 0.5 degrees, inverted
}

TEST_CASE("FlyCamera: only entities with a fly camera are touched") {
    World world;
    const EntityHandle plain = world.createEntity("Prop");
    world.transform(plain)->position = Vec3(1.0f, 2.0f, 3.0f);

    Input input;
    input.setKeyDown(Key::W, true);
    CHECK(FlyCameraSystem::update(world, input, 1.0f, true) == 0);
    CHECK(world.transform(plain)->position == Vec3(1.0f, 2.0f, 3.0f));
}
