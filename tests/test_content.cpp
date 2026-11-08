// Tests for the content that ships with the engine: templates and samples.
// SPDX-License-Identifier: Apache-2.0
//
// These files are the first thing a beginner opens, so a typo in one of them is
// worse than a typo in the engine. Load every project and scene for real.
#include <doctest/doctest.h>

#include <algorithm>
#include <filesystem>
#include <string>
#include <vector>

#include "core/FileSystem.hpp"
#include "project/Project.hpp"
#include "scene/Scene.hpp"

#ifndef WAYPOINT_SOURCE_DIR
#error "WAYPOINT_SOURCE_DIR must point at the repository root"
#endif

using namespace waypoint;

namespace {

std::vector<std::filesystem::path> findProjectFiles(const std::filesystem::path& root) {
    std::vector<std::filesystem::path> found;
    std::error_code code;
    if (!std::filesystem::is_directory(root, code)) {
        return found;
    }
    for (const auto& entry : std::filesystem::recursive_directory_iterator(root, code)) {
        if (entry.is_regular_file() && entry.path().filename() == kProjectFileName) {
            found.push_back(entry.path());
        }
    }
    std::sort(found.begin(), found.end());
    return found;
}

}  // namespace

TEST_CASE("Content: every shipped project manifest and scene loads") {
    const std::filesystem::path root{WAYPOINT_SOURCE_DIR};
    std::vector<std::filesystem::path> manifests = findProjectFiles(root / "samples");
    const auto templateManifests = findProjectFiles(root / "templates");
    manifests.insert(manifests.end(), templateManifests.begin(), templateManifests.end());

    REQUIRE_MESSAGE(!manifests.empty(),
                    "no waypoint.project.json found under samples/ or templates/");

    for (const auto& manifest : manifests) {
        std::string error;
        const auto project = loadProjectManifest(manifest, &error);
        REQUIRE_MESSAGE(project.has_value(),
                        manifest.generic_string() << ": " << error);
        CHECK_FALSE(project->name.empty());
        CHECK_FALSE(project->startScene.empty());

        // Templates are copied and renamed at creation time, so the start scene
        // must exist relative to the project even before that happens.
        CHECK_MESSAGE(FileSystem::isRegularFile(project->startScenePath()),
                      manifest.generic_string() << " points at a missing scene");

        const auto scene = Scene::loadFromFile(project->startScenePath(), &error);
        REQUIRE_MESSAGE(scene.has_value(),
                        project->startScenePath().generic_string() << ": " << error);
        CHECK(scene->world().entityCount() > 0);

        // Every shipped scene needs something to look through and a light.
        bool hasCamera = false;
        bool hasLight = false;
        for (const EntityHandle handle : scene->world().entities()) {
            hasCamera = hasCamera || scene->world().has<CameraComponent>(handle);
            hasLight = hasLight || scene->world().has<DirectionalLightComponent>(handle);
        }
        CHECK_MESSAGE(hasCamera, project->name << " has no camera");
        CHECK_MESSAGE(hasLight, project->name << " has no light");

        if (!project->scriptEntry.empty() &&
            FileSystem::isRegularFile(project->scriptEntryPath())) {
            // A shipped script must at least be readable text.
            CHECK(FileSystem::readTextFile(project->scriptEntryPath()).has_value());
        }
    }
}

TEST_CASE("Content: the blank template uses the placeholder the CLI rewrites") {
    const std::filesystem::path templateDir{WAYPOINT_SOURCE_DIR};
    const auto manifest = templateDir / "templates" / "blank_project" / "waypoint.project.json";
    REQUIRE(FileSystem::isRegularFile(manifest));

    const auto text = FileSystem::readTextFile(manifest);
    REQUIRE(text.has_value());
    CHECK(text->find("__PROJECT_NAME__") != std::string::npos);

    // Copying the template into a fresh folder must produce a loadable project.
    const std::filesystem::path destination =
        std::filesystem::temp_directory_path() / "waypoint_template_copy";
    std::error_code code;
    std::filesystem::remove_all(destination, code);
    std::filesystem::create_directories(destination.parent_path(), code);

    for (const auto& entry :
         std::filesystem::recursive_directory_iterator(templateDir / "templates" / "blank_project")) {
        if (!entry.is_regular_file()) {
            continue;
        }
        const auto relative = std::filesystem::relative(entry.path(),
                                                       templateDir / "templates" / "blank_project",
                                                       code);
        const auto target = destination / relative;
        std::filesystem::create_directories(target.parent_path(), code);
        std::filesystem::copy_file(entry.path(), target,
                                   std::filesystem::copy_options::overwrite_existing, code);
    }

    // Rewrite the placeholder exactly like `wp.py new` does.
    for (const auto& entry : std::filesystem::recursive_directory_iterator(destination)) {
        if (!entry.is_regular_file()) {
            continue;
        }
        auto contents = FileSystem::readTextFile(entry.path());
        if (!contents.has_value() || contents->find("__PROJECT_NAME__") == std::string::npos) {
            continue;
        }
        std::string replaced = *contents;
        std::size_t position = 0;
        while ((position = replaced.find("__PROJECT_NAME__", position)) != std::string::npos) {
            replaced.replace(position, std::string("__PROJECT_NAME__").size(), "TestProject");
            position += 11;
        }
        REQUIRE(FileSystem::writeTextFile(entry.path(), replaced));
    }

    std::string error;
    const auto project = loadProject(destination, &error);
    REQUIRE_MESSAGE(project.has_value(), error);
    CHECK(project->name == "TestProject");

    const auto scene = Scene::loadFromFile(project->startScenePath(), &error);
    REQUIRE_MESSAGE(scene.has_value(), error);

    std::filesystem::remove_all(destination, code);
}
