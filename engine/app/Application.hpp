// Waypoint Engine — the game loop.
// SPDX-License-Identifier: Apache-2.0
//
// Application wires the subsystems together in the order they must run:
// window -> events -> scripts/camera -> physics -> render -> present.
//
// It is only compiled when rendering is enabled; a headless build has no window
// to loop on, which is why `wp.py export` refuses to run headless.
#pragma once

#include <filesystem>
#include <string>

#include "core/Types.hpp"
#include "platform/Input.hpp"
#include "project/Project.hpp"
#include "scene/Scene.hpp"

namespace waypoint {

struct ApplicationConfig {
    Project project;
    // Scene to open. Defaults to the project's startScene.
    std::filesystem::path scenePath;
    bool vsync = true;
    bool debugRenderer = false;
    // Stop after one rendered frame. Used by smoke tests that need to prove a
    // window and device came up without leaving a window open.
    bool exitAfterFirstFrame = false;
    // Write the next rendered frame to this path and exit. The extension picks
    // the format (.png, .bmp, .tga, .jpg). Implies a short run: the frame has to
    // be presented before bgfx can hand the pixels over.
    std::filesystem::path screenshotPath;
};

class Application {
public:
    Application();
    ~Application();

    Application(const Application&) = delete;
    Application& operator=(const Application&) = delete;

    // Runs until the player quits. Returns 0 on a clean exit, non-zero when the
    // window, renderer or scene could not be created.
    [[nodiscard]] int run(const ApplicationConfig& config);

private:
    class Impl;
    Impl* m_impl = nullptr;
};

}  // namespace waypoint
