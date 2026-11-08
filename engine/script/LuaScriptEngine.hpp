// Waypoint Engine — Lua scripting.
// SPDX-License-Identifier: Apache-2.0
//
// Gameplay code lives in Lua files next to the project, so changing behaviour
// never needs a rebuild. A script may define two functions:
//
//   function on_start()          -- once, after the scene loads
//   function on_update(dt)       -- every frame, dt in seconds
//
// and can call the `waypoint` API:
//
//   waypoint.log("hello")
//   waypoint.input.is_key_down("space")
//
// The sol2 dependency is hidden behind a pimpl, so projects that disable
// scripting never pay for it (and the header stays cheap to include).
#pragma once

#include <filesystem>
#include <memory>
#include <string>
#include <string_view>

#include "core/Types.hpp"
#include "platform/Input.hpp"

namespace waypoint {

class LuaScriptEngine {
public:
    LuaScriptEngine();
    ~LuaScriptEngine();

    LuaScriptEngine(const LuaScriptEngine&) = delete;
    LuaScriptEngine& operator=(const LuaScriptEngine&) = delete;
    LuaScriptEngine(LuaScriptEngine&&) noexcept;
    LuaScriptEngine& operator=(LuaScriptEngine&&) noexcept;

    // Scripts ask this object about keys and the mouse. It is not owned, and it
    // may be null: every input call then answers "not pressed".
    void setInput(const Input* input);

    // Loads and runs a script file. Returns false and fills `error` on missing
    // files or Lua syntax/runtime errors while executing the chunk.
    [[nodiscard]] bool loadFile(const std::filesystem::path& path, std::string* error = nullptr);
    [[nodiscard]] bool loadString(std::string_view source, std::string_view chunkName = "script",
                                  std::string* error = nullptr);

    // Runs a hook if the script defined it. Returns false only when the hook
    // ran and raised an error; a missing hook counts as success.
    [[nodiscard]] bool callOnStart(std::string* error = nullptr);
    [[nodiscard]] bool callOnUpdate(f32 deltaSeconds, std::string* error = nullptr);

    [[nodiscard]] bool hasOnStart() const;
    [[nodiscard]] bool hasOnUpdate() const;

    // True once a script has been loaded successfully.
    [[nodiscard]] bool loaded() const;

    // Throws away the Lua state and every value held in it.
    void reset();

private:
    struct Impl;
    std::unique_ptr<Impl> m_impl;
};

}  // namespace waypoint
