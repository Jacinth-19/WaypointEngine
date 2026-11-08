// Waypoint Engine — SDL3 window and event pump.
// SPDX-License-Identifier: Apache-2.0
//
// Only compiled when rendering is enabled. Everything SDL-specific lives here:
// the rest of the engine talks to `Input` and to the native window handles that
// this class exposes for the renderer.
#pragma once

#include <cstdint>
#include <string>

#include "core/Types.hpp"
#include "platform/Input.hpp"

namespace waypoint {

class SdlWindow {
public:
    struct Config {
        std::string title = "Waypoint Engine";
        int width = 1280;
        int height = 720;
        bool resizable = true;
        bool startWithRelativeMouse = false;
    };

    SdlWindow() = default;
    ~SdlWindow();

    SdlWindow(const SdlWindow&) = delete;
    SdlWindow& operator=(const SdlWindow&) = delete;

    // Initialises the SDL video subsystem. Call once per process.
    static bool initializeSubsystem(std::string* error = nullptr);
    static void shutdownSubsystem();
    [[nodiscard]] static bool subsystemInitialized();

    [[nodiscard]] bool create(const Config& config, std::string* error = nullptr);
    void destroy();
    [[nodiscard]] bool created() const;

    // Drains the event queue into `input`. Sets the quit flag on SDL_QUIT,
    // Escape is *not* handled here because scripts may want it.
    void pollEvents(Input& input);

    [[nodiscard]] bool shouldQuit() const { return m_shouldQuit; }
    void requestQuit() { m_shouldQuit = true; }

    // Relative (captured) mouse mode: the cursor disappears and motion arrives
    // as deltas, which is what a first-person camera needs.
    void setRelativeMouseMode(bool enabled);
    [[nodiscard]] bool relativeMouseMode() const { return m_relativeMouse; }
    void toggleRelativeMouseMode() { setRelativeMouseMode(!m_relativeMouse); }

    // Growable window: the renderer asks once per frame and resets it.
    [[nodiscard]] bool consumeResize(u16& width, u16& height);
    [[nodiscard]] u16 width() const { return m_width; }
    [[nodiscard]] u16 height() const { return m_height; }

    // Native handles handed to bgfx; the meaning depends on the video driver.
    [[nodiscard]] void* nativeWindow() const { return m_nativeWindow; }
    [[nodiscard]] void* nativeDisplay() const { return m_nativeDisplay; }
    // 0 = bgfx default handle type, 1 = Wayland surface.
    [[nodiscard]] int nativeWindowType() const { return m_nativeWindowType; }

    // Folder the executable lives in. SDL caches this pointer and it must never
    // be freed, so it is returned as a plain std::string.
    [[nodiscard]] static std::string basePath();

private:
    void* m_window = nullptr;
    void* m_nativeWindow = nullptr;
    void* m_nativeDisplay = nullptr;
    int m_nativeWindowType = 0;
    u16 m_width = 1280;
    u16 m_height = 720;
    u16 m_pendingWidth = 0;
    u16 m_pendingHeight = 0;
    bool m_relativeMouse = false;
    bool m_shouldQuit = false;
};

}  // namespace waypoint
