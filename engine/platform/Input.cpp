// Waypoint Engine — input implementation.
// SPDX-License-Identifier: Apache-2.0
#include "platform/Input.hpp"

#include <algorithm>
#include <array>
#include <cctype>
#include <unordered_map>

namespace waypoint {
namespace {

std::string normalize(std::string_view name) {
    std::string result;
    result.reserve(name.size());
    for (const char c : name) {
        const auto uc = static_cast<unsigned char>(c);
        if (c == '_' || c == '-' || c == ' ' || c == '\t' || c == '.') {
            continue;  // "page_up" == "pageup" == "Page Up"
        }
        result.push_back(static_cast<char>(std::tolower(uc)));
    }
    return result;
}

using KeyTable = std::unordered_map<std::string, Key>;

const KeyTable& keyTable() {
    static const KeyTable table = [] {
        KeyTable map;
        const auto add = [&map](Key key, std::string_view name) {
            map.emplace(normalize(name), key);
        };

        // Letters.
        for (int i = 0; i < 26; ++i) {
            const auto key = static_cast<Key>(static_cast<std::uint16_t>(Key::A) + i);
            const char name[2] = {static_cast<char>('a' + i), '\0'};
            map.emplace(name, key);
        }

        // Digits, with and without a "num" prefix.
        for (int i = 0; i < 10; ++i) {
            const auto key = static_cast<Key>(static_cast<std::uint16_t>(Key::Num0) + i);
            const std::string name = std::to_string(i);
            map.emplace(name, key);
            map.emplace("num" + name, key);
            map.emplace("digit" + name, key);
        }

        add(Key::Space, "space");
        add(Key::Space, "spacebar");
        add(Key::Escape, "escape");
        add(Key::Escape, "esc");
        add(Key::Return, "return");
        add(Key::Return, "enter");
        add(Key::Tab, "tab");
        add(Key::Backspace, "backspace");
        add(Key::Delete, "delete");
        add(Key::Delete, "del");
        add(Key::Insert, "insert");
        add(Key::Insert, "ins");
        add(Key::Home, "home");
        add(Key::End, "end");
        add(Key::PageUp, "pageup");
        add(Key::PageUp, "pgup");
        add(Key::PageDown, "pagedown");
        add(Key::PageDown, "pgdown");
        add(Key::Left, "left");
        add(Key::Left, "arrowleft");
        add(Key::Right, "right");
        add(Key::Right, "arrowright");
        add(Key::Up, "up");
        add(Key::Up, "arrowup");
        add(Key::Down, "down");
        add(Key::Down, "arrowdown");

        add(Key::LeftShift, "shift");
        add(Key::LeftShift, "lshift");
        add(Key::LeftShift, "leftshift");
        add(Key::RightShift, "rightshift");
        add(Key::RightShift, "rshift");
        add(Key::LeftControl, "control");
        add(Key::LeftControl, "ctrl");
        add(Key::LeftControl, "lctrl");
        add(Key::LeftControl, "leftctrl");
        add(Key::LeftControl, "leftcontrol");
        add(Key::RightControl, "rightctrl");
        add(Key::RightControl, "rightcontrol");
        add(Key::RightControl, "rctrl");
        add(Key::LeftAlt, "alt");
        add(Key::LeftAlt, "lalt");
        add(Key::LeftAlt, "leftalt");
        add(Key::RightAlt, "altgr");
        add(Key::RightAlt, "rightalt");
        add(Key::RightAlt, "ralt");
        add(Key::LeftSuper, "super");
        add(Key::LeftSuper, "meta");
        add(Key::LeftSuper, "cmd");
        add(Key::LeftSuper, "command");
        add(Key::LeftSuper, "leftsuper");
        add(Key::LeftSuper, "win");
        add(Key::RightSuper, "rightsuper");

        for (int i = 0; i < 12; ++i) {
            const auto key = static_cast<Key>(static_cast<std::uint16_t>(Key::F1) + i);
            map.emplace("f" + std::to_string(i + 1), key);
        }

        add(Key::Minus, "minus");
        add(Key::Minus, "dash");
        add(Key::Equals, "equals");
        add(Key::Equals, "equal");
        add(Key::LeftBracket, "leftbracket");
        add(Key::LeftBracket, "lbracket");
        add(Key::LeftBracket, "[");
        add(Key::RightBracket, "rightbracket");
        add(Key::RightBracket, "rbracket");
        add(Key::RightBracket, "]");
        add(Key::Backslash, "backslash");
        add(Key::Semicolon, "semicolon");
        add(Key::Apostrophe, "apostrophe");
        add(Key::Apostrophe, "quote");
        add(Key::Comma, "comma");
        add(Key::Period, "period");
        add(Key::Period, "dot");
        add(Key::Slash, "slash");
        add(Key::Grave, "grave");
        add(Key::Grave, "backtick");
        return map;
    }();
    return table;
}

// Canonical names, in the same order as the Key enum. Written out by hand so
// that keyName() is stable no matter how many aliases keyFromName() accepts.
const std::array<std::string_view, kKeyCount>& keyNames() {
    static const std::array<std::string_view, kKeyCount> names = {{
        "unknown",
        "a", "b", "c", "d", "e", "f", "g", "h", "i", "j", "k", "l", "m",
        "n", "o", "p", "q", "r", "s", "t", "u", "v", "w", "x", "y", "z",
        "0", "1", "2", "3", "4", "5", "6", "7", "8", "9",
        "space", "escape", "return", "tab", "backspace", "delete", "insert", "home", "end",
        "pageup", "pagedown",
        "left", "right", "up", "down",
        "leftshift", "rightshift", "leftcontrol", "rightcontrol",
        "leftalt", "rightalt", "leftsuper", "rightsuper",
        "f1", "f2", "f3", "f4", "f5", "f6", "f7", "f8", "f9", "f10", "f11", "f12",
        "minus", "equals", "leftbracket", "rightbracket", "backslash", "semicolon",
        "apostrophe", "comma", "period", "slash", "grave",
    }};
    static_assert(names.size() == kKeyCount, "key name table must cover every Key");
    return names;
}

std::size_t indexOf(Key key) {
    const auto index = static_cast<std::size_t>(key);
    return index < kKeyCount ? index : 0;
}

std::size_t indexOf(MouseButton button) {
    const auto index = static_cast<std::size_t>(button);
    return index < kMouseButtonCount ? index : 0;
}

}  // namespace

Key keyFromName(std::string_view name) {
    const std::string normalized = normalize(name);
    if (normalized.empty()) {
        return Key::Unknown;
    }
    const auto& table = keyTable();
    if (const auto it = table.find(normalized); it != table.end()) {
        return it->second;
    }
    return Key::Unknown;
}

std::string_view keyName(Key key) {
    return keyNames()[indexOf(key)];
}

MouseButton mouseButtonFromName(std::string_view name) {
    const std::string normalized = normalize(name);
    if (normalized == "left" || normalized == "l") return MouseButton::Left;
    if (normalized == "middle" || normalized == "m" || normalized == "wheel") {
        return MouseButton::Middle;
    }
    if (normalized == "right" || normalized == "r") return MouseButton::Right;
    if (normalized == "x1" || normalized == "back" || normalized == "4") return MouseButton::X1;
    if (normalized == "x2" || normalized == "forward" || normalized == "5") {
        return MouseButton::X2;
    }
    return MouseButton::Unknown;
}

std::string_view mouseButtonName(MouseButton button) {
    switch (button) {
        case MouseButton::Left: return "left";
        case MouseButton::Middle: return "middle";
        case MouseButton::Right: return "right";
        case MouseButton::X1: return "x1";
        case MouseButton::X2: return "x2";
        case MouseButton::Unknown:
        case MouseButton::Count:
            break;
    }
    return "unknown";
}

void Input::beginFrame() {
    m_previousKeys = m_keys;
    m_previousMouseButtons = m_mouseButtons;
    m_mouseDeltaX = 0.0f;
    m_mouseDeltaY = 0.0f;
    m_mouseWheel = 0.0f;
}

void Input::setKeyDown(Key key, bool down) {
    m_keys[indexOf(key)] = down;
}

void Input::clearKeys() {
    m_keys.fill(false);
    m_previousKeys.fill(false);
}

bool Input::isKeyDown(Key key) const {
    return m_keys[indexOf(key)];
}

bool Input::isKeyPressed(Key key) const {
    const std::size_t index = indexOf(key);
    return m_keys[index] && !m_previousKeys[index];
}

bool Input::isKeyReleased(Key key) const {
    const std::size_t index = indexOf(key);
    return !m_keys[index] && m_previousKeys[index];
}

void Input::setMouseButtonDown(MouseButton button, bool down) {
    m_mouseButtons[indexOf(button)] = down;
}

void Input::clearMouseButtons() {
    m_mouseButtons.fill(false);
    m_previousMouseButtons.fill(false);
}

bool Input::isMouseDown(MouseButton button) const {
    return m_mouseButtons[indexOf(button)];
}

bool Input::isMousePressed(MouseButton button) const {
    const std::size_t index = indexOf(button);
    return m_mouseButtons[index] && !m_previousMouseButtons[index];
}

bool Input::isMouseReleased(MouseButton button) const {
    const std::size_t index = indexOf(button);
    return !m_mouseButtons[index] && m_previousMouseButtons[index];
}

void Input::addMouseMotion(f32 deltaX, f32 deltaY) {
    m_mouseDeltaX += deltaX;
    m_mouseDeltaY += deltaY;
}

void Input::addMouseWheel(f32 delta) {
    m_mouseWheel += delta;
}

void Input::setMousePosition(f32 x, f32 y) {
    m_mouseX = x;
    m_mouseY = y;
}

void Input::reset() {
    m_keys.fill(false);
    m_previousKeys.fill(false);
    m_mouseButtons.fill(false);
    m_previousMouseButtons.fill(false);
    m_mouseDeltaX = 0.0f;
    m_mouseDeltaY = 0.0f;
    m_mouseWheel = 0.0f;
}

}  // namespace waypoint
