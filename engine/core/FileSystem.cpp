// Waypoint Engine — file helpers implementation.
// SPDX-License-Identifier: Apache-2.0
#include "core/FileSystem.hpp"

#include <cstdlib>
#include <fstream>
#include <sstream>
#include <system_error>

#if defined(_WIN32)
#include <windows.h>
#elif defined(__APPLE__)
#include <mach-o/dyld.h>
#else
#include <unistd.h>
#endif

namespace waypoint::FileSystem {
namespace fs = std::filesystem;

std::optional<std::string> readTextFile(const Path& path) {
    std::ifstream stream(path, std::ios::binary);
    if (!stream.is_open()) {
        return std::nullopt;
    }
    std::ostringstream buffer;
    buffer << stream.rdbuf();
    if (stream.bad()) {
        return std::nullopt;
    }
    return buffer.str();
}

std::optional<std::vector<std::uint8_t>> readBinaryFile(const Path& path) {
    std::ifstream stream(path, std::ios::binary | std::ios::ate);
    if (!stream.is_open()) {
        return std::nullopt;
    }
    const std::streamsize size = stream.tellg();
    if (size < 0) {
        return std::nullopt;
    }
    stream.seekg(0, std::ios::beg);
    std::vector<std::uint8_t> bytes(static_cast<std::size_t>(size));
    if (size > 0) {
        stream.read(reinterpret_cast<char*>(bytes.data()), size);
        if (!stream) {
            return std::nullopt;
        }
    }
    return bytes;
}

bool writeTextFile(const Path& path, std::string_view contents, std::string* error) {
    const Path parent = parentDirectory(path);
    if (!parent.empty() && !ensureDirectory(parent)) {
        if (error) {
            *error = "could not create directory: " + genericString(parent);
        }
        return false;
    }

    std::ofstream stream(path, std::ios::binary | std::ios::trunc);
    if (!stream.is_open()) {
        if (error) {
            *error = "could not open file for writing: " + genericString(path);
        }
        return false;
    }
    stream.write(contents.data(), static_cast<std::streamsize>(contents.size()));
    stream.flush();
    if (!stream.good()) {
        if (error) {
            *error = "failed while writing file: " + genericString(path);
        }
        return false;
    }
    return true;
}

bool exists(const Path& path) {
    std::error_code code;
    return fs::exists(path, code);
}

bool isDirectory(const Path& path) {
    std::error_code code;
    return fs::is_directory(path, code);
}

bool isRegularFile(const Path& path) {
    std::error_code code;
    return fs::is_regular_file(path, code);
}

bool ensureDirectory(const Path& path) {
    if (path.empty()) {
        return true;
    }
    std::error_code code;
    if (fs::is_directory(path, code)) {
        return true;
    }
    fs::create_directories(path, code);
    return fs::is_directory(path, code);
}

Path joinPath(const Path& base, std::string_view relative) {
    const Path relativePath{std::string(relative)};
    if (relativePath.is_absolute()) {
        return relativePath;
    }
    if (base.empty()) {
        return relativePath;
    }
    return base / relativePath;
}

Path absolute(const Path& path) {
    std::error_code code;
    const Path result = fs::absolute(path, code);
    return code ? path : result;
}

std::string genericString(const Path& path) {
    std::string text = path.generic_string();
    return text;
}

Path parentDirectory(const Path& path) {
    if (path.empty()) {
        return {};
    }
    if (path.has_parent_path() && !path.parent_path().empty()) {
        return path.parent_path();
    }
    const std::string text = path.generic_string();
    const std::size_t slash = text.find_last_of('/');
    if (slash != std::string::npos && slash > 0) {
        return Path{text.substr(0, slash)};
    }
    return {};
}

Path executableDirectory() {
    static const Path cached = [] {
        Path result;
#if defined(_WIN32)
        wchar_t buffer[MAX_PATH];
        const DWORD length = ::GetModuleFileNameW(nullptr, buffer, MAX_PATH);
        if (length > 0 && length < MAX_PATH) {
            result = Path{std::wstring(buffer, length)};
        }
#elif defined(__APPLE__)
        std::uint32_t size = 0;
        ::_NSGetExecutablePath(nullptr, &size);
        std::string buffer(size, '\0');
        if (::_NSGetExecutablePath(buffer.data(), &size) == 0) {
            result = Path{buffer.c_str()};
        }
#else
        std::error_code code;
        result = fs::read_symlink("/proc/self/exe", code);
        if (code) {
            result.clear();
        }
#endif
        if (result.empty()) {
            std::error_code pathError;
            result = fs::current_path(pathError);
            return pathError ? Path{"."} : result;
        }
        return parentDirectory(result);
    }();
    return cached;
}

std::optional<std::string> environmentVariable(std::string_view name) {
    const std::string key{name};
    if (const char* value = std::getenv(key.c_str()); value != nullptr && value[0] != '\0') {
        return std::string{value};
    }
    return std::nullopt;
}

}  // namespace waypoint::FileSystem
