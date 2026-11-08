// Tests for core: UUIDs, files, logging, transforms.
// SPDX-License-Identifier: Apache-2.0
#include "TestDoctest.hpp"

#include <functional>
#include <set>
#include <string>
#include <vector>

#include "TestSupport.hpp"
#include "core/FileSystem.hpp"
#include "core/Log.hpp"
#include "core/Types.hpp"
#include "core/Uuid.hpp"

using namespace waypoint;

TEST_CASE("Uuid: generated ids are valid, distinct and printable") {
    std::set<std::string> seen;
    for (int i = 0; i < 200; ++i) {
        const Uuid id = Uuid::generate();
        REQUIRE_FALSE(id.isNull());
        const std::string text = id.toString();
        REQUIRE(text.size() == Uuid::kStringLength);
        CHECK(text[8] == '-');
        CHECK(text[13] == '-');
        CHECK(text[18] == '-');
        CHECK(text[23] == '-');
        // Version 4 and RFC 4122 variant bits.
        CHECK(text[14] == '4');
        const char variant = text[19];
        const bool variantIsRfc4122 = variant == '8' || variant == '9' || variant == 'a' ||
                                      variant == 'b';
        CHECK(variantIsRfc4122);
        seen.insert(text);
    }
    CHECK(seen.size() == 200);
}

TEST_CASE("Uuid: null, parse and round-trip") {
    CHECK(Uuid::null().isNull());
    CHECK(Uuid::null().toString() == "00000000-0000-0000-0000-000000000000");

    const Uuid id = Uuid::generate();
    const auto parsed = Uuid::fromString(id.toString());
    REQUIRE(parsed.has_value());
    CHECK(*parsed == id);

    // Hyphens are optional, case does not matter.
    const auto plain = Uuid::fromString("3F2B1C4D8A9E4F10B7C25D6E7F809A1B");
    REQUIRE(plain.has_value());
    CHECK(plain->toString() == "3f2b1c4d-8a9e-4f10-b7c2-5d6e7f809a1b");

    // Braces, as copied from some tools, are tolerated.
    CHECK(Uuid::fromString("{3f2b1c4d-8a9e-4f10-b7c2-5d6e7f809a1b}").has_value());

    // Bad input is rejected instead of silently becoming zero.
    CHECK_FALSE(Uuid::fromString("").has_value());
    CHECK_FALSE(Uuid::fromString("not-a-uuid").has_value());
    CHECK_FALSE(Uuid::fromString("3f2b1c4d8a9e4f10b7c25d6e7f809a1").has_value());
    CHECK_FALSE(Uuid::fromString("3f2b1c4d8a9e4f10b7c25d6e7f809a1bb").has_value());
}

TEST_CASE("Uuid: hash agrees with equality") {
    const Uuid a = Uuid::generate();
    const Uuid b = Uuid::fromString(a.toString()).value();
    CHECK(a == b);
    CHECK(std::hash<Uuid>{}(a) == std::hash<Uuid>{}(b));
}

TEST_CASE("FileSystem: write, read and directory creation") {
    const test::TempDir temp;
    const auto nested = temp / "a/b/c.txt";
    CHECK_FALSE(FileSystem::exists(nested));

    std::string error;
    REQUIRE(FileSystem::writeTextFile(nested, "hello \xF0\x9F\x8C\x8D", &error));
    CHECK(error.empty());
    CHECK(FileSystem::exists(nested));
    CHECK(FileSystem::isRegularFile(nested));
    CHECK(FileSystem::isDirectory(temp / "a/b"));

    const auto text = FileSystem::readTextFile(nested);
    REQUIRE(text.has_value());
    CHECK(*text == "hello \xF0\x9F\x8C\x8D");

    // Reading something that is not there must not throw.
    CHECK_FALSE(FileSystem::readTextFile(temp / "missing.txt").has_value());
    CHECK_FALSE(FileSystem::isRegularFile(temp / "missing.txt"));
}

TEST_CASE("FileSystem: path helpers behave the same on every platform") {
    const std::filesystem::path joined = FileSystem::joinPath("game", "scenes/main.json");
    CHECK(joined.generic_string() == "game/scenes/main.json");
    CHECK(FileSystem::genericString(joined).find('\\') == std::string::npos);

    // An absolute second argument wins over the base. Built from the platform's
    // own temp folder: "/etc/passwd" is *not* absolute on Windows, which is
    // exactly the trap this test used to fall into.
    const std::filesystem::path absolutePath =
        std::filesystem::absolute(std::filesystem::temp_directory_path() / "waypoint_abs.txt");
    CHECK(FileSystem::joinPath("game", absolutePath.generic_string()) == absolutePath);

    CHECK(FileSystem::parentDirectory("game/scenes/main.json").generic_string() ==
          "game/scenes");
    CHECK(FileSystem::parentDirectory("main.json").empty());

    CHECK(FileSystem::environmentVariable("WAYPOINT_SURELY_NOT_SET_VARIABLE").has_value() == false);
}

TEST_CASE("Log: formatting and callbacks") {
    CHECK(Log::format("loaded {} entities from {}", 3, "main.scene.json") ==
          "loaded 3 entities from main.scene.json");
    CHECK(Log::format("no placeholders") == "no placeholders");

    std::vector<std::string> captured;
    Log::init(LogLevel::Info);
    Log::setCallback([&captured](LogLevel level, std::string_view message) {
        if (level == LogLevel::Error) {
            captured.emplace_back(message);
        }
    });

    Log::info("ignored by the callback");
    Log::error("something broke: {}", 42);

    Log::clearCallback();
    Log::error("after the callback was removed");

    REQUIRE(captured.size() == 1);
    CHECK(captured[0] == "something broke: 42");

    // Level filtering is respected by enabled().
    Log::setLevel(LogLevel::Warn);
    CHECK_FALSE(Log::enabled(LogLevel::Info));
    CHECK(Log::enabled(LogLevel::Error));
    Log::setLevel(LogLevel::Off);
    CHECK_FALSE(Log::enabled(LogLevel::Critical));
    Log::setLevel(LogLevel::Info);
}

TEST_CASE("Transform: matrix composes scale, rotation and position") {
    Transform transform;
    transform.position = Vec3(1.0f, 2.0f, 3.0f);

    const Mat4 matrix = transform.matrix();
    CHECK(matrix[3].x == doctest::Approx(1.0f));
    CHECK(matrix[3].y == doctest::Approx(2.0f));
    CHECK(matrix[3].z == doctest::Approx(3.0f));

    // A +90 degree turn around Y maps local +Z onto world +X.
    transform.rotation = glm::angleAxis(glm::radians(90.0f), Vec3(0.0f, 1.0f, 0.0f));
    const Vec3 forward = transform.forward();
    CHECK(forward.x == doctest::Approx(1.0f));
    CHECK(forward.y == doctest::Approx(0.0f));
    CHECK(forward.z == doctest::Approx(0.0f));

    // Scaling by 2 doubles the local Y axis, which this rotation leaves alone.
    transform.scale = Vec3(2.0f, 2.0f, 2.0f);
    CHECK(transform.matrix()[1].y == doctest::Approx(2.0f));
}
