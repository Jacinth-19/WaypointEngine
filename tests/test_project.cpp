// Tests for project manifests.
// SPDX-License-Identifier: Apache-2.0
#include <doctest/doctest.h>

#include <nlohmann/json.hpp>

#include "TestSupport.hpp"
#include "core/FileSystem.hpp"
#include "project/Project.hpp"
#include "scene/Scene.hpp"

using namespace waypoint;

TEST_CASE("Project: createProject writes a runnable project") {
    const test::TempDir temp;
    const auto directory = temp / "MyGame";

    std::string error;
    const auto project = createProject(directory, "MyGame", &error);
    REQUIRE_MESSAGE(project.has_value(), error);

    CHECK(project->name == "MyGame");
    CHECK(project->engineVersion == std::string(kEngineVersion));
    CHECK(project->startScene == "scenes/main.scene.json");
    CHECK(project->scriptEntry == "scripts/main.lua");

    CHECK(FileSystem::isRegularFile(directory / "waypoint.project.json"));
    CHECK(FileSystem::isRegularFile(directory / "scenes/main.scene.json"));
    CHECK(FileSystem::isRegularFile(directory / "scripts/main.lua"));

    // The starter scene must actually load.
    const auto scene = Scene::loadFromFile(project->startScenePath(), &error);
    REQUIRE_MESSAGE(scene.has_value(), error);
    CHECK(scene->world().entityCount() >= 3);
    CHECK(scene->world().findByName("Camera").has_value());
    CHECK(scene->world().findByName("Sun").has_value());
    CHECK(scene->world().findByName("Cube").has_value());

    // The generated manifest has exactly the documented fields.
    const auto text = FileSystem::readTextFile(directory / "waypoint.project.json");
    REQUIRE(text.has_value());
    const auto manifest = nlohmann::json::parse(*text);
    CHECK(manifest["name"] == "MyGame");
    CHECK(manifest["engineVersion"] == "0.1.0");
    CHECK(manifest["startScene"] == "scenes/main.scene.json");
    CHECK(manifest["scriptEntry"] == "scripts/main.lua");
}

TEST_CASE("Project: an existing project is never overwritten") {
    const test::TempDir temp;
    std::string error;
    REQUIRE(createProject(temp / "Game", "Game", &error).has_value());

    error.clear();
    CHECK_FALSE(createProject(temp / "Game", "Game", &error).has_value());
    CHECK(error.find("already exists") != std::string::npos);
}

TEST_CASE("Project: load resolves paths under the project folder") {
    const test::TempDir temp;
    std::string error;
    REQUIRE(createProject(temp / "Game", "Game", &error).has_value());

    const auto project = loadProject(temp / "Game", &error);
    REQUIRE_MESSAGE(project.has_value(), error);

    CHECK(project->rootDirectory.filename() == "Game");
    CHECK(project->manifestPath().filename() == "waypoint.project.json");
    CHECK(project->startScenePath().filename() == "main.scene.json");
    CHECK(project->startScenePath().parent_path().filename() == "scenes");
    CHECK(project->scriptEntryPath().parent_path().filename() == "scripts");

    // Absolute paths are honoured as-is (built portably: a Windows absolute
    // path needs a drive letter, so "/tmp/x" would not be absolute there).
    const std::filesystem::path absoluteScript =
        std::filesystem::absolute(std::filesystem::temp_directory_path() / "elsewhere.lua");
    CHECK(project->resolve(absoluteScript.generic_string()) == absoluteScript);
}

TEST_CASE("Project: saveProject round-trips edited fields") {
    const test::TempDir temp;
    std::string error;
    auto project = createProject(temp / "Game", "Game", &error);
    REQUIRE_MESSAGE(project.has_value(), error);

    project->name = "Renamed";
    project->startScene = "scenes/other.scene.json";
    REQUIRE_MESSAGE(saveProject(*project, &error), error);

    const auto reloaded = loadProject(temp / "Game", &error);
    REQUIRE_MESSAGE(reloaded.has_value(), error);
    CHECK(reloaded->name == "Renamed");
    CHECK(reloaded->startScene == "scenes/other.scene.json");
}

TEST_CASE("Project: broken manifests are rejected with a readable reason") {
    const test::TempDir temp;
    std::string error;

    SUBCASE("missing file") {
        CHECK_FALSE(loadProject(temp / "Nowhere", &error).has_value());
        CHECK(error.find("waypoint.project.json") != std::string::npos);
    }

    SUBCASE("invalid JSON") {
        const auto directory = temp / "Broken";
        REQUIRE(FileSystem::ensureDirectory(directory));
        REQUIRE(FileSystem::writeTextFile(directory / "waypoint.project.json", "{ nope"));
        CHECK_FALSE(loadProject(directory, &error).has_value());
        CHECK(error.find("invalid JSON") != std::string::npos);
    }

    SUBCASE("empty name") {
        const auto directory = temp / "Nameless";
        REQUIRE(FileSystem::ensureDirectory(directory));
        REQUIRE(FileSystem::writeTextFile(directory / "waypoint.project.json",
                                          R"({"name": "", "startScene": "a.json"})"));
        CHECK_FALSE(loadProject(directory, &error).has_value());
        CHECK(error.find("name") != std::string::npos);
    }

    SUBCASE("wrong field type") {
        const auto directory = temp / "WrongType";
        REQUIRE(FileSystem::ensureDirectory(directory));
        REQUIRE(FileSystem::writeTextFile(directory / "waypoint.project.json",
                                          R"({"name": 12, "startScene": "a.json"})"));
        CHECK_FALSE(loadProject(directory, &error).has_value());
        CHECK(error.find("must be a string") != std::string::npos);
    }
}

TEST_CASE("Project: validate catches unusable field values") {
    Project project;
    project.name = "ok";
    project.startScene = "scenes/main.scene.json";
    project.rootDirectory = "/tmp/game";
    CHECK(project.validate());

    std::string error;
    project.name.clear();
    CHECK_FALSE(project.validate(&error));
    CHECK(error.find("name") != std::string::npos);

    project.name = "ok";
    project.rootDirectory.clear();
    CHECK_FALSE(project.validate(&error));
    CHECK(error.find("folder") != std::string::npos);
}
