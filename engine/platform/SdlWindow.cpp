// Waypoint Engine — SDL3 window implementation.
// SPDX-License-Identifier: Apache-2.0
//
// SDL3 differences from SDL2 that matter here:
//   * SDL_GetWindowRelativeMouseMode() for the current state.
//   * Motion events carry xrel/yrel as floats.
//   * Mouse button events carry a `down` flag instead of pressed/released types.
//   * Wheel events use `direction` and can be flipped.
#include "platform/SdlWindow.hpp"

#include <SDL3/SDL.h>

#include <cstring>

#include "core/Log.hpp"

#if defined(__linux__)
#include <SDL3/SDL_vulkan.h>
#endif

namespace waypoint {
namespace {

bool g_subsystemInitialized = false;

Key translateKey(SDL_Keycode keycode) {
    // SDL keycodes for letters and digits are their ASCII values, and so are the
    // enum values in our Key enum, which keeps this readable.
    if (keycode >= SDLK_A && keycode <= SDLK_Z) {
        return static_cast<Key>(static_cast<std::uint16_t>(Key::A) + (keycode - SDLK_A));
    }
    if (keycode >= SDLK_0 && keycode <= SDLK_9) {
        return static_cast<Key>(static_cast<std::uint16_t>(Key::Num0) + (keycode - SDLK_0));
    }
    if (keycode >= SDLK_F1 && keycode <= SDLK_F12) {
        return static_cast<Key>(static_cast<std::uint16_t>(Key::F1) + (keycode - SDLK_F1));
    }

    switch (keycode) {
        case SDLK_SPACE: return Key::Space;
        case SDLK_ESCAPE: return Key::Escape;
        case SDLK_RETURN: return Key::Return;
        case SDLK_KP_ENTER: return Key::Return;
        case SDLK_TAB: return Key::Tab;
        case SDLK_BACKSPACE: return Key::Backspace;
        case SDLK_DELETE: return Key::Delete;
        case SDLK_INSERT: return Key::Insert;
        case SDLK_HOME: return Key::Home;
        case SDLK_END: return Key::End;
        case SDLK_PAGEUP: return Key::PageUp;
        case SDLK_PAGEDOWN: return Key::PageDown;
        case SDLK_LEFT: return Key::Left;
        case SDLK_RIGHT: return Key::Right;
        case SDLK_UP: return Key::Up;
        case SDLK_DOWN: return Key::Down;
        case SDLK_LSHIFT: return Key::LeftShift;
        case SDLK_RSHIFT: return Key::RightShift;
        case SDLK_LCTRL: return Key::LeftControl;
        case SDLK_RCTRL: return Key::RightControl;
        case SDLK_LALT: return Key::LeftAlt;
        case SDLK_RALT: return Key::RightAlt;
        case SDLK_LGUI: return Key::LeftSuper;
        case SDLK_RGUI: return Key::RightSuper;
        case SDLK_MINUS: return Key::Minus;
        case SDLK_EQUALS: return Key::Equals;
        case SDLK_LEFTBRACKET: return Key::LeftBracket;
        case SDLK_RIGHTBRACKET: return Key::RightBracket;
        case SDLK_BACKSLASH: return Key::Backslash;
        case SDLK_SEMICOLON: return Key::Semicolon;
        case SDLK_APOSTROPHE: return Key::Apostrophe;
        case SDLK_COMMA: return Key::Comma;
        case SDLK_PERIOD: return Key::Period;
        case SDLK_SLASH: return Key::Slash;
        case SDLK_GRAVE: return Key::Grave;
        default: return Key::Unknown;
    }
}

MouseButton translateButton(std::uint8_t button) {
    switch (button) {
        case SDL_BUTTON_LEFT: return MouseButton::Left;
        case SDL_BUTTON_MIDDLE: return MouseButton::Middle;
        case SDL_BUTTON_RIGHT: return MouseButton::Right;
        case SDL_BUTTON_X1: return MouseButton::X1;
        case SDL_BUTTON_X2: return MouseButton::X2;
        default: return MouseButton::Unknown;
    }
}

}  // namespace

SdlWindow::~SdlWindow() {
    destroy();
}

bool SdlWindow::initializeSubsystem(std::string* error) {
    if (g_subsystemInitialized) {
        return true;
    }
    if (!SDL_Init(SDL_INIT_VIDEO | SDL_INIT_EVENTS)) {
        if (error != nullptr) {
            *error = std::string("SDL_Init failed: ") + SDL_GetError();
        }
        return false;
    }
    g_subsystemInitialized = true;
    return true;
}

void SdlWindow::shutdownSubsystem() {
    if (g_subsystemInitialized) {
        SDL_Quit();
        g_subsystemInitialized = false;
    }
}

bool SdlWindow::subsystemInitialized() {
    return g_subsystemInitialized;
}

bool SdlWindow::create(const Config& config, std::string* error) {
    if (m_window != nullptr) {
        return true;
    }
    if (!initializeSubsystem(error)) {
        return false;
    }

    SDL_WindowFlags flags = SDL_WINDOW_HIGH_PIXEL_DENSITY;
    if (config.resizable) {
        flags |= SDL_WINDOW_RESIZABLE;
    }

    // Creating the window with a Vulkan flag tells SDL which surface type we
    // want; bgfx falls back to another backend if Vulkan is unavailable.
#if defined(__linux__)
    if (SDL_Vulkan_LoadLibrary(nullptr)) {
        flags |= SDL_WINDOW_VULKAN;
    }
#endif

    SDL_Window* window = SDL_CreateWindow(config.title.c_str(), config.width, config.height, flags);
    if (window == nullptr) {
        if (error != nullptr) {
            *error = std::string("SDL_CreateWindow failed: ") + SDL_GetError();
        }
        return false;
    }
    m_window = window;
    m_width = static_cast<u16>(config.width);
    m_height = static_cast<u16>(config.height);
    m_shouldQuit = false;

    // bgfx wants the window/display handles for the video driver in use.
    const char* driver = SDL_GetCurrentVideoDriver();
    const std::string driverName = driver != nullptr ? driver : "";
    SDL_PropertiesID properties = SDL_GetWindowProperties(window);

    if (driverName == "wayland") {
        m_nativeWindow = SDL_GetPointerProperty(properties, SDL_PROP_WINDOW_WAYLAND_SURFACE_POINTER,
                                                nullptr);
        m_nativeDisplay = SDL_GetPointerProperty(
            properties, SDL_PROP_WINDOW_WAYLAND_DISPLAY_POINTER, nullptr);
        m_nativeWindowType = 1;  // bgfx::NativeWindowHandleType::Wayland
    } else if (driverName == "windows") {
        m_nativeWindow =
            SDL_GetPointerProperty(properties, SDL_PROP_WINDOW_WIN32_HWND_POINTER, nullptr);
        m_nativeDisplay = nullptr;
        m_nativeWindowType = 0;
    } else if (driverName == "cocoa") {
        m_nativeWindow =
            SDL_GetPointerProperty(properties, SDL_PROP_WINDOW_COCOA_WINDOW_POINTER, nullptr);
        m_nativeDisplay = nullptr;
        m_nativeWindowType = 0;
    } else if (driverName == "x11") {
        // SDL3 exposes the X11 window as a number, not a pointer.
        const Sint64 x11Window = SDL_GetNumberProperty(properties, SDL_PROP_WINDOW_X11_WINDOW_NUMBER, 0);
        m_nativeWindow = reinterpret_cast<void*>(static_cast<std::uintptr_t>(x11Window));
        m_nativeDisplay =
            SDL_GetPointerProperty(properties, SDL_PROP_WINDOW_X11_DISPLAY_POINTER, nullptr);
        m_nativeWindowType = 0;
    } else {
        Log::warn("SDL is using the '{}' video driver; bgfx may not find a window handle",
                  driverName);
    }

    if (config.startWithRelativeMouse) {
        setRelativeMouseMode(true);
    }
    Log::info("Window created: {}x{} (driver: {})", config.width, config.height, driverName);
    return true;
}

void SdlWindow::destroy() {
    setRelativeMouseMode(false);
    if (m_window != nullptr) {
        SDL_DestroyWindow(static_cast<SDL_Window*>(m_window));
        m_window = nullptr;
    }
    m_nativeWindow = nullptr;
    m_nativeDisplay = nullptr;
    m_relativeMouse = false;
}

bool SdlWindow::created() const {
    return m_window != nullptr;
}

void SdlWindow::pollEvents(Input& input) {
    SDL_Event event;
    while (SDL_PollEvent(&event)) {
        switch (event.type) {
            case SDL_EVENT_QUIT:
                m_shouldQuit = true;
                break;
            case SDL_EVENT_KEY_DOWN: {
                const Key key = translateKey(event.key.key);
                if (key != Key::Unknown) {
                    input.setKeyDown(key, true);
                }
                break;
            }
            case SDL_EVENT_KEY_UP: {
                const Key key = translateKey(event.key.key);
                if (key != Key::Unknown) {
                    input.setKeyDown(key, false);
                }
                break;
            }
            case SDL_EVENT_MOUSE_BUTTON_DOWN:
            case SDL_EVENT_MOUSE_BUTTON_UP: {
                const MouseButton button = translateButton(event.button.button);
                if (button != MouseButton::Unknown) {
                    input.setMouseButtonDown(button, event.button.down);
                }
                break;
            }
            case SDL_EVENT_MOUSE_MOTION:
                input.addMouseMotion(event.motion.xrel, event.motion.yrel);
                input.setMousePosition(event.motion.x, event.motion.y);
                break;
            case SDL_EVENT_MOUSE_WHEEL: {
                const float direction =
                    event.wheel.direction == SDL_MOUSEWHEEL_FLIPPED ? -1.0f : 1.0f;
                input.addMouseWheel(event.wheel.y * direction);
                break;
            }
            case SDL_EVENT_WINDOW_RESIZED:
            case SDL_EVENT_WINDOW_PIXEL_SIZE_CHANGED: {
                const int width = event.window.data1;
                const int height = event.window.data2;
                if (width > 0 && height > 0) {
                    m_width = static_cast<u16>(width);
                    m_height = static_cast<u16>(height);
                    m_pendingWidth = m_width;
                    m_pendingHeight = m_height;
                }
                break;
            }
            case SDL_EVENT_WINDOW_FOCUS_LOST:
                // Forget every held key so the camera does not drift away while
                // the player is alt-tabbed.
                input.reset();
                break;
            default:
                break;
        }
    }
}

void SdlWindow::setRelativeMouseMode(bool enabled) {
    if (m_window == nullptr) {
        m_relativeMouse = false;
        return;
    }
    SDL_Window* window = static_cast<SDL_Window*>(m_window);
    if (SDL_SetWindowRelativeMouseMode(window, enabled)) {
        m_relativeMouse = enabled;
    } else {
        Log::warn("Could not set relative mouse mode: {}", SDL_GetError());
    }
}

bool SdlWindow::consumeResize(u16& width, u16& height) {
    if (m_pendingWidth == 0 || m_pendingHeight == 0) {
        return false;
    }
    width = m_pendingWidth;
    height = m_pendingHeight;
    m_pendingWidth = 0;
    m_pendingHeight = 0;
    return true;
}

std::string SdlWindow::basePath() {
    // SDL caches this string internally; freeing it would corrupt the cache.
    const char* path = SDL_GetBasePath();
    if (path == nullptr) {
        return std::string("./");
    }
    return std::string(path);
}

}  // namespace waypoint
