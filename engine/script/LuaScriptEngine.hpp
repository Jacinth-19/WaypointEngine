// Waypoint Engine — Lua scripting.
// SPDX-License-Identifier: Apache-2.0
//
// Gameplay code lives in Lua files next to the project, so changing behaviour
// never needs a rebuild. A script may define two functions:
//
//   function on_start(handle)    -- once, after the scene loads
//   function on_update(dt, handle)  -- every frame, dt in seconds
//
// `handle` is the entity the script is attached to, so the same file can drive
// several entities and still tell them apart. Scripts that ignore it keep
// working: Lua discards arguments a function does not declare.
//
// One script file may be the project's entry script (the manifest's
// `scriptEntry`, loaded with loadFile), or be attached per entity through
// ScriptComponent - which gets its own environment, so two entities running the
// same file do not share globals.
//
// A script can call the `waypoint` API:
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
#include "ecs/World.hpp"
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

    // Loads a *separate* environment for one entity's ScriptComponent and
    // returns its id, or 0 when the file could not be read. Each entity gets its
    // own environment, so two entities sharing a file have separate globals.
    [[nodiscard]] std::uint32_t loadEntityScript(const std::filesystem::path& path,
                                                 EntityHandle entity,
                                                 std::string* error = nullptr);

    // Runs a hook across every environment: the entry script first, then the
    // per-entity ones in load order. One broken script is reported and the rest
    // still run - a single bad file must not freeze the game.
    [[nodiscard]] bool callOnStartAll(std::string* error = nullptr);
    [[nodiscard]] bool callOnUpdateAll(f32 deltaSeconds, std::string* error = nullptr);

    [[nodiscard]] bool hasOnStart() const;
    [[nodiscard]] bool hasOnUpdate() const;
    [[nodiscard]] std::size_t environmentCount() const;

    // True once a script has been loaded successfully.
    [[nodiscard]] bool loaded() const;

    // Throws away the Lua state and every value held in it.
    void reset();

private:
    struct Impl;
    std::unique_ptr<Impl> m_impl;
};

}  // namespace waypoint
