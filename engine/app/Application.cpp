// Waypoint Engine — game loop implementation.
// SPDX-License-Identifier: Apache-2.0
#include "app/Application.hpp"

#include <algorithm>
#include <chrono>
#include <cstddef>
#include <string>
#include <utility>

#include "core/FileSystem.hpp"
#include "core/Log.hpp"
#include "ecs/Components.hpp"
#include "platform/SdlWindow.hpp"
#include "render/BgfxRenderer.hpp"
#include "scene/Systems.hpp"

#if WAYPOINT_HAS_PHYSICS
#include "physics/JoltPhysicsWorld.hpp"
#endif

#if WAYPOINT_HAS_SCRIPTING
#include "script/LuaScriptEngine.hpp"
#endif

namespace waypoint {
namespace {

// Finds the folder holding compiled shaders. Order:
//   1. <exe folder>/shaders        (exported game, or the build folder)
//   2. the build-time shader folder baked in by CMake
std::filesystem::path findShaderDirectory() {
    const std::filesystem::path besideExecutable =
        FileSystem::joinPath(FileSystem::executableDirectory(), "shaders");
    if (FileSystem::isDirectory(besideExecutable)) {
        return besideExecutable;
    }
#if defined(WAYPOINT_SHADER_DIR)
    const std::filesystem::path compiled{WAYPOINT_SHADER_DIR};
    if (FileSystem::isDirectory(compiled)) {
        return compiled;
    }
#endif
    return besideExecutable;  // may not exist; the renderer will warn
}

// Frames to render when capturing a screenshot: one to draw it, a couple more
// for bgfx to deliver the pixels back through the callback.
constexpr std::size_t kScreenshotFrames = 4;

// The entity the player looks through: a primary CameraComponent if there is
// one, otherwise the first camera, otherwise nothing.
Transform primaryCameraTransform(const Scene& scene, f32& fieldOfViewDegrees) {
    fieldOfViewDegrees = 60.0f;
    const auto& registry = scene.world().registry();

    const auto primary = registry.view<const CameraComponent, const TransformComponent>();
    for (const entt::entity entity : primary) {
        const auto& camera = primary.get<const CameraComponent>(entity);
        if (camera.primary) {
            fieldOfViewDegrees = camera.fieldOfViewDegrees;
            return primary.get<const TransformComponent>(entity).transform;
        }
    }
    for (const entt::entity entity : primary) {
        fieldOfViewDegrees = primary.get<const CameraComponent>(entity).fieldOfViewDegrees;
        return primary.get<const TransformComponent>(entity).transform;
    }

    // No camera in the scene: look at the origin from a friendly distance.
    Transform fallback;
    fallback.position = Vec3(0.0f, 4.0f, 12.0f);
    return fallback;
}

}  // namespace

class Application::Impl {
public:
    SdlWindow window;
    BgfxRenderer renderer;
    Input input;
    Scene scene;
#if WAYPOINT_HAS_PHYSICS
    JoltPhysicsWorld physics;
#endif
#if WAYPOINT_HAS_SCRIPTING
    LuaScriptEngine script;
#endif
};

Application::Application() : m_impl(new Impl()) {}

Application::~Application() {
    delete m_impl;
    m_impl = nullptr;
}

int Application::run(const ApplicationConfig& config) {
    Impl& app = *m_impl;

    // ---- scene -------------------------------------------------------------
    std::string error;
    auto loadedScene = Scene::loadFromFile(config.scenePath, &error);
    if (!loadedScene.has_value()) {
        Log::error("Could not open scene: {}", error);
        return 1;
    }
    app.scene = std::move(*loadedScene);
    Log::info("Scene '{}' loaded: {} entities", app.scene.name(), app.scene.world().entityCount());

    // ---- window ------------------------------------------------------------
    if (!SdlWindow::initializeSubsystem(&error)) {
        Log::error("Could not start SDL: {}", error);
        return 1;
    }

    SdlWindow::Config windowConfig;
    windowConfig.title = config.project.name.empty() ? "Waypoint Engine" : config.project.name;
    if (!app.window.create(windowConfig, &error)) {
        Log::error("{}", error);
        SdlWindow::shutdownSubsystem();
        return 1;
    }
    // The window is resizable, so start from the size SDL actually gave us.
    windowConfig.width = app.window.width();
    windowConfig.height = app.window.height();

    // ---- renderer ----------------------------------------------------------
    RendererConfig rendererConfig;
    rendererConfig.nativeWindow = app.window.nativeWindow();
    rendererConfig.nativeDisplay = app.window.nativeDisplay();
    rendererConfig.nativeWindowKind = app.window.nativeWindowType() == 1
                                          ? NativeWindowKind::Wayland
                                          : NativeWindowKind::Default;
    rendererConfig.width = static_cast<u16>(windowConfig.width);
    rendererConfig.height = static_cast<u16>(windowConfig.height);
    rendererConfig.shaderDirectory = FileSystem::genericString(findShaderDirectory());
    rendererConfig.vsync = config.vsync;
    rendererConfig.debug = config.debugRenderer;

    if (!app.renderer.initialize(rendererConfig, &error)) {
        Log::error("{}", error);
        app.window.destroy();
        SdlWindow::shutdownSubsystem();
        return 1;
    }
    Log::info("Shaders: {}", app.renderer.shadersLoaded() ? "loaded" : "missing (fallback screen)");

    // ---- physics -----------------------------------------------------------
#if WAYPOINT_HAS_PHYSICS
    if (!app.physics.initialize({}, &error)) {
        Log::warn("Physics unavailable: {}", error);
    } else {
        int bodies = 0;
        const auto bodiesView =
            app.scene.world().registry().view<const RigidBodyComponent, const TransformComponent>();
        for (const entt::entity entity : bodiesView) {
            const auto& body = bodiesView.get<const RigidBodyComponent>(entity);
            const auto& transform = bodiesView.get<const TransformComponent>(entity).transform;
            const EntityHandle handle = app.scene.world().toHandle(entity);
            if (app.physics.addBoxBody(handle, transform, body.halfExtents, body.motion, body.mass,
                                       body.useGravity)) {
                ++bodies;
            }
        }
        Log::info("Physics: {} bodies created", bodies);
    }
#endif

    // ---- scripting ---------------------------------------------------------
#if WAYPOINT_HAS_SCRIPTING
    app.script.setInput(&app.input);
    bool scriptReady = false;
    if (!config.project.scriptEntry.empty()) {
        const std::filesystem::path scriptPath = config.project.scriptEntryPath();
        if (FileSystem::isRegularFile(scriptPath)) {
            scriptReady = app.script.loadFile(scriptPath, &error);
            if (!scriptReady) {
                Log::warn("Script problem: {}", error);
            }
        } else {
            Log::warn("No script at {} - running without gameplay logic",
                      scriptPath.generic_string());
        }
    }
    if (scriptReady && app.script.hasOnStart()) {
        if (!app.script.callOnStart(&error)) {
            Log::warn("{}", error);
        }
    }
#endif

    // ---- loop --------------------------------------------------------------
    Log::info(
        "Controls: WASD move, Space/Ctrl up and down, Shift boost, right mouse or F1 "
        "to look, Escape quits.");

    auto previousTime = std::chrono::steady_clock::now();
    std::size_t frames = 0;
    std::size_t drawsSubmitted = 0;
    // A screenshot needs the frame presented first, and bgfx delivers the pixels
    // on a later frame than the one that was requested, so a capture runs a few
    // frames rather than stopping at one.
    const bool capturing = !config.screenshotPath.empty();
    const std::size_t frameLimit = capturing ? kScreenshotFrames : 1;
    bool running = true;
    while (running) {
        const auto now = std::chrono::steady_clock::now();
        f32 deltaSeconds = std::chrono::duration<f32>(now - previousTime).count();
        previousTime = now;
        // Clamp so a stalled frame (or a debugger break) cannot fling the camera
        // across the map or queue up a second of physics.
        deltaSeconds = std::clamp(deltaSeconds, 0.0f, 0.1f);

        app.input.beginFrame();
        app.window.pollEvents(app.input);

        if (app.window.shouldQuit() || app.input.isKeyPressed(Key::Escape)) {
            running = false;
        }
        if (app.input.isKeyPressed(Key::F1)) {
            app.window.toggleRelativeMouseMode();
        }
        // Holding the right mouse button captures the pointer, the way most
        // third-person/first-person games do.
        if (app.input.isMousePressed(MouseButton::Right)) {
            app.window.setRelativeMouseMode(true);
        }
        if (app.input.isMouseReleased(MouseButton::Right) && !app.input.isKeyDown(Key::F1)) {
            app.window.setRelativeMouseMode(false);
        }

        const bool mouseLookEnabled = app.window.relativeMouseMode();

        u16 newWidth = 0;
        u16 newHeight = 0;
        if (app.window.consumeResize(newWidth, newHeight)) {
            app.renderer.resize(newWidth, newHeight);
            Log::debug("Window resized to {}x{}", newWidth, newHeight);
        }

        FlyCameraSystem::update(app.scene.world(), app.input, deltaSeconds, mouseLookEnabled);

#if WAYPOINT_HAS_PHYSICS
        app.physics.step(deltaSeconds);
        app.physics.syncTransforms(app.scene.world());
#endif

#if WAYPOINT_HAS_SCRIPTING
        if (scriptReady && app.script.hasOnUpdate()) {
            if (!app.script.callOnUpdate(deltaSeconds, &error)) {
                Log::warn("{}", error);
            }
        }
#endif

        f32 fieldOfView = 60.0f;
        const Transform camera = primaryCameraTransform(app.scene, fieldOfView);
        app.renderer.beginFrame(camera, fieldOfView);
        drawsSubmitted += app.renderer.drawWorld(app.scene.world());
        app.renderer.endFrame();

        ++frames;
        if (capturing && frames == 1) {
            app.renderer.requestScreenshot(FileSystem::genericString(config.screenshotPath));
            Log::info("Capturing a frame to {}", FileSystem::genericString(config.screenshotPath));
        }
        if (config.exitAfterFirstFrame || (capturing && frames >= frameLimit)) {
            running = false;
        }
    }

    // ---- teardown (reverse order) ------------------------------------------
#if WAYPOINT_HAS_SCRIPTING
    app.script.reset();
#endif
#if WAYPOINT_HAS_PHYSICS
    app.physics.shutdown();
#endif
    const std::string backend = app.renderer.backendName();
    const bool shadersLoaded = app.renderer.shadersLoaded();
    app.renderer.shutdown();
    app.window.destroy();
    SdlWindow::shutdownSubsystem();

    if (capturing) {
        // bgfx writes the file from inside a later frame, so its absence means
        // either the backend cannot capture (Noop has no back buffer) or the path
        // was refused. Either way the user asked for pixels and got none.
        if (!FileSystem::exists(config.screenshotPath)) {
            Log::error("No screenshot was written to {}.{}",
                       FileSystem::genericString(config.screenshotPath),
                       backend == "Noop"
                           ? " The noop backend has no back buffer to capture;"
                             " run without WAYPOINT_RENDERER=noop."
                           : "");
            return 1;
        }
        Log::info("Screenshot saved: {}", FileSystem::genericString(config.screenshotPath));
    }

    if (config.exitAfterFirstFrame) {
        // A smoke test is only worth anything if it fails when nothing was drawn:
        // a window that opened and a frame that presented but drew no geometry
        // would look identical in the exit code otherwise.
        bool passed = frames > 0;
        if (!passed) {
            Log::error("Smoke test failed: the loop exited without submitting a frame.");
        } else if (!shadersLoaded) {
            Log::error("Smoke test failed: no shader program was created, so nothing "
                       "could be drawn. Expected compiled .bin files in '{}'.",
                       FileSystem::genericString(findShaderDirectory()));
            passed = false;
        } else if (drawsSubmitted == 0) {
            Log::error("Smoke test failed: the frame presented but submitted no draws.");
            passed = false;
        }
        if (passed) {
            Log::info("Smoke test passed: {} frame(s), {} draw call(s), {} backend, shaders loaded.",
                      frames, drawsSubmitted, backend);
        }
        return passed ? 0 : 1;
    }
    Log::info("Goodbye.");
    return 0;
}

}  // namespace waypoint
