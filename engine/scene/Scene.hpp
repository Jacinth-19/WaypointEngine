// Waypoint Engine — scenes.
// SPDX-License-Identifier: Apache-2.0
//
// A *scene* is one level or menu: a list of entities saved as JSON. Scenes are
// plain data, so a game can ship new levels without recompiling the engine.
//
//   auto scene = Scene::loadFromFile("game/scenes/main.scene.json", &error);
//   scene->world().add<MeshRendererComponent>(handle);
#pragma once

#include <filesystem>
#include <optional>
#include <string>
#include <string_view>

#include <nlohmann/json.hpp>

#include "ecs/World.hpp"

namespace waypoint {

class Scene {
public:
    Scene() = default;
    explicit Scene(std::string name);

    // Reads `*.scene.json` from disk. Returns std::nullopt and fills `error` on
    // any problem (missing file, invalid JSON, bad component data).
    [[nodiscard]] static std::optional<Scene> loadFromFile(
        const std::filesystem::path& path, std::string* error = nullptr);

    [[nodiscard]] static std::optional<Scene> fromJson(const nlohmann::json& json,
                                                      std::string* error = nullptr);

    // Writes the scene, creating parent directories as needed.
    [[nodiscard]] bool saveToFile(const std::filesystem::path& path,
                                  std::string* error = nullptr) const;

    [[nodiscard]] nlohmann::json toJson() const;

    [[nodiscard]] const std::string& name() const { return m_name; }
    void setName(std::string name) { m_name = std::move(name); }

    [[nodiscard]] World& world() { return m_world; }
    [[nodiscard]] const World& world() const { return m_world; }

    [[nodiscard]] const std::filesystem::path& sourcePath() const { return m_sourcePath; }
    void setSourcePath(std::filesystem::path path) { m_sourcePath = std::move(path); }

    // Folder the scene was loaded from; used to resolve relative asset paths.
    [[nodiscard]] std::filesystem::path sourceDirectory() const;

    // Adds an entity with a transform and a name. The UUID is generated.
    EntityHandle createEntity(std::string_view name);

    // Destroys every entity but keeps the scene name.
    void clear();

private:
    std::string m_name{"Untitled"};
    World m_world;
    std::filesystem::path m_sourcePath;
};

}  // namespace waypoint
