// Waypoint Engine — logging implementation.
// SPDX-License-Identifier: Apache-2.0
#include "core/Log.hpp"

#include <memory>
#include <mutex>

#include <spdlog/sinks/stdout_color_sinks.h>
#include <spdlog/spdlog.h>

namespace waypoint {
namespace {

std::mutex g_mutex;
std::shared_ptr<spdlog::logger> g_logger;
Log::SinkCallback g_callback;
LogLevel g_level = LogLevel::Info;

spdlog::level::level_enum toSpdlog(LogLevel level) {
    switch (level) {
        case LogLevel::Trace: return spdlog::level::trace;
        case LogLevel::Debug: return spdlog::level::debug;
        case LogLevel::Info: return spdlog::level::info;
        case LogLevel::Warn: return spdlog::level::warn;
        case LogLevel::Error: return spdlog::level::err;
        case LogLevel::Critical: return spdlog::level::critical;
        case LogLevel::Off: return spdlog::level::off;
    }
    return spdlog::level::info;
}

std::shared_ptr<spdlog::logger> ensureLogger() {
    if (!g_logger) {
        auto sink = std::make_shared<spdlog::sinks::stderr_color_sink_mt>();
        g_logger = std::make_shared<spdlog::logger>("waypoint", std::move(sink));
        g_logger->set_pattern("[%H:%M:%S.%e] [%^%l%$] %v");
    }
    return g_logger;
}

}  // namespace

void Log::init(LogLevel level) {
    std::lock_guard<std::mutex> lock(g_mutex);
    auto logger = ensureLogger();
    logger->set_level(toSpdlog(level));
    g_level = level;
}

void Log::shutdown() {
    std::lock_guard<std::mutex> lock(g_mutex);
    if (g_logger) {
        g_logger->flush();
    }
    spdlog::shutdown();
    g_logger.reset();
    g_callback = nullptr;
}

void Log::setLevel(LogLevel level) {
    std::lock_guard<std::mutex> lock(g_mutex);
    ensureLogger()->set_level(toSpdlog(level));
    g_level = level;
}

LogLevel Log::level() {
    std::lock_guard<std::mutex> lock(g_mutex);
    return g_level;
}

bool Log::enabled(LogLevel level) {
    std::lock_guard<std::mutex> lock(g_mutex);
    if (level == LogLevel::Off) {
        return false;
    }
    return static_cast<int>(level) >= static_cast<int>(g_level) && g_level != LogLevel::Off;
}

void Log::setCallback(SinkCallback callback) {
    std::lock_guard<std::mutex> lock(g_mutex);
    g_callback = std::move(callback);
}

void Log::clearCallback() {
    std::lock_guard<std::mutex> lock(g_mutex);
    g_callback = nullptr;
}

void Log::write(LogLevel level, std::string_view message) {
    SinkCallback callback;
    {
        std::lock_guard<std::mutex> lock(g_mutex);
        auto logger = ensureLogger();
        switch (level) {
            case LogLevel::Trace: logger->trace(message); break;
            case LogLevel::Debug: logger->debug(message); break;
            case LogLevel::Info: logger->info(message); break;
            case LogLevel::Warn: logger->warn(message); break;
            case LogLevel::Error: logger->error(message); break;
            case LogLevel::Critical: logger->critical(message); break;
            case LogLevel::Off: return;
        }
        callback = g_callback;
    }
    if (callback) {
        callback(level, message);
    }
}

}  // namespace waypoint
