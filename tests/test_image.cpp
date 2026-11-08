// Tests for the screenshot writer.
// SPDX-License-Identifier: Apache-2.0
//
// The renderer gives back raw RGBA and someone has to turn it into a file a
// human can open. That code is deliberately outside the renderer so it can be
// tested here, on every platform, without a GPU: write an image, decode it again
// with stb_image and compare the pixels.
#include "TestDoctest.hpp"

#include <cstdint>
#include <filesystem>
#include <fstream>
#include <string>
#include <vector>

#include "core/ImageWrite.hpp"
#include "TestSupport.hpp"

// Declarations only: the implementation lives in engine/assets/StbImage.cpp, so
// the test binary links against the same code the engine uses.
#include <stb_image.h>

using namespace waypoint;
using waypoint::test::TempDir;

namespace {

// Four recognisable pixels: red, green, blue, white.
std::vector<std::uint8_t> testPixels() {
    return {
        255, 0, 0, 255,   0, 255, 0, 255,
        0, 0, 255, 255,   255, 255, 255, 255,
    };
}

}  // namespace

TEST_CASE("writeImage writes a PNG that decodes back to the same pixels") {
    const TempDir dir;
    const auto path = dir.path() / "shot.png";
    const std::vector<std::uint8_t> pixels = testPixels();

    std::string error;
    REQUIRE(writeImage(path, 2, 2, pixels.data(), &error));

    int width = 0;
    int height = 0;
    int channels = 0;
    stbi_uc* decoded = stbi_load(path.string().c_str(), &width, &height, &channels, 4);
    REQUIRE(decoded != nullptr);

    CHECK(width == 2);
    CHECK(height == 2);
    CHECK(channels == 4);
    for (std::size_t i = 0; i < pixels.size(); ++i) {
        INFO("byte " << i);
        CHECK(decoded[i] == pixels[i]);
    }
    stbi_image_free(decoded);
}

TEST_CASE("writeImage writes lossless formats exactly") {
    const TempDir dir;
    const std::vector<std::uint8_t> pixels = testPixels();

    for (const char* extension : {".bmp", ".tga"}) {
        const auto path = dir.path() / ("shot" + std::string(extension));
        std::string error;
        REQUIRE(writeImage(path, 2, 2, pixels.data(), &error));

        int width = 0;
        int height = 0;
        int channels = 0;
        stbi_uc* decoded = stbi_load(path.string().c_str(), &width, &height, &channels, 4);
        REQUIRE(decoded != nullptr);
        INFO(extension);
        CHECK(width == 2);
        CHECK(height == 2);
        for (std::size_t i = 0; i < pixels.size(); ++i) {
            CHECK(decoded[i] == pixels[i]);
        }
        stbi_image_free(decoded);
    }
}

TEST_CASE("writeImage creates the folder it is asked to write into") {
    const TempDir dir;
    const auto path = dir.path() / "nested" / "deeper" / "shot.png";
    const std::vector<std::uint8_t> pixels = testPixels();

    std::string error;
    CHECK(writeImage(path, 2, 2, pixels.data(), &error));
    CHECK(std::filesystem::exists(path));
}

TEST_CASE("writeImage writes a JPEG that starts with the JPEG magic") {
    const TempDir dir;
    const auto path = dir.path() / "shot.jpg";
    const std::vector<std::uint8_t> pixels = testPixels();

    std::string error;
    REQUIRE(writeImage(path, 2, 2, pixels.data(), &error));

    std::ifstream file(path, std::ios::binary);
    REQUIRE(file.is_open());
    const char magic[2] = {static_cast<char>(file.get()), static_cast<char>(file.get())};
    CHECK(magic[0] == static_cast<char>(0xFF));
    CHECK(magic[1] == static_cast<char>(0xD8));
}

TEST_CASE("writeImage refuses an unknown extension instead of writing junk") {
    const TempDir dir;
    const auto path = dir.path() / "shot.gif";
    const std::vector<std::uint8_t> pixels = testPixels();

    std::string error;
    CHECK_FALSE(writeImage(path, 2, 2, pixels.data(), &error));
    CHECK(error.find("unknown image format") != std::string::npos);
    CHECK_FALSE(std::filesystem::exists(path));
}

TEST_CASE("writeImage refuses an empty image") {
    const TempDir dir;
    const auto path = dir.path() / "shot.png";

    std::string error;
    CHECK_FALSE(writeImage(path, 0, 2, nullptr, &error));
    CHECK_FALSE(writeImage(path, 2, 0, nullptr, &error));
    CHECK(error.find("empty") != std::string::npos);
}
