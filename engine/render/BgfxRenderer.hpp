// Waypoint Engine — bgfx renderer.
// SPDX-License-Identifier: Apache-2.0
//
// One view, one shader program, vertex-coloured geometry: the cube and the grid
// exist to prove the whole path (window -> device -> shader -> pixels) works.
// Lighting and materials come next; see docs/ROADMAP.md.
//
// bgfx types are hidden behind a pimpl so including this header stays cheap.
#pragma once

#include <memory>
#include <string>

#include "core/Types.hpp"
#include "ecs/World.hpp"

namespace waypoint {

// Which kind of native handle the window passed in. Wayland needs to say so;
// every other platform uses the default.
enum class NativeWindowKind : u8 {
    Default = 0,
    Wayland = 1,
};

struct RendererConfig {
    void* nativeWindow = nullptr;
    void* nativeDisplay = nullptr;
    NativeWindowKind nativeWindowKind = NativeWindowKind::Default;
    u16 width = 1280;
    u16 height = 720;
    // Folder containing the compiled shaders, laid out as <dir>/<profile>/*.bin.
    std::string shaderDirectory;
    // Empty or "auto" picks the best backend for the machine. The
    // WAYPOINT_RENDERER environment variable overrides this at runtime.
    std::string preferredRenderer;
    bool vsync = true;
    bool debug = false;
};

class BgfxRenderer {
public:
    BgfxRenderer();
    ~BgfxRenderer();

    BgfxRenderer(const BgfxRenderer&) = delete;
    BgfxRenderer& operator=(const BgfxRenderer&) = delete;
    BgfxRenderer(BgfxRenderer&&) noexcept;
    BgfxRenderer& operator=(BgfxRenderer&&) noexcept;

    [[nodiscard]] bool initialize(const RendererConfig& config, std::string* error = nullptr);
    void shutdown();
    [[nodiscard]] bool initialized() const;

    // Call after the window changes size. Recreates the back buffer.
    void resize(u16 width, u16 height);

    // Starts a frame: clears the colour buffer, sets the camera and draws the
    // ground grid. `fieldOfViewDegrees` is vertical.
    void beginFrame(const Transform& cameraTransform, f32 fieldOfViewDegrees);

    // Draws every entity that has a MeshRendererComponent and a Transform.
    // Returns the number of meshes submitted.
    std::size_t drawWorld(const World& world);

    // Presents the frame.
    void endFrame();

    [[nodiscard]] u16 width() const;
    [[nodiscard]] u16 height() const;

    // "Vulkan", "OpenGL", "Metal", ... as reported by bgfx.
    [[nodiscard]] std::string backendName() const;

    // False when the .bin shaders could not be found; the frame then shows the
    // debug-text explanation instead of geometry.
    [[nodiscard]] bool shadersLoaded() const;

    // Asks bgfx to write the next frame to `path` (PPM). Best effort.
    void requestScreenshot(const std::string& path);

private:
    struct Impl;
    std::unique_ptr<Impl> m_impl;
};

}  // namespace waypoint
