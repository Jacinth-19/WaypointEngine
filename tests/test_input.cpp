// Tests for the SDL-free input layer, including the Lua key-name mapping.
// SPDX-License-Identifier: Apache-2.0
#include <doctest/doctest.h>

#include "platform/Input.hpp"

using namespace waypoint;

TEST_CASE("Input: key names are case-insensitive and ignore separators") {
    CHECK(keyFromName("W") == Key::W);
    CHECK(keyFromName("w") == Key::W);
    CHECK(keyFromName("space") == Key::Space);
    CHECK(keyFromName("SPACE") == Key::Space);
    CHECK(keyFromName("Page Up") == Key::PageUp);
    CHECK(keyFromName("page_up") == Key::PageUp);
    CHECK(keyFromName("page-up") == Key::PageUp);
    CHECK(keyFromName("PAGEUP") == Key::PageUp);
    CHECK(keyFromName("1") == Key::Num1);
    CHECK(keyFromName("num1") == Key::Num1);
    CHECK(keyFromName("f12") == Key::F12);
    CHECK(keyFromName("F1") == Key::F1);
}

TEST_CASE("Input: friendly aliases match the physical keys") {
    CHECK(keyFromName("return") == Key::Return);
    CHECK(keyFromName("enter") == Key::Return);
    CHECK(keyFromName("esc") == Key::Escape);
    CHECK(keyFromName("escape") == Key::Escape);
    CHECK(keyFromName("shift") == Key::LeftShift);
    CHECK(keyFromName("ctrl") == Key::LeftControl);
    CHECK(keyFromName("control") == Key::LeftControl);
    CHECK(keyFromName("alt") == Key::LeftAlt);
    CHECK(keyFromName("cmd") == Key::LeftSuper);
    CHECK(keyFromName("left") == Key::Left);
    CHECK(keyFromName("arrowleft") == Key::Left);
    CHECK(keyFromName("up") == Key::Up);
    CHECK(keyFromName("arrowup") == Key::Up);
    CHECK(keyFromName("dot") == Key::Period);
    CHECK(keyFromName("backtick") == Key::Grave);
}

TEST_CASE("Input: unknown names map to Key::Unknown") {
    CHECK(keyFromName("") == Key::Unknown);
    CHECK(keyFromName("banana") == Key::Unknown);
    CHECK(keyFromName("   ") == Key::Unknown);
    CHECK(keyFromName("f13") == Key::Unknown);
    CHECK(keyFromName("--") == Key::Unknown);
}

TEST_CASE("Input: canonical key names round-trip") {
    for (std::uint16_t i = 1; i < static_cast<std::uint16_t>(Key::Count); ++i) {
        const auto key = static_cast<Key>(i);
        const std::string_view name = keyName(key);
        REQUIRE_MESSAGE(name != "unknown", "key " << i << " has no canonical name");
        CHECK_MESSAGE(keyFromName(name) == key, "name '" << name << "' did not round-trip");
    }
    CHECK(keyName(Key::W) == "w");
    CHECK(keyName(Key::Num0) == "0");
    CHECK(keyName(Key::PageUp) == "pageup");
}

TEST_CASE("Input: mouse buttons can be named") {
    CHECK(mouseButtonFromName("left") == MouseButton::Left);
    CHECK(mouseButtonFromName("Right") == MouseButton::Right);
    CHECK(mouseButtonFromName("middle") == MouseButton::Middle);
    CHECK(mouseButtonFromName("x1") == MouseButton::X1);
    CHECK(mouseButtonFromName("nope") == MouseButton::Unknown);
    CHECK(mouseButtonName(MouseButton::Right) == "right");
}

TEST_CASE("Input: held, pressed and released are different questions") {
    Input input;
    CHECK_FALSE(input.isKeyDown(Key::W));
    CHECK_FALSE(input.isKeyPressed(Key::W));

    input.setKeyDown(Key::W, true);
    CHECK(input.isKeyDown(Key::W));
    CHECK(input.isKeyPressed(Key::W));  // first frame it is down
    CHECK_FALSE(input.isKeyReleased(Key::W));

    input.beginFrame();
    CHECK(input.isKeyDown(Key::W));
    CHECK_FALSE(input.isKeyPressed(Key::W));  // held, but not new
    CHECK_FALSE(input.isKeyReleased(Key::W));

    input.setKeyDown(Key::W, false);
    CHECK_FALSE(input.isKeyDown(Key::W));
    CHECK_FALSE(input.isKeyPressed(Key::W));
    CHECK(input.isKeyReleased(Key::W));  // first frame it is up

    input.beginFrame();
    CHECK_FALSE(input.isKeyReleased(Key::W));
    CHECK_FALSE(input.isKeyPressed(Key::W));
}

TEST_CASE("Input: mouse motion accumulates until the next frame") {
    Input input;
    CHECK(input.mouseDelta() == std::pair<f32, f32>{0.0f, 0.0f});

    input.addMouseMotion(3.0f, -4.0f);
    input.addMouseMotion(2.0f, 1.0f);
    CHECK(input.mouseDelta().first == doctest::Approx(5.0f));
    CHECK(input.mouseDelta().second == doctest::Approx(-3.0f));

    input.beginFrame();
    CHECK(input.mouseDelta().first == doctest::Approx(0.0f));
    CHECK(input.mouseDelta().second == doctest::Approx(0.0f));
}

TEST_CASE("Input: wheel and buttons behave like keys") {
    Input input;
    input.addMouseWheel(1.0f);
    input.addMouseWheel(1.0f);
    CHECK(input.mouseWheel() == doctest::Approx(2.0f));
    input.beginFrame();
    CHECK(input.mouseWheel() == doctest::Approx(0.0f));

    input.setMouseButtonDown(MouseButton::Right, true);
    CHECK(input.isMouseDown(MouseButton::Right));
    CHECK(input.isMousePressed(MouseButton::Right));
    input.beginFrame();
    CHECK(input.isMouseDown(MouseButton::Right));
    CHECK_FALSE(input.isMousePressed(MouseButton::Right));
}

TEST_CASE("Input: reset and focus loss forget every key") {
    Input input;
    input.setKeyDown(Key::Escape, true);
    input.setMouseButtonDown(MouseButton::Left, true);
    input.addMouseMotion(10.0f, 10.0f);

    input.reset();
    CHECK_FALSE(input.isKeyDown(Key::Escape));
    CHECK_FALSE(input.isMouseDown(MouseButton::Left));
    CHECK(input.mouseDelta().first == doctest::Approx(0.0f));
}
