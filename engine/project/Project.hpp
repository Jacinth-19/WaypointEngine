// Waypoint Engine — project manifests.
// SPDX-License-Identifier: Apache-2.0
//
// A *project* is a folder with a `waypoint.project.json` file. It records the
// project name, which scene to start, and which Lua script boots the game.
// Game content always lives in a project; nothing about a specific game is
// compiled into the engine.
#pragma once

#include <filesystem>
#include <optional>
#include <string>
#include <string_view>

namespace waypoint {

inline constexpr std::string_view kEngineVersion = "0.1.0";
inline constexpr std::string_view kProjectFileName = "waypoint.project.json";

struct Project {
    std::string name{"Untitled"};
    std::string engineVersion{std::string(kEngineVersion)};
    std::string startScene{"scenes/main.scene.json"};
    std::string scriptEntry{"scripts/main.lua"};
    std::filesystem::path rootDirectory;

    [[nodiscard]] std::filesystem::path manifestPath() const;
    [[nodiscard]] std::filesystem::path resolve(std::string_view relativePath) const;
    [[nodiscard]] std::filesystem::path startScenePath() const;
    [[nodiscard]] std::filesystem::path scriptEntryPath() const;

    // Checks the fields that the engine needs. Returns false and fills `error`.
    [[nodiscard]] bool validate(std::string* error = nullptr) const;
};

// Loads `<directory>/waypoint.project.json`. Returns std::nullopt on error.
[[nodiscard]] std::optional<Project> loadProject(const std::filesystem::path& directory,
                                                 std::string* error = nullptr);

// Loads the manifest at an explicit path (the path may be the file itself).
[[nodiscard]] std::optional<Project> loadProjectManifest(
    const std::filesystem::path& manifestFile, std::string* error = nullptr);

// Writes `waypoint.project.json` for an existing project folder.
[[nodiscard]] bool saveProject(const Project& project, std::string* error = nullptr);

// Creates a ready-to-run project: manifest, starter scene and starter script.
// Existing files are never overwritten.
[[nodiscard]] std::optional<Project> createProject(const std::filesystem::path& directory,
                                                   std::string_view name,
                                                   std::string* error = nullptr);

}  // namespace waypoint
