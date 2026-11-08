// Waypoint Engine — keyboard and mouse state (no SDL dependency).
// SPDX-License-Identifier: Apache-2.0
//
// Keeping input SDL-free means scripts, gameplay code and tests can read the
// keyboard without opening a window. SdlWindow translates SDL3 events into the
// calls below.
//
//   if (input.isKeyDown(Key::W)) { ... }        // held right now
//   if (input.isKeyPressed(Key::Escape)) { ... } // went down this frame
#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <string>
#include <string_view>
#include <utility>

#include "core/Types.hpp"

namespace waypoint {

// One entry per physical key the engine understands.
enum class Key : std::uint16_t {
    Unknown = 0,
    A, B, C, D, E, F, G, H, I, J, K, L, M,
    N, O, P, Q, R, S, T, U, V, W, X, Y, Z,
    Num0, Num1, Num2, Num3, Num4, Num5, Num6, Num7, Num8, Num9,
    Space,
    Escape,
    Return,
    Tab,
    Backspace,
    Delete,
    Insert,
    Home,
    End,
    PageUp,
    PageDown,
    Left,
    Right,
    Up,
    Down,
    LeftShift,
    RightShift,
    LeftControl,
    RightControl,
    LeftAlt,
    RightAlt,
    LeftSuper,
    RightSuper,
    F1, F2, F3, F4, F5, F6, F7, F8, F9, F10, F11, F12,
    Minus,
    Equals,
    LeftBracket,
    RightBracket,
    Backslash,
    Semicolon,
    Apostrophe,
    Comma,
    Period,
    Slash,
    Grave,
    Count
};

inline constexpr std::size_t kKeyCount = static_cast<std::size_t>(Key::Count);

enum class MouseButton : std::uint8_t {
    Unknown = 0,
    Left,
    Middle,
    Right,
    X1,
    X2,
    Count
};

inline constexpr std::size_t kMouseButtonCount = static_cast<std::size_t>(MouseButton::Count);

// Turns a friendly name into a Key. Case-insensitive; `_`, `-` and spaces are
// ignored, so "Page Up", "page_up" and "PAGEUP" are the same key. Common
// short forms are accepted ("esc", "return", "shift", "ctrl", "alt", "cmd").
// Anything unknown returns Key::Unknown.
[[nodiscard]] Key keyFromName(std::string_view name);

// The canonical lowercase name for a key, used in error messages and tooling.
[[nodiscard]] std::string_view keyName(Key key);

// "left", "right", "middle", "x1", "x2". Unknown names map to Unknown.
[[nodiscard]] MouseButton mouseButtonFromName(std::string_view name);
[[nodiscard]] std::string_view mouseButtonName(MouseButton button);

class Input {
public:
    // Call once at the top of every frame. Copies the key state so that
    // "pressed this frame" works, and resets mouse deltas.
    void beginFrame();

    // --- key state -----------------------------------------------------------
    void setKeyDown(Key key, bool down);
    void clearKeys();

    [[nodiscard]] bool isKeyDown(Key key) const;
    // True on the first frame a key is held. Repeats are ignored.
    [[nodiscard]] bool isKeyPressed(Key key) const;
    // True on the first frame after a key is released.
    [[nodiscard]] bool isKeyReleased(Key key) const;

    // --- mouse state ---------------------------------------------------------
    void setMouseButtonDown(MouseButton button, bool down);
    void clearMouseButtons();

    [[nodiscard]] bool isMouseDown(MouseButton button) const;
    [[nodiscard]] bool isMousePressed(MouseButton button) const;
    [[nodiscard]] bool isMouseReleased(MouseButton button) const;

    // Adds relative motion in pixels. Call as often as the OS reports it; the
    // values are summed until the next beginFrame().
    void addMouseMotion(f32 deltaX, f32 deltaY);
    [[nodiscard]] std::pair<f32, f32> mouseDelta() const { return {m_mouseDeltaX, m_mouseDeltaY}; }

    void addMouseWheel(f32 delta);
    [[nodiscard]] f32 mouseWheel() const { return m_mouseWheel; }

    void setMousePosition(f32 x, f32 y);
    [[nodiscard]] std::pair<f32, f32> mousePosition() const { return {m_mouseX, m_mouseY}; }

    // Forgets everything (used when the window loses focus).
    void reset();

private:
    std::array<bool, kKeyCount> m_keys{};
    std::array<bool, kKeyCount> m_previousKeys{};
    std::array<bool, kMouseButtonCount> m_mouseButtons{};
    std::array<bool, kMouseButtonCount> m_previousMouseButtons{};
    f32 m_mouseDeltaX = 0.0f;
    f32 m_mouseDeltaY = 0.0f;
    f32 m_mouseWheel = 0.0f;
    f32 m_mouseX = 0.0f;
    f32 m_mouseY = 0.0f;
};

}  // namespace waypoint
