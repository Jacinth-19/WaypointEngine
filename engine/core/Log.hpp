// Waypoint Engine — logging.
// SPDX-License-Identifier: Apache-2.0
//
// Logging writes human-readable lines to the terminal (and, in the editor, to
// the console panel). Use it instead of std::cout so every message is
// timestamped and can be filtered by level.
#pragma once

#include <functional>
#include <string>
#include <string_view>
#include <utility>

#include <spdlog/fmt/fmt.h>

namespace waypoint {

// Higher levels are more severe. `Off` silences everything.
enum class LogLevel { Trace, Debug, Info, Warn, Error, Critical, Off };

class Log {
public:
    // Receives every message after it has been formatted. The editor installs
    // a callback here to mirror messages into its console dock.
    using SinkCallback = std::function<void(LogLevel, std::string_view)>;

    static void init(LogLevel level = LogLevel::Info);
    static void shutdown();

    static void setLevel(LogLevel level);
    [[nodiscard]] static LogLevel level();
    [[nodiscard]] static bool enabled(LogLevel level);

    static void setCallback(SinkCallback callback);
    static void clearCallback();

    // Everything below accepts fmt-style placeholders, e.g.
    //   Log::info("Loaded {} entities from {}", count, path)
    template <typename... Args>
    static void trace(std::string_view message, Args&&... args) {
        if (enabled(LogLevel::Trace)) {
            write(LogLevel::Trace, fmt::format(fmt::runtime(message), std::forward<Args>(args)...));
        }
    }

    template <typename... Args>
    static void debug(std::string_view message, Args&&... args) {
        if (enabled(LogLevel::Debug)) {
            write(LogLevel::Debug, fmt::format(fmt::runtime(message), std::forward<Args>(args)...));
        }
    }

    template <typename... Args>
    static void info(std::string_view message, Args&&... args) {
        if (enabled(LogLevel::Info)) {
            write(LogLevel::Info, fmt::format(fmt::runtime(message), std::forward<Args>(args)...));
        }
    }

    template <typename... Args>
    static void warn(std::string_view message, Args&&... args) {
        if (enabled(LogLevel::Warn)) {
            write(LogLevel::Warn, fmt::format(fmt::runtime(message), std::forward<Args>(args)...));
        }
    }

    template <typename... Args>
    static void error(std::string_view message, Args&&... args) {
        if (enabled(LogLevel::Error)) {
            write(LogLevel::Error, fmt::format(fmt::runtime(message), std::forward<Args>(args)...));
        }
    }

    template <typename... Args>
    static void critical(std::string_view message, Args&&... args) {
        if (enabled(LogLevel::Critical)) {
            write(LogLevel::Critical,
                  fmt::format(fmt::runtime(message), std::forward<Args>(args)...));
        }
    }

    // Formats a message without emitting it. Useful in tests and tools.
    template <typename... Args>
    [[nodiscard]] static std::string format(std::string_view message, Args&&... args) {
        return fmt::format(fmt::runtime(message), std::forward<Args>(args)...);
    }

private:
    static void write(LogLevel level, std::string_view message);
};

}  // namespace waypoint
