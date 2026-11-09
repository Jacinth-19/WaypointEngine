// Waypoint Engine — Lua scripting implementation.
// SPDX-License-Identifier: Apache-2.0
#include "script/LuaScriptEngine.hpp"

#include <utility>

#include <sol/sol.hpp>

#include "core/FileSystem.hpp"
#include "core/Log.hpp"

namespace waypoint {
namespace {

// Log::setCallback and friends are process wide, but scripts should only be
// able to write log lines, never reach into the engine.
std::string stringifyArguments(sol::state& lua, sol::variadic_args args) {
    std::string text;
    bool first = true;
    const sol::protected_function tostring = lua["tostring"];
    for (const sol::object& value : args) {
        if (!first) {
            text.push_back('\t');
        }
        first = false;
        switch (value.get_type()) {
            case sol::type::string:
                text += value.as<std::string>();
                break;
            case sol::type::number:
                text += std::to_string(value.as<double>());
                break;
            case sol::type::boolean:
                text += value.as<bool>() ? "true" : "false";
                break;
            case sol::type::lua_nil:
                // `sol::type::nil` only exists when the platform has no `nil`
                // macro (Obj-C SDKs define one), so use the always-present name.
                text += "nil";
                break;
            default: {
                const sol::protected_function_result result = tostring(value);
                text += result.valid() ? result.get<std::string>() : "<value>";
                break;
            }
        }
    }
    return text;
}

// sol2 v3.3 reports errors as a Lua string sitting in the result; `get<error>()`
// wraps it with a readable message. Only call this on an invalid result.
template <typename Result>
std::string describeError(const Result& result) {
    const sol::error error = result.template get<sol::error>();
    return error.what();
}

}  // namespace

// One Lua state plus the entity it belongs to. The entry script uses
// kInvalidEntity; per-entity scripts carry their own handle.
struct ScriptEnvironment {
    sol::state lua;
    EntityHandle entity = kInvalidEntity;
    std::string name;
};

struct LuaScriptEngine::Impl {
    // Index 0 is the project's entry script, when there is one. The rest are
    // per-entity scripts, in load order.
    std::vector<std::unique_ptr<ScriptEnvironment>> environments;
    const Input* input = nullptr;
    bool loadedSuccessfully = false;

    Impl() = default;

    void bindApi(sol::state& lua) {
        sol::table waypoint = lua.create_named_table("waypoint");

        waypoint.set_function("log", [&lua](sol::variadic_args args) {
            Log::info("lua: {}", stringifyArguments(lua, args));
        });
        waypoint.set_function("warn", [&lua](sol::variadic_args args) {
            Log::warn("lua: {}", stringifyArguments(lua, args));
        });
        waypoint.set_function("error", [&lua](sol::variadic_args args) {
            Log::error("lua: {}", stringifyArguments(lua, args));
        });

        sol::table inputApi = waypoint.create_named("input");

        inputApi.set_function("is_key_down", [this](std::string name) {
            if (input == nullptr) {
                return false;
            }
            const Key key = keyFromName(name);
            return key != Key::Unknown && input->isKeyDown(key);
        });
        inputApi.set_function("is_key_pressed", [this](std::string name) {
            if (input == nullptr) {
                return false;
            }
            const Key key = keyFromName(name);
            return key != Key::Unknown && input->isKeyPressed(key);
        });
        inputApi.set_function("is_mouse_down", [this](std::string name) {
            if (input == nullptr) {
                return false;
            }
            const MouseButton button = mouseButtonFromName(name);
            return button != MouseButton::Unknown && input->isMouseDown(button);
        });
        // Two return values: local dx, dy = waypoint.input.mouse_delta()
        inputApi.set_function("mouse_delta", [this]() {
            if (input == nullptr) {
                return std::make_pair(0.0f, 0.0f);
            }
            return input->mouseDelta();
        });
    }

    static bool hasFunction(const sol::state& lua, const char* name) {
        const sol::object value = lua[std::string(name)];
        return value.is<sol::protected_function>() || value.is<sol::function>();
    }

    // Creates a state with the engine API bound. Every environment gets its own,
    // so scripts cannot read or clobber each other's globals.
    std::unique_ptr<ScriptEnvironment> makeEnvironment(std::string name, EntityHandle entity) {
        auto environment = std::make_unique<ScriptEnvironment>();
        environment->name = std::move(name);
        environment->entity = entity;
        environment->lua.open_libraries(sol::lib::base, sol::lib::string, sol::lib::math,
                                        sol::lib::table, sol::lib::os, sol::lib::coroutine);
        bindApi(environment->lua);
        return environment;
    }

    // Runs one hook in one environment, passing the entity handle.
    bool callHook(ScriptEnvironment& environment, const char* hookName, f32 deltaSeconds,
                  bool passEntity, std::string* error) {
        if (!hasFunction(environment.lua, hookName)) {
            return true;  // a script without this hook is not an error
        }
        const sol::protected_function function = environment.lua[hookName];
        const sol::protected_function_result result =
            passEntity ? function(deltaSeconds, environment.entity) : function(environment.entity);
        if (result.valid()) {
            return true;
        }
        const std::string message = "error in " + environment.name + " " + hookName + "(): " +
                                    describeError(result);
        Log::error("{}", message);
        if (error != nullptr) {
            *error = message;
        }
        return false;
    }

};

LuaScriptEngine::LuaScriptEngine() : m_impl(std::make_unique<Impl>()) {}

LuaScriptEngine::~LuaScriptEngine() = default;

LuaScriptEngine::LuaScriptEngine(LuaScriptEngine&&) noexcept = default;
LuaScriptEngine& LuaScriptEngine::operator=(LuaScriptEngine&&) noexcept = default;

void LuaScriptEngine::setInput(const Input* input) {
    m_impl->input = input;
}

bool LuaScriptEngine::loadFile(const std::filesystem::path& path, std::string* error) {
    const auto source = FileSystem::readTextFile(path);
    if (!source.has_value()) {
        const std::string message = "could not read script: " + path.generic_string();
        if (error != nullptr) {
            *error = message;
        }
        Log::error("{}", message);
        return false;
    }
    return loadString(*source, path.generic_string(), error);
}

bool LuaScriptEngine::loadString(std::string_view source, std::string_view chunkName,
                                std::string* error) {
    // The entry script replaces whatever was loaded before, so `loaded()` keeps
    // meaning "the project's entry script is in place".
    m_impl->loadedSuccessfully = false;
    m_impl->environments.clear();

    auto environment = m_impl->makeEnvironment(std::string(chunkName), kInvalidEntity);
    sol::load_result loaded = environment->lua.load(std::string(source), std::string(chunkName));
    if (!loaded.valid()) {
        const std::string message = "syntax error in " + std::string(chunkName) + ": " +
                                    describeError(loaded);
        if (error != nullptr) {
            *error = message;
        }
        Log::error("{}", message);
        return false;
    }

    const sol::protected_function chunk = loaded;
    const sol::protected_function_result result = chunk();
    if (!result.valid()) {
        const std::string message = "error running " + std::string(chunkName) + ": " +
                                    describeError(result);
        if (error != nullptr) {
            *error = message;
        }
        Log::error("{}", message);
        return false;
    }

    m_impl->environments.push_back(std::move(environment));
    m_impl->loadedSuccessfully = true;
    return true;
}

bool LuaScriptEngine::callOnStart(std::string* error) {
    if (m_impl->environments.empty()) {
        return true;
    }
    return m_impl->callHook(*m_impl->environments.front(), "on_start", 0.0f, false, error);
}

bool LuaScriptEngine::callOnUpdate(f32 deltaSeconds, std::string* error) {
    if (m_impl->environments.empty()) {
        return true;
    }
    return m_impl->callHook(*m_impl->environments.front(), "on_update", deltaSeconds, true, error);
}

std::uint32_t LuaScriptEngine::loadEntityScript(const std::filesystem::path& path,
                                                EntityHandle entity, std::string* error) {
    const auto source = FileSystem::readTextFile(path);
    if (!source.has_value()) {
        const std::string message = "could not read script: " + path.generic_string();
        if (error != nullptr) {
            *error = message;
        }
        Log::error("{}", message);
        return 0;
    }

    auto environment = m_impl->makeEnvironment(path.generic_string(), entity);
    sol::load_result loaded = environment->lua.load(*source, path.generic_string());
    if (!loaded.valid()) {
        const std::string message =
            "syntax error in " + path.generic_string() + ": " + describeError(loaded);
        if (error != nullptr) {
            *error = message;
        }
        Log::error("{}", message);
        return 0;
    }

    const sol::protected_function chunk = loaded;
    const sol::protected_function_result result = chunk();
    if (!result.valid()) {
        const std::string message =
            "error running " + path.generic_string() + ": " + describeError(result);
        if (error != nullptr) {
            *error = message;
        }
        Log::error("{}", message);
        return 0;
    }

    m_impl->environments.push_back(std::move(environment));
    return static_cast<std::uint32_t>(m_impl->environments.size());
}

bool LuaScriptEngine::callOnStartAll(std::string* error) {
    bool ok = true;
    for (auto& environment : m_impl->environments) {
        if (!m_impl->callHook(*environment, "on_start", 0.0f, false, error)) {
            ok = false;
        }
    }
    return ok;
}

bool LuaScriptEngine::callOnUpdateAll(f32 deltaSeconds, std::string* error) {
    bool ok = true;
    for (auto& environment : m_impl->environments) {
        if (!m_impl->callHook(*environment, "on_update", deltaSeconds, true, error)) {
            ok = false;
        }
    }
    return ok;
}

bool LuaScriptEngine::hasOnStart() const {
    return !m_impl->environments.empty() &&
           Impl::hasFunction(m_impl->environments.front()->lua, "on_start");
}

bool LuaScriptEngine::hasOnUpdate() const {
    return !m_impl->environments.empty() &&
           Impl::hasFunction(m_impl->environments.front()->lua, "on_update");
}

std::size_t LuaScriptEngine::environmentCount() const {
    return m_impl->environments.size();
}

bool LuaScriptEngine::loaded() const {
    return m_impl->loadedSuccessfully;
}

void LuaScriptEngine::reset() {
    const Input* input = m_impl->input;
    m_impl = std::make_unique<Impl>();
    m_impl->input = input;
}

}  // namespace waypoint
