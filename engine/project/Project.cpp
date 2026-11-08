// Waypoint Engine — project manifest implementation.
// SPDX-License-Identifier: Apache-2.0
#include "project/Project.hpp"

#include <nlohmann/json.hpp>

#include "core/FileSystem.hpp"
#include "core/Log.hpp"
#include "ecs/Components.hpp"
#include "scene/Scene.hpp"

namespace waypoint {
namespace {

using json = nlohmann::json;

void setError(std::string* error, std::string message) {
    if (error != nullptr) {
        *error = std::move(message);
    }
}

bool readString(const json& object, const char* key, std::string& out, std::string* error) {
    const auto it = object.find(key);
    if (it == object.end()) {
        return true;
    }
    if (!it->is_string()) {
        setError(error, std::string("project field '") + key + "' must be a string");
        return false;
    }
    out = it->get<std::string>();
    return true;
}

constexpr char kStarterScript[] = R"(--- Starter script for a Waypoint project.
-- These two functions are called by the engine automatically.
-- Delete them and nothing runs; keep them and you have a game loop.

local time = 0.0

function on_start()
    waypoint.log("Hello from game/scripts/main.lua")
end

function on_update(dt)
    time = time + dt

    -- Hold Space to lift the cube, release to let it fall again.
    -- (Requires a rigidBody component with "useGravity" enabled.)
    if waypoint.input.is_key_down("space") then
        waypoint.log(string.format("wheee: %.2f s", time))
    end
end
)";

}  // namespace

std::filesystem::path Project::manifestPath() const {
    return rootDirectory / std::string(kProjectFileName);
}

std::filesystem::path Project::resolve(std::string_view relativePath) const {
    return FileSystem::joinPath(rootDirectory, relativePath);
}

std::filesystem::path Project::startScenePath() const {
    return resolve(startScene);
}

std::filesystem::path Project::scriptEntryPath() const {
    return resolve(scriptEntry);
}

bool Project::validate(std::string* error) const {
    if (name.empty()) {
        setError(error, "project 'name' cannot be empty");
        return false;
    }
    if (startScene.empty()) {
        setError(error, "project 'startScene' cannot be empty");
        return false;
    }
    if (rootDirectory.empty()) {
        setError(error, "project folder is not set");
        return false;
    }
    return true;
}

std::optional<Project> loadProjectManifest(const std::filesystem::path& manifestFile,
                                           std::string* error) {
    std::filesystem::path file = manifestFile;
    if (!FileSystem::isRegularFile(file)) {
        if (FileSystem::isDirectory(file)) {
            file = file / std::string(kProjectFileName);
        }
        if (!FileSystem::isRegularFile(file)) {
            setError(error, "no " + std::string(kProjectFileName) + " found at " +
                                manifestFile.generic_string());
            return std::nullopt;
        }
    }

    const auto text = FileSystem::readTextFile(file);
    if (!text.has_value()) {
        setError(error, "could not read " + file.generic_string());
        return std::nullopt;
    }

    json parsed;
    try {
        parsed = json::parse(*text);
    } catch (const json::exception& exception) {
        setError(error, "invalid JSON in " + file.generic_string() + ": " + exception.what());
        return std::nullopt;
    }
    if (!parsed.is_object()) {
        setError(error, "project manifest must contain a JSON object");
        return std::nullopt;
    }

    Project project;
    if (!readString(parsed, "name", project.name, error) ||
        !readString(parsed, "engineVersion", project.engineVersion, error) ||
        !readString(parsed, "startScene", project.startScene, error) ||
        !readString(parsed, "scriptEntry", project.scriptEntry, error)) {
        return std::nullopt;
    }

    project.rootDirectory = FileSystem::parentDirectory(FileSystem::absolute(file));
    if (!project.validate(error)) {
        return std::nullopt;
    }

    if (project.engineVersion != kEngineVersion) {
        Log::warn("Project '{}' was made with engine {} but this engine is {}",
                  project.name, project.engineVersion, kEngineVersion);
    }
    if (!FileSystem::isRegularFile(project.startScenePath())) {
        Log::warn("Project '{}' start scene is missing: {}", project.name,
                  project.startScenePath().generic_string());
    }
    return project;
}

std::optional<Project> loadProject(const std::filesystem::path& directory, std::string* error) {
    return loadProjectManifest(FileSystem::joinPath(directory, kProjectFileName), error);
}

bool saveProject(const Project& project, std::string* error) {
    if (!project.validate(error)) {
        return false;
    }

    json manifest = json::object();
    manifest["name"] = project.name;
    manifest["engineVersion"] = project.engineVersion.empty() ? std::string(kEngineVersion)
                                                             : project.engineVersion;
    manifest["startScene"] = project.startScene;
    manifest["scriptEntry"] = project.scriptEntry;

    return FileSystem::writeTextFile(project.manifestPath(), manifest.dump(2) + "\n", error);
}

std::optional<Project> createProject(const std::filesystem::path& directory, std::string_view name,
                                    std::string* error) {
    if (name.empty()) {
        setError(error, "project name cannot be empty");
        return std::nullopt;
    }

    Project project;
    project.name = std::string(name);
    project.engineVersion = std::string(kEngineVersion);
    project.startScene = "scenes/main.scene.json";
    project.scriptEntry = "scripts/main.lua";
    project.rootDirectory = FileSystem::absolute(directory);

    if (!FileSystem::ensureDirectory(project.rootDirectory)) {
        setError(error, "could not create project folder: " +
                            project.rootDirectory.generic_string());
        return std::nullopt;
    }
    if (FileSystem::isRegularFile(project.manifestPath())) {
        setError(error, "a project already exists at " + project.rootDirectory.generic_string());
        return std::nullopt;
    }

    // Scene: a camera, a sun, and one mesh waiting to fall.
    Scene scene{project.name};
    const EntityHandle camera = scene.createEntity("Camera");
    scene.world().add<CameraComponent>(camera);
    scene.world().add<FlyCameraComponent>(camera);
    if (auto* transform = scene.world().transform(camera)) {
        transform->position = Vec3(0.0f, 3.0f, 10.0f);
    }

    const EntityHandle sun = scene.createEntity("Sun");
    scene.world().add<DirectionalLightComponent>(sun);
    if (auto* transform = scene.world().transform(sun)) {
        transform->rotation = glm::normalize(Quat(glm::vec3(glm::radians(-40.0f),
                                                           glm::radians(30.0f), 0.0f)));
    }

    const EntityHandle cube = scene.createEntity("Cube");
    scene.world().add<MeshRendererComponent>(cube);
    scene.world().add<RigidBodyComponent>(cube);
    if (auto* transform = scene.world().transform(cube)) {
        transform->position = Vec3(0.0f, 4.0f, 0.0f);
    }

    if (!scene.saveToFile(project.startScenePath(), error)) {
        return std::nullopt;
    }
    if (!FileSystem::writeTextFile(project.scriptEntryPath(), kStarterScript, error)) {
        return std::nullopt;
    }
    if (!saveProject(project, error)) {
        return std::nullopt;
    }

    Log::info("Created project '{}' at {}", project.name, project.rootDirectory.generic_string());
    return project;
}

}  // namespace waypoint
