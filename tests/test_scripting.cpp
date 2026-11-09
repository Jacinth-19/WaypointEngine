// Tests for the Lua scripting engine and the `waypoint` script API.
// SPDX-License-Identifier: Apache-2.0
#include "TestDoctest.hpp"

#include <filesystem>
#include <string>
#include <vector>

#include "TestSupport.hpp"
#include "core/FileSystem.hpp"
#include "core/Log.hpp"
#include "platform/Input.hpp"
#include "script/LuaScriptEngine.hpp"

using namespace waypoint;
using waypoint::test::TempDir;

namespace {

// Captures everything Lua logs so tests can assert on it.
class LogCapture {
 public:
    LogCapture() {
        Log::init(LogLevel::Info);
        Log::setCallback([this](LogLevel level, std::string_view message) {
            messages.emplace_back(std::string(levelName(level)) + ": " + std::string(message));
        });
    }
    ~LogCapture() { Log::clearCallback(); }

    [[nodiscard]] bool contains(std::string_view needle) const {
        for (const std::string& message : messages) {
            if (message.find(needle) != std::string::npos) {
                return true;
            }
        }
        return false;
    }

    static const char* levelName(LogLevel level) {
        switch (level) {
            case LogLevel::Warn: return "warn";
            case LogLevel::Error: return "error";
            case LogLevel::Debug: return "debug";
            case LogLevel::Trace: return "trace";
            case LogLevel::Critical: return "critical";
            default: return "info";
        }
    }

    std::vector<std::string> messages;
};

}  // namespace

TEST_CASE("Lua: on_start and on_update run with the documented signatures") {
    LogCapture capture;
    LuaScriptEngine engine;

    std::string error;
    REQUIRE_MESSAGE(engine.loadString(R"(
        started = false
        frames = 0
        lastDt = 0

        function on_start()
            started = true
            waypoint.log("started")
        end

        function on_update(dt)
            frames = frames + 1
            lastDt = dt
        end
    )",
                                      "test_chunk", &error),
                    error);

    CHECK(engine.loaded());
    CHECK(engine.hasOnStart());
    CHECK(engine.hasOnUpdate());

    REQUIRE_MESSAGE(engine.callOnStart(&error), error);
    CHECK(capture.contains("info: lua: started"));

    REQUIRE_MESSAGE(engine.callOnUpdate(0.25f, &error), error);
    REQUIRE_MESSAGE(engine.callOnUpdate(0.25f, &error), error);
    CHECK(error.empty());
}

TEST_CASE("Lua: scripts without hooks are valid and hooks are skipped") {
    LuaScriptEngine engine;
    std::string error;
    REQUIRE(engine.loadString("x = 1", "quiet", &error));
    CHECK_FALSE(engine.hasOnStart());
    CHECK_FALSE(engine.hasOnUpdate());
    CHECK(engine.callOnStart(&error));
    CHECK(engine.callOnUpdate(0.016f, &error));
    CHECK(error.empty());
}

TEST_CASE("Lua: waypoint.log/warn/error reach the engine log") {
    LogCapture capture;
    LuaScriptEngine engine;
    std::string error;
    REQUIRE_MESSAGE(engine.loadString(R"(
        waypoint.log("plain", 7, true)
        waypoint.warn("careful")
        waypoint.error("broken")
    )",
                                      "logging", &error),
                    error);

    CHECK(capture.contains("info: lua: plain"));
    CHECK(capture.contains("warn: lua: careful"));
    CHECK(capture.contains("error: lua: broken"));
}

TEST_CASE("Lua: input queries honour key names and the bound Input") {
    LuaScriptEngine engine;
    Input input;
    engine.setInput(&input);

    std::string error;
    REQUIRE_MESSAGE(engine.loadString(R"(
        seen = { down = false, pressed = false, mouse = false, dx = 0, dy = 0 }

        function on_update(dt)
            local down = waypoint.input.is_key_down("Space")
            local pressed = waypoint.input.is_key_pressed("space")
            local mouse = waypoint.input.is_mouse_down("right")
            local dx, dy = waypoint.input.mouse_delta()
            waypoint.log("state", down, pressed, mouse, dx, dy)
        end
    )",
                                      "input", &error),
                    error);

    input.setKeyDown(Key::Space, true);
    input.setMouseButtonDown(MouseButton::Right, true);
    input.addMouseMotion(3.0f, -2.0f);

    LogCapture capture;
    REQUIRE_MESSAGE(engine.callOnUpdate(0.016f, &error), error);
    // Space is held and newly pressed, right mouse is down, and both deltas
    // arrive in the same order the engine recorded them.
    CHECK(capture.contains("state\ttrue\ttrue\ttrue"));
}

TEST_CASE("Lua: a missing Input answers 'nothing is pressed' instead of crashing") {
    LuaScriptEngine engine;
    std::string error;
    REQUIRE_MESSAGE(engine.loadString(R"(
        function on_start()
            local down = waypoint.input.is_key_down("w")
            local mouse = waypoint.input.is_mouse_down("left")
            local dx, dy = waypoint.input.mouse_delta()
            waypoint.log("no input", down, mouse, dx, dy)
        end
    )",
                                      "no_input", &error),
                    error);

    LogCapture capture;
    REQUIRE_MESSAGE(engine.callOnStart(&error), error);
    CHECK(capture.contains("false"));
}

TEST_CASE("Lua: syntax errors and runtime errors are reported, not thrown") {
    LuaScriptEngine engine;
    std::string error;

    CHECK_FALSE(engine.loadString("function on_start(", "broken", &error));
    CHECK(error.find("syntax error") != std::string::npos);
    CHECK_FALSE(engine.loaded());

    error.clear();
    REQUIRE(engine.loadString("function on_start() error('boom') end", "throws", &error));
    CHECK_FALSE(engine.callOnStart(&error));
    CHECK(error.find("on_start") != std::string::npos);
    CHECK(error.find("boom") != std::string::npos);
}

TEST_CASE("Lua: unknown key names are treated as 'not pressed'") {
    LuaScriptEngine engine;
    Input input;
    engine.setInput(&input);

    std::string error;
    REQUIRE_MESSAGE(engine.loadString(R"(
        function on_start()
            waypoint.log("unknown key:", waypoint.input.is_key_down("banana"))
        end
    )",
                                      "unknown_key", &error),
                    error);

    LogCapture capture;
    REQUIRE_MESSAGE(engine.callOnStart(&error), error);
    CHECK(capture.contains("unknown key:\tfalse"));
}

TEST_CASE("Lua: loadFile reads from disk and reports missing files") {
    const test::TempDir temp;
    const auto path = temp / "scripts/hello.lua";
    std::string error;
    REQUIRE(FileSystem::writeTextFile(path,
                                      "function on_start() waypoint.log('from a file') end\n",
                                      &error));

    LuaScriptEngine engine;
    REQUIRE_MESSAGE(engine.loadFile(path, &error), error);
    CHECK(engine.hasOnStart());

    LogCapture capture;
    REQUIRE_MESSAGE(engine.callOnStart(&error), error);
    CHECK(capture.contains("from a file"));

    LuaScriptEngine other;
    error.clear();
    CHECK_FALSE(other.loadFile(temp / "scripts/missing.lua", &error));
    CHECK(error.find("could not read script") != std::string::npos);
}

TEST_CASE("Lua: reset() clears state so scripts cannot leak into each other") {
    LuaScriptEngine engine;
    std::string error;
    REQUIRE_MESSAGE(engine.loadString("function on_start() end", "first", &error), error);
    CHECK(engine.hasOnStart());

    engine.reset();
    CHECK_FALSE(engine.loaded());
    CHECK_FALSE(engine.hasOnStart());

    REQUIRE_MESSAGE(engine.loadString("function on_update(dt) end", "second", &error), error);
    CHECK_FALSE(engine.hasOnStart());
    CHECK(engine.hasOnUpdate());
}

TEST_CASE("Lua: per-entity scripts get their own environment and their handle") {
    LogCapture capture;
    LuaScriptEngine engine;

    // The entry script first, so the ordering the engine documents holds.
    std::string error;
    REQUIRE(engine.loadString(R"(
        function on_start(handle)
            waypoint.log("entry " .. tostring(handle))
        end
    )",
                              "entry.lua", &error));

    // Two entities, same file: each must get its own globals, or a counter in
    // one would be visible in the other.
    const TempDir dir;
    const auto script = dir.path() / "spin.lua";
    REQUIRE(FileSystem::writeTextFile(script, R"(
        turns = 0
        mine = nil

        function on_start(handle)
            mine = handle
            waypoint.log("start " .. tostring(handle))
        end

        function on_update(dt, handle)
            turns = turns + 1
            waypoint.log("entity " .. tostring(handle) .. " turn " .. turns)
        end
    )",
                                     nullptr));

    const EntityHandle first = 111;
    const EntityHandle second = 222;
    REQUIRE(engine.loadEntityScript(script, first, &error) != 0);
    REQUIRE(engine.loadEntityScript(script, second, &error) != 0);
    CHECK(engine.environmentCount() == 3);

    REQUIRE(engine.callOnStartAll(&error));
    // Each environment was handed only its own entity's handle.
    CHECK(capture.contains("info: lua: start 111"));
    CHECK(capture.contains("info: lua: start 222"));

    // Two updates: the counters are per environment, so each is at 2 - not 4.
    LogCapture turnCapture;
    REQUIRE(engine.callOnUpdateAll(0.5f, &error));
    REQUIRE(engine.callOnUpdateAll(0.5f, &error));
    CHECK(turnCapture.contains("info: lua: entity 111 turn 2"));
    CHECK(turnCapture.contains("info: lua: entity 222 turn 2"));
    CHECK_FALSE(turnCapture.contains("turn 3"));
    CHECK_FALSE(turnCapture.contains("turn 4"));
}

TEST_CASE("Lua: a broken entity script does not stop the others") {
    LogCapture capture;
    LuaScriptEngine engine;
    std::string error;

    const TempDir dir;
    const auto good = dir.path() / "good.lua";
    const auto bad = dir.path() / "bad.lua";
    REQUIRE(FileSystem::writeTextFile(good, "function on_update(dt) waypoint.log(\"good\") end\n",
                                      nullptr));
    REQUIRE(FileSystem::writeTextFile(bad, "function on_update(dt) error(\"boom\") end\n", nullptr));

    REQUIRE(engine.loadEntityScript(good, 1, &error) != 0);
    REQUIRE(engine.loadEntityScript(bad, 2, &error) != 0);

    CHECK_FALSE(engine.callOnUpdateAll(0.1f, &error));
    CHECK(error.find("boom") != std::string::npos);
    // The healthy script still ran.
    CHECK(capture.contains("info: lua: good"));
}

TEST_CASE("Lua: loadEntityScript reports a missing file instead of adding an environment") {
    LuaScriptEngine engine;
    std::string error;
    const TempDir dir;

    CHECK(engine.loadEntityScript(dir.path() / "nope.lua", 7, &error) == 0);
    CHECK(error.find("could not read script") != std::string::npos);
    CHECK(engine.environmentCount() == 0);
}

TEST_CASE("Lua: scripts that ignore the handle argument still run") {
    // The documented signature grew a parameter; a script written against the
    // old one must keep working, because Lua discards extra arguments.
    LuaScriptEngine engine;
    std::string error;
    REQUIRE(engine.loadString(R"(
        calls = 0
        function on_update(dt)
            calls = calls + 1
        end
    )",
                              "old.lua", &error));
    REQUIRE(engine.callOnUpdate(0.016f, &error));
    CHECK(error.empty());
}
