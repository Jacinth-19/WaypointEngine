// Tests for scene JSON loading and saving.
// SPDX-License-Identifier: Apache-2.0
#include <doctest/doctest.h>

#include <nlohmann/json.hpp>

#include "TestSupport.hpp"
#include "core/FileSystem.hpp"
#include "scene/Scene.hpp"

using namespace waypoint;
using nlohmann::json;

namespace {

Scene makeScene() {
    Scene scene{"test level"};
    const EntityHandle camera = scene.createEntity("Camera");
    scene.world().add<CameraComponent>(camera);
    scene.world().add<FlyCameraComponent>(camera);
    scene.world().transform(camera)->position = Vec3(0.0f, 3.0f, 10.0f);

    const EntityHandle sun = scene.createEntity("Sun");
    DirectionalLightComponent light;
    light.intensity = 2.5f;
    light.color = Vec3(1.0f, 0.9f, 0.8f);
    scene.world().add<DirectionalLightComponent>(sun, light);

    const EntityHandle cube = scene.createEntity("Crate");
    MeshRendererComponent mesh;
    mesh.mesh = "builtin:cube";
    mesh.color = Vec3(0.1f, 0.2f, 0.3f);
    scene.world().add<MeshRendererComponent>(cube, mesh);
    RigidBodyComponent body;
    body.motion = RigidBodyComponent::Motion::Dynamic;
    body.halfExtents = Vec3(0.5f);
    body.mass = 2.0f;
    scene.world().add<RigidBodyComponent>(cube, body);
    ScriptComponent script{"scripts/crate.lua"};
    scene.world().add<ScriptComponent>(cube, script);
    scene.world().transform(cube)->position = Vec3(1.0f, 4.0f, 2.0f);
    return scene;
}

std::filesystem::path writeJson(const test::TempDir& temp, const std::string& text) {
    const auto path = temp / "scene.scene.json";
    std::string error;
    REQUIRE_MESSAGE(FileSystem::writeTextFile(path, text, &error), error);
    return path;
}

}  // namespace

TEST_CASE("Scene: save then load preserves everything the editor can edit") {
    const test::TempDir temp;
    const Scene original = makeScene();
    const auto path = temp / "main.scene.json";

    std::string error;
    REQUIRE_MESSAGE(original.saveToFile(path, &error), error);
    CHECK(error.empty());

    const auto loaded = Scene::loadFromFile(path, &error);
    REQUIRE_MESSAGE(loaded.has_value(), error);
    CHECK(loaded->name() == "test level");
    CHECK(loaded->world().entityCount() == 3);

    const auto camera = loaded->world().findByName("Camera");
    REQUIRE(camera.has_value());
    CHECK(loaded->world().has<CameraComponent>(*camera));
    CHECK(loaded->world().has<FlyCameraComponent>(*camera));
    CHECK(loaded->world().transform(*camera)->position.z == doctest::Approx(10.0f));

    const auto cube = loaded->world().findByName("Crate");
    REQUIRE(cube.has_value());
    CHECK(loaded->world().transform(*cube)->position.y == doctest::Approx(4.0f));

    const auto* body = loaded->world().tryGet<RigidBodyComponent>(*cube);
    REQUIRE(body != nullptr);
    CHECK(body->motion == RigidBodyComponent::Motion::Dynamic);
    CHECK(body->mass == doctest::Approx(2.0f));

    const auto* script = loaded->world().tryGet<ScriptComponent>(*cube);
    REQUIRE(script != nullptr);
    CHECK(script->path == "scripts/crate.lua");

    // Entity ids survive the round-trip, so references keep working.
    const Uuid originalId = original.world().tryGet<UuidComponent>(
        *original.world().findByName("Crate"))->id;
    CHECK(loaded->world().tryGet<UuidComponent>(*cube)->id == originalId);
}

TEST_CASE("Scene: JSON shape matches the documented format") {
    const json document = makeScene().toJson();
    REQUIRE(document.is_object());
    CHECK(document["name"] == "test level");
    CHECK(document["engineVersion"] == "0.1.0");
    REQUIRE(document["entities"].is_array());
    REQUIRE(document["entities"].size() == 3);

    const json& crate = document["entities"][2];
    CHECK(crate["name"] == "Crate");
    CHECK(crate["id"].is_string());
    REQUIRE(crate["transform"]["position"].size() == 3);
    REQUIRE(crate["transform"]["rotation"].size() == 4);
    REQUIRE(crate["transform"]["scale"].size() == 3);
    CHECK(crate["transform"]["rotation"][0] == doctest::Approx(1.0f));
    CHECK(crate["components"]["meshRenderer"]["mesh"] == "builtin:cube");
    CHECK(crate["components"]["rigidBody"]["motion"] == "dynamic");
    CHECK(crate["components"]["script"]["path"] == "scripts/crate.lua");
}

TEST_CASE("Scene: minimal documents load with sensible defaults") {
    const json document = json::parse(R"({
        "name": "tiny",
        "entities": [
            {"name": "Only Entity"}
        ]
    })");

    std::string error;
    const auto scene = Scene::fromJson(document, &error);
    REQUIRE_MESSAGE(scene.has_value(), error);
    CHECK(scene->name() == "tiny");
    REQUIRE(scene->world().entityCount() == 1);

    const EntityHandle handle = scene->world().entities()[0];
    CHECK(*scene->world().name(handle) == "Only Entity");
    CHECK(scene->world().transform(handle)->scale == Vec3(1.0f));
    // A missing id still produces a usable, non-null uuid.
    CHECK_FALSE(scene->world().tryGet<UuidComponent>(handle)->id.isNull());
}

TEST_CASE("Scene: rotation is normalised and zero rotations fall back to identity") {
    const json document = json::parse(R"({
        "name": "rotations",
        "entities": [
            {"name": "unnormalised", "transform": {"rotation": [2, 0, 0, 0]}},
            {"name": "zero", "transform": {"rotation": [0, 0, 0, 0]}}
        ]
    })");

    std::string error;
    const auto scene = Scene::fromJson(document, &error);
    REQUIRE_MESSAGE(scene.has_value(), error);

    const auto first = scene->world().findByName("unnormalised");
    REQUIRE(first.has_value());
    const Quat rotation = scene->world().transform(*first)->rotation;
    CHECK(glm::length(rotation) == doctest::Approx(1.0f));

    const auto second = scene->world().findByName("zero");
    REQUIRE(second.has_value());
    CHECK(scene->world().transform(*second)->rotation.w == doctest::Approx(1.0f));
}

TEST_CASE("Scene: broken files are reported, never guessed at") {
    const test::TempDir temp;
    std::string error;

    SUBCASE("not JSON at all") {
        const auto path = writeJson(temp, "this is not json");
        CHECK_FALSE(Scene::loadFromFile(path, &error).has_value());
        CHECK(error.find("invalid JSON") != std::string::npos);
    }

    SUBCASE("entities must be an array") {
        const auto path = writeJson(temp, R"({"name": "x", "entities": 5})");
        CHECK_FALSE(Scene::loadFromFile(path, &error).has_value());
        CHECK(error.find("entities") != std::string::npos);
    }

    SUBCASE("a position must have three numbers") {
        const auto path = writeJson(temp, R"({
            "name": "x",
            "entities": [{"name": "bad", "transform": {"position": [1, 2]}}]
        })");
        CHECK_FALSE(Scene::loadFromFile(path, &error).has_value());
        CHECK(error.find("position") != std::string::npos);
    }

    SUBCASE("a negative half extent is rejected") {
        const auto path = writeJson(temp, R"({
            "name": "x",
            "entities": [{"name": "bad", "components": {
                "rigidBody": {"halfExtents": [1, -1, 1]}
            }}]
        })");
        CHECK_FALSE(Scene::loadFromFile(path, &error).has_value());
        CHECK(error.find("halfExtents") != std::string::npos);
    }

    SUBCASE("an unknown motion mode is rejected") {
        const auto path = writeJson(temp, R"({
            "name": "x",
            "entities": [{"name": "bad", "components": {
                "rigidBody": {"motion": "floaty"}
            }}]
        })");
        CHECK_FALSE(Scene::loadFromFile(path, &error).has_value());
        CHECK(error.find("motion") != std::string::npos);
    }

    SUBCASE("missing files are reported") {
        CHECK_FALSE(Scene::loadFromFile(temp / "nope.scene.json", &error).has_value());
        CHECK(error.find("could not read") != std::string::npos);
    }
}

TEST_CASE("Scene: unknown components are ignored so old engines can read new files") {
    const json document = json::parse(R"({
        "name": "future",
        "entities": [
            {"name": "thing", "components": {"hovercraft": {"lift": 3}, "camera": {}}}
        ]
    })");

    std::string error;
    const auto scene = Scene::fromJson(document, &error);
    REQUIRE_MESSAGE(scene.has_value(), error);
    const EntityHandle handle = scene->world().entities()[0];
    CHECK(scene->world().has<CameraComponent>(handle));
}

TEST_CASE("Scene: duplicate ids are replaced instead of colliding") {
    const json document = json::parse(R"({
        "name": "dupes",
        "entities": [
            {"id": "3f2b1c4d-8a9e-4f10-b7c2-5d6e7f809a1b", "name": "a"},
            {"id": "3f2b1c4d-8a9e-4f10-b7c2-5d6e7f809a1b", "name": "b"}
        ]
    })");

    std::string error;
    const auto scene = Scene::fromJson(document, &error);
    REQUIRE_MESSAGE(scene.has_value(), error);

    const auto a = scene->world().findByName("a");
    const auto b = scene->world().findByName("b");
    REQUIRE(a.has_value());
    REQUIRE(b.has_value());
    CHECK(scene->world().tryGet<UuidComponent>(*a)->id !=
          scene->world().tryGet<UuidComponent>(*b)->id);
}

TEST_CASE("Scene: clear() keeps the name but drops entities") {
    Scene scene = makeScene();
    scene.clear();
    CHECK(scene.world().entityCount() == 0);
    CHECK(scene.name() == "test level");
}
