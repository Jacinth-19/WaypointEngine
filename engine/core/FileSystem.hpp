// Waypoint Engine — small file helpers.
// SPDX-License-Identifier: Apache-2.0
//
// These thin wrappers keep std::filesystem usage in one place so the rest of
// the engine stays readable and every failure returns an error instead of
// throwing. Paths are UTF-8; use genericString() when writing JSON.
#pragma once

#include <cstdint>
#include <filesystem>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace waypoint::FileSystem {

using Path = std::filesystem::path;

// Reads a whole text file. Returns std::nullopt when the file cannot be read.
[[nodiscard]] std::optional<std::string> readTextFile(const Path& path);

// Reads a whole binary file (compiled shaders, images, ...).
[[nodiscard]] std::optional<std::vector<std::uint8_t>> readBinaryFile(const Path& path);

// Writes a text file, creating parent directories as needed.
// On failure returns false and fills `error` (when provided) with a reason.
bool writeTextFile(const Path& path, std::string_view contents, std::string* error = nullptr);

[[nodiscard]] bool exists(const Path& path);
[[nodiscard]] bool isDirectory(const Path& path);
[[nodiscard]] bool isRegularFile(const Path& path);

// Creates `path` and any missing parents. Returns true if it now exists.
bool ensureDirectory(const Path& path);

[[nodiscard]] Path joinPath(const Path& base, std::string_view relative);
[[nodiscard]] Path absolute(const Path& path);

// Always uses '/' separators, so the same string works in JSON on every OS.
[[nodiscard]] std::string genericString(const Path& path);

// Directory that contains the running executable. Used to find `shaders/`.
[[nodiscard]] Path executableDirectory();

// Directory that contains a project or scene file, with a trailing separator
// removed. Returns an empty path for a bare filename.
[[nodiscard]] Path parentDirectory(const Path& path);

// Reads an environment variable. Returns std::nullopt when unset or empty.
[[nodiscard]] std::optional<std::string> environmentVariable(std::string_view name);

}  // namespace waypoint::FileSystem
