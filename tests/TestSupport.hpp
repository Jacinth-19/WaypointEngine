// Waypoint Engine — shared test helpers.
// SPDX-License-Identifier: Apache-2.0
#pragma once

#include <filesystem>
#include <functional>
#include <ostream>
#include <string>
#include <string_view>

namespace waypoint::test {

// A unique, empty folder that deletes itself when the test ends. Tests never
// touch the repository, so a failed test cannot leave junk behind.
class TempDir {
public:
    TempDir() {
        static int counter = 0;
        path_ = std::filesystem::temp_directory_path() /
                ("waypoint_test_" + std::to_string(++counter) + "_" +
                 std::to_string(static_cast<unsigned long long>(
                     std::hash<std::string>{}("waypoint"))));
        std::error_code code;
        std::filesystem::remove_all(path_, code);
        std::filesystem::create_directories(path_, code);
    }

    ~TempDir() {
        std::error_code code;
        std::filesystem::remove_all(path_, code);
    }

    TempDir(const TempDir&) = delete;
    TempDir& operator=(const TempDir&) = delete;

    [[nodiscard]] const std::filesystem::path& path() const { return path_; }
    [[nodiscard]] std::filesystem::path operator/(std::string_view name) const {
        return path_ / std::string(name);
    }

private:
    std::filesystem::path path_;
};

}  // namespace waypoint::test
