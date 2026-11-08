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

struct LuaScriptEngine::Impl {
    sol::state lua;
    const Input* input = nullptr;
    bool loadedSuccessfully = false;

    Impl() {
        lua.open_libraries(sol::lib::base, sol::lib::string, sol::lib::math, sol::lib::table,
                           sol::lib::os, sol::lib::coroutine);
        bindApi();
    }

    void bindApi() {
        sol::table waypoint = lua.create_named_table("waypoint");

        waypoint.set_function("log", [this](sol::variadic_args args) {
            Log::info("lua: {}", stringifyArguments(lua, args));
        });
        waypoint.set_function("warn", [this](sol::variadic_args args) {
            Log::warn("lua: {}", stringifyArguments(lua, args));
        });
        waypoint.set_function("error", [this](sol::variadic_args args) {
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

    [[nodiscard]] bool hasFunction(const char* name) const {
        const sol::object value = lua[std::string(name)];
        return value.is<sol::protected_function>() || value.is<sol::function>();
    }

    bool describeResult(const sol::protected_function_result& result, const char* hookName,
                        std::string* error) {
        if (result.valid()) {
            return true;
        }
        const std::string message =
            std::string("error in ") + hookName + "(): " + describeError(result);
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
    m_impl->loadedSuccessfully = false;

    sol::load_result loaded =
        m_impl->lua.load(std::string(source), std::string(chunkName));
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

    m_impl->loadedSuccessfully = true;
    return true;
}

bool LuaScriptEngine::callOnStart(std::string* error) {
    if (!m_impl->hasFunction("on_start")) {
        return true;
    }
    const sol::protected_function function = m_impl->lua["on_start"];
    const sol::protected_function_result result = function();
    return m_impl->describeResult(result, "on_start", error);
}

bool LuaScriptEngine::callOnUpdate(f32 deltaSeconds, std::string* error) {
    if (!m_impl->hasFunction("on_update")) {
        return true;
    }
    const sol::protected_function function = m_impl->lua["on_update"];
    const sol::protected_function_result result = function(deltaSeconds);
    return m_impl->describeResult(result, "on_update", error);
}

bool LuaScriptEngine::hasOnStart() const {
    return m_impl->hasFunction("on_start");
}

bool LuaScriptEngine::hasOnUpdate() const {
    return m_impl->hasFunction("on_update");
}

bool LuaScriptEngine::loaded() const {
    return m_impl->loadedSuccessfully;
}

void LuaScriptEngine::reset() {
    const Input* input = m_impl->input;
    m_impl->lua = sol::state{};
    m_impl = std::make_unique<Impl>();
    m_impl->input = input;
}

}  // namespace waypoint
