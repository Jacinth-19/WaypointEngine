// Tests for the Jolt physics wrapper.
// SPDX-License-Identifier: Apache-2.0
#include "TestDoctest.hpp"

#include "core/Log.hpp"
#include "ecs/Components.hpp"
#include "physics/JoltPhysicsWorld.hpp"

using namespace waypoint;

namespace {

// A world with a static ground plane and one falling box.
struct PhysicsScene {
    World world;
    JoltPhysicsWorld physics;
    EntityHandle ground = kInvalidEntity;
    EntityHandle box = kInvalidEntity;

    explicit PhysicsScene(f32 boxHeight = 5.0f) {
        std::string error;
        REQUIRE_MESSAGE(physics.initialize({}, &error), error);

        ground = world.createEntity("Ground");
        Transform groundTransform;
        groundTransform.position = Vec3(0.0f, -0.5f, 0.0f);
        groundTransform.scale = Vec3(20.0f, 1.0f, 20.0f);
        world.transform(ground)->position = groundTransform.position;
        world.transform(ground)->scale = groundTransform.scale;
        REQUIRE(physics.addBoxBody(ground, groundTransform, Vec3(10.0f, 0.5f, 10.0f),
                                   RigidBodyComponent::Motion::Static, 1.0f, false));

        box = world.createEntity("Box");
        Transform boxTransform;
        boxTransform.position = Vec3(0.0f, boxHeight, 0.0f);
        world.transform(box)->position = boxTransform.position;
        REQUIRE(physics.addBoxBody(box, boxTransform, Vec3(0.5f, 0.5f, 0.5f),
                                   RigidBodyComponent::Motion::Dynamic, 1.0f, true));
    }
};

void simulate(JoltPhysicsWorld& physics, World& world, f32 seconds) {
    constexpr f32 frame = 1.0f / 60.0f;
    const int frames = static_cast<int>(seconds / frame);
    for (int i = 0; i < frames; ++i) {
        physics.step(frame);
        physics.syncTransforms(world);
    }
}

}  // namespace

TEST_CASE("Physics: initialize and shutdown are safe to repeat") {
    JoltPhysicsWorld physics;
    CHECK_FALSE(physics.initialized());
    CHECK(physics.step(0.016f) == 0);  // stepping before init is a no-op

    std::string error;
    REQUIRE_MESSAGE(physics.initialize({}, &error), error);
    CHECK(physics.initialized());
    CHECK(physics.initialize({}, &error));  // second call is harmless
    CHECK(physics.settings().fixedTimeStep == doctest::Approx(1.0f / 60.0f));

    physics.shutdown();
    CHECK_FALSE(physics.initialized());
    physics.shutdown();  // and so is a second shutdown
}

TEST_CASE("Physics: a dynamic box falls and lands on static ground") {
    PhysicsScene scene(5.0f);
    CHECK(scene.physics.bodyCount() == 2);
    CHECK(scene.world.transform(scene.box)->position.y == doctest::Approx(5.0f));

    simulate(scene.physics, scene.world, 1.5f);

    const f32 landedHeight = scene.world.transform(scene.box)->position.y;
    CHECK(landedHeight < 5.0f);              // it fell
    CHECK(landedHeight > 0.3f);              // but did not sink through the floor
    CHECK(landedHeight == doctest::Approx(0.5f).epsilon(0.1));  // resting on the ground
    // Static geometry is never written back.
    CHECK(scene.world.transform(scene.ground)->position.y == doctest::Approx(-0.5f));
}

TEST_CASE("Physics: static bodies do not move even with gravity enabled") {
    World world;
    JoltPhysicsWorld physics;
    std::string error;
    REQUIRE_MESSAGE(physics.initialize({}, &error), error);

    const EntityHandle wall = world.createEntity("Wall");
    Transform transform;
    transform.position = Vec3(0.0f, 3.0f, 0.0f);
    world.transform(wall)->position = transform.position;
    REQUIRE(physics.addBoxBody(wall, transform, Vec3(0.5f), RigidBodyComponent::Motion::Static,
                               1.0f, true));

    simulate(physics, world, 2.0f);
    CHECK(world.transform(wall)->position.y == doctest::Approx(3.0f));
}

TEST_CASE("Physics: step() honours the fixed timestep and the substep cap") {
    World world;
    JoltPhysicsWorld physics;
    JoltPhysicsWorld::Settings settings;
    settings.fixedTimeStep = 1.0f / 60.0f;
    settings.maxSubSteps = 4;
    std::string error;
    REQUIRE_MESSAGE(physics.initialize(settings, &error), error);

    const EntityHandle box = world.createEntity("Box");
    Transform transform;
    transform.position = Vec3(0.0f, 100.0f, 0.0f);
    world.transform(box)->position = transform.position;
    REQUIRE(physics.addBoxBody(box, transform, Vec3(0.5f)));

    CHECK(physics.step(0.0f) == 0);
    CHECK(physics.step(1.0f / 120.0f) == 0);  // half a step is not enough yet
    CHECK(physics.step(1.0f / 120.0f) == 1);  // two halves make one step

    // A one-second hitch is clamped to maxSubSteps so the game does not freeze.
    CHECK(physics.step(1.0f) == 4);

    // A negative or absurd delta is ignored rather than launching the world.
    CHECK(physics.step(-1.0f) == 0);
    CHECK(physics.step(10.0f) == 0);
}

TEST_CASE("Physics: bodies can be removed and cleared") {
    World world;
    JoltPhysicsWorld physics;
    std::string error;
    REQUIRE_MESSAGE(physics.initialize({}, &error), error);

    const EntityHandle first = world.createEntity("first");
    const EntityHandle second = world.createEntity("second");
    Transform transform;
    transform.position = Vec3(0.0f, 10.0f, 0.0f);
    REQUIRE(physics.addBoxBody(first, transform, Vec3(0.5f)));
    REQUIRE(physics.addBoxBody(second, transform, Vec3(0.5f)));
    CHECK(physics.bodyCount() == 2);
    CHECK(physics.hasBody(first));

    physics.removeBody(first);
    CHECK_FALSE(physics.hasBody(first));
    CHECK(physics.bodyCount() == 1);
    physics.removeBody(first);  // removing twice is fine
    CHECK(physics.bodyCount() == 1);

    physics.clearBodies();
    CHECK(physics.bodyCount() == 0);
}

TEST_CASE("Physics: re-adding an entity replaces its body instead of stacking") {
    World world;
    JoltPhysicsWorld physics;
    std::string error;
    REQUIRE_MESSAGE(physics.initialize({}, &error), error);

    const EntityHandle box = world.createEntity("Box");
    Transform transform;
    transform.position = Vec3(0.0f, 5.0f, 0.0f);
    REQUIRE(physics.addBoxBody(box, transform, Vec3(0.5f)));
    REQUIRE(physics.addBoxBody(box, transform, Vec3(1.0f)));
    CHECK(physics.bodyCount() == 1);
}

TEST_CASE("Physics: invalid input is refused with a message, not a crash") {
    JoltPhysicsWorld physics;
    std::string error;

    // Bad settings are rejected.
    JoltPhysicsWorld::Settings bad;
    bad.fixedTimeStep = 0.0f;
    CHECK_FALSE(physics.initialize(bad, &error));
    CHECK(error.find("fixedTimeStep") != std::string::npos);
    CHECK_FALSE(physics.initialized());

    // Without initialize(), nothing can be added.
    Transform transform;
    CHECK_FALSE(physics.addBoxBody(1, transform, Vec3(0.5f)));
}
