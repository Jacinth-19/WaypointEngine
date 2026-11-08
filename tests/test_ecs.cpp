// Tests for the ECS world.
// SPDX-License-Identifier: Apache-2.0
#include <doctest/doctest.h>

#include <algorithm>
#include <stdexcept>

#include "ecs/World.hpp"

using namespace waypoint;

TEST_CASE("World: created entities are alive and fully equipped") {
    World world;
    CHECK(world.entityCount() == 0);

    const EntityHandle crate = world.createEntity("Crate");
    CHECK(crate != kInvalidEntity);
    CHECK(world.alive(crate));
    CHECK(world.entityCount() == 1);

    // Every entity always has a handle, a name, a transform and an id.
    CHECK(world.has<TransformComponent>(crate));
    CHECK(world.has<HandleComponent>(crate));
    CHECK(world.has<NameComponent>(crate));
    CHECK(world.has<UuidComponent>(crate));

    REQUIRE(world.name(crate) != nullptr);
    CHECK(*world.name(crate) == "Crate");

    REQUIRE(world.transform(crate) != nullptr);
    CHECK(world.transform(crate)->position == Vec3(0.0f));
    CHECK(world.transform(crate)->scale == Vec3(1.0f));
    CHECK(world.transform(crate)->rotation.w == doctest::Approx(1.0f));
}

TEST_CASE("World: handles stay valid until destroyed, and are never reused") {
    World world;
    const EntityHandle first = world.createEntity("first");
    CHECK(world.destroyEntity(first));
    CHECK_FALSE(world.alive(first));
    CHECK_FALSE(world.destroyEntity(first));  // double destroy is harmless

    const EntityHandle second = world.createEntity("second");
    CHECK(second != first);
    CHECK(world.alive(second));
    CHECK(world.entityCount() == 1);

    // Dead handles must not resolve to the recycled storage slot.
    CHECK(world.transform(first) == nullptr);
    CHECK(world.tryGet<NameComponent>(first) == nullptr);
    CHECK_FALSE(world.has<TransformComponent>(first));

    // Destroying an id that was never handed out is a no-op.
    CHECK_FALSE(world.destroyEntity(9999));
}

TEST_CASE("World: components can be added, read and removed") {
    World world;
    const EntityHandle box = world.createEntity("Box");

    auto& mesh = world.add<MeshRendererComponent>(box);
    mesh.color = Vec3(1.0f, 0.0f, 0.0f);
    CHECK(world.has<MeshRendererComponent>(box));

    const auto* readBack = world.tryGet<MeshRendererComponent>(box);
    REQUIRE(readBack != nullptr);
    CHECK(readBack->color.x == doctest::Approx(1.0f));

    // Adding twice replaces rather than duplicating.
    world.add<MeshRendererComponent>(box, MeshRendererComponent{});
    CHECK(world.tryGet<MeshRendererComponent>(box)->color.y == doctest::Approx(0.55f));

    world.remove<MeshRendererComponent>(box);
    CHECK_FALSE(world.has<MeshRendererComponent>(box));
    CHECK(world.tryGet<MeshRendererComponent>(box) == nullptr);

    // Removing something that is not there is safe.
    world.remove<MeshRendererComponent>(box);
    CHECK_THROWS_AS(world.add<CameraComponent>(kInvalidEntity), std::invalid_argument);
}

TEST_CASE("World: lookup by name and by uuid") {
    World world;
    const EntityHandle camera = world.createEntity("Main Camera");
    const Uuid id = Uuid::generate();
    const EntityHandle light = world.createEntity(id, "Sun");

    const auto byName = world.findByName("Main Camera");
    REQUIRE(byName.has_value());
    CHECK(*byName == camera);
    CHECK_FALSE(world.findByName("Nothing").has_value());

    const auto byUuid = world.findByUuid(id);
    REQUIRE(byUuid.has_value());
    CHECK(*byUuid == light);
    CHECK_FALSE(world.findByUuid(Uuid::generate()).has_value());
}

TEST_CASE("World: entity listing is stable and clear() resets everything") {
    World world;
    std::vector<EntityHandle> created;
    for (int i = 0; i < 5; ++i) {
        created.push_back(world.createEntity("e" + std::to_string(i)));
    }

    const std::vector<EntityHandle> listed = world.entities();
    REQUIRE(listed.size() == created.size());
    CHECK(std::is_sorted(listed.begin(), listed.end()));

    world.clear();
    CHECK(world.entityCount() == 0);
    for (const EntityHandle handle : created) {
        CHECK_FALSE(world.alive(handle));
    }

    // A cleared world starts handing out handles from the beginning again,
    // which is fine because nothing can still be holding an old one.
    CHECK(world.createEntity("fresh") == 1);
}
