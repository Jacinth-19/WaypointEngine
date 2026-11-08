// Waypoint Engine — image writing for screenshots.
// SPDX-License-Identifier: Apache-2.0
//
// stb_image_write is a single header that needs its implementation compiled in
// exactly one translation unit; this is that unit. It picks the encoder from the
// file extension, so `--screenshot shot.png` and `shot.jpg` both just work.
#include "core/ImageWrite.hpp"

#include <algorithm>
#include <cctype>
#include <vector>

#include "core/FileSystem.hpp"

#define STB_IMAGE_WRITE_IMPLEMENTATION
#include <stb_image_write.h>

namespace waypoint {

bool writeImage(const std::filesystem::path& path,
                std::uint32_t width,
                std::uint32_t height,
                const std::uint8_t* pixels,
                std::string* error) {
    auto fail = [&error](std::string message) {
        if (error != nullptr) {
            *error = std::move(message);
        }
        return false;
    };

    if (pixels == nullptr || width == 0 || height == 0) {
        return fail("nothing to write: the image is empty");
    }

    // A screenshot of a 4K display is 33 MB; anything larger is a bug, and
    // stb would quietly write a broken file rather than say so.
    constexpr std::uint64_t kMaxPixels = 64u * 1024u * 1024u;
    if (static_cast<std::uint64_t>(width) * height > kMaxPixels) {
        return fail("image is too large to write (" + std::to_string(width) + "x" +
                    std::to_string(height) + ")");
    }

    std::string extension = path.extension().string();
    std::transform(extension.begin(), extension.end(), extension.begin(),
                   [](unsigned char c) { return static_cast<char>(std::tolower(c)); });

    // stb writes through a FILE*, and the parent folder has to exist first:
    // a screenshot path in a folder the user has not created yet is the normal
    // case, not an error.
    const FileSystem::Path parent = path.parent_path();
    if (!parent.empty() && !FileSystem::ensureDirectory(parent)) {
        return fail("could not create the folder " + FileSystem::genericString(parent));
    }
    const int stride = static_cast<int>(width) * 4;
    int ok = 0;
    if (extension == ".png") {
        ok = stbi_write_png(path.string().c_str(), static_cast<int>(width),
                            static_cast<int>(height), 4, pixels, stride);
    } else if (extension == ".bmp") {
        ok = stbi_write_bmp(path.string().c_str(), static_cast<int>(width),
                            static_cast<int>(height), 4, pixels);
    } else if (extension == ".tga") {
        ok = stbi_write_tga(path.string().c_str(), static_cast<int>(width),
                            static_cast<int>(height), 4, pixels);
    } else if (extension == ".jpg" || extension == ".jpeg") {
        ok = stbi_write_jpg(path.string().c_str(), static_cast<int>(width),
                            static_cast<int>(height), 4, pixels, 90);
    } else {
        return fail("unknown image format '" + extension +
                    "'; use .png, .bmp, .tga, .jpg or .jpeg");
    }

    if (ok == 0) {
        return fail("could not write " + FileSystem::genericString(path));
    }
    return true;
}

}  // namespace waypoint
