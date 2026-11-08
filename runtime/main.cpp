// Waypoint Engine — the `waypoint` player executable.
// SPDX-License-Identifier: Apache-2.0
//
// Usage:
//   waypoint <project-folder>
//   waypoint <project-folder> --scene scenes/other.scene.json
//   waypoint <project-folder> --smoke-test
//   waypoint --version
//
// A bare .scene.json path also works, which is handy while learning the format.
#include <cstring>
#include <string>

#include "core/FileSystem.hpp"
#include "core/Log.hpp"
#include "project/Project.hpp"

#if WAYPOINT_HAS_RENDER
#include "app/Application.hpp"
#endif

namespace {

void printUsage() {
    std::printf(
        "Waypoint Engine %s\n"
        "\n"
        "Usage:\n"
        "  waypoint <project-folder>                  play the project's start scene\n"
        "  waypoint <project-folder> --scene <file>   play a specific scene\n"
        "  waypoint <file.scene.json>                 play a single scene\n"
        "  waypoint <project-folder> --smoke-test     render one frame, then exit\n"
        "  waypoint --version                         print the version\n"
        "\n"
        "Controls: WASD move, Space/LeftControl up and down, Shift boost,\n"
        "          hold the right mouse button (or press F1) to look around,\n"
        "          Escape quits.\n"
        "\n"
        "The WAYPOINT_RENDERER environment variable picks a graphics backend:\n"
        "  vulkan, opengl, opengles, d3d11, d3d12, metal  (default: auto)\n"
        "  noop   - no GPU at all; validates the submit path, used by CI\n",
        std::string(waypoint::kEngineVersion).c_str());
}

bool endsWith(const std::string& text, const std::string& suffix) {
    return text.size() >= suffix.size() &&
           text.compare(text.size() - suffix.size(), suffix.size(), suffix) == 0;
}

}  // namespace

int main(int argc, char** argv) {
#if !WAYPOINT_HAS_RENDER
    (void)argc;
    (void)argv;
    std::fprintf(stderr,
                 "This copy of Waypoint was built without rendering "
                 "(WAYPOINT_ENABLE_RENDER=OFF or WAYPOINT_HEADLESS_BUILD=1).\n"
                 "There is no window to open. Rebuild with rendering enabled.\n");
    return 1;
#else
    using namespace waypoint;

    Log::init(LogLevel::Info);

    std::string projectArgument;
    std::string sceneOverride;
    bool smokeTest = false;

    for (int i = 1; i < argc; ++i) {
        const std::string argument = argv[i];
        if (argument == "--help" || argument == "-h") {
            printUsage();
            return 0;
        }
        if (argument == "--version" || argument == "-v") {
            std::printf("Waypoint Engine %s\n", std::string(kEngineVersion).c_str());
            return 0;
        }
        if (argument == "--smoke-test") {
            smokeTest = true;
            continue;
        }
        if (argument == "--scene") {
            if (i + 1 >= argc) {
                Log::error("--scene needs a file path");
                return 1;
            }
            sceneOverride = argv[++i];
            continue;
        }
        if (!argument.empty() && argument[0] == '-') {
            Log::error("Unknown option '{}'. Try --help.", argument);
            return 1;
        }
        if (!projectArgument.empty()) {
            Log::error("Only one project or scene can be given at a time");
            return 1;
        }
        projectArgument = argument;
    }

    if (projectArgument.empty()) {
        printUsage();
        return 1;
    }

    const auto path = std::filesystem::path(projectArgument).lexically_normal();

    ApplicationConfig config;
    config.exitAfterFirstFrame = smokeTest;
    if (endsWith(projectArgument, ".scene.json") || FileSystem::isRegularFile(path)) {
        // A scene file on its own: no project manifest to read.
        config.project.name = path.stem().stem().generic_string();
        config.project.rootDirectory = FileSystem::parentDirectory(path);
        config.scenePath = path;
        if (config.project.name.empty()) {
            config.project.name = "Waypoint";
        }
    } else {
        std::string error;
        auto project = loadProject(path, &error);
        if (!project.has_value()) {
            Log::error("{}", error);
            Log::info("Pass a project folder (with {}) or a .scene.json file.",
                      std::string(kProjectFileName));
            return 1;
        }
        config.project = *project;
        config.scenePath = config.project.startScenePath();
        Log::info("Project '{}' (engine {})", config.project.name, config.project.engineVersion);
    }

    if (!sceneOverride.empty()) {
        config.scenePath = config.project.resolve(sceneOverride);
    }
    if (!FileSystem::isRegularFile(config.scenePath)) {
        Log::error("Scene not found: {}", config.scenePath.generic_string());
        return 1;
    }

    Application application;
    const int result = application.run(config);
    Log::shutdown();
    return result;
#endif
}
