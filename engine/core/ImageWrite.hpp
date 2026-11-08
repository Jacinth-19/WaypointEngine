// Waypoint Engine — image writing for screenshots.
// SPDX-License-Identifier: Apache-2.0
//
// The renderer hands back raw RGBA8 pixels; something has to turn them into a
// file a human can open. This is that something, kept out of the renderer so it
// can be tested without a GPU: the headless suite writes a PNG, reads it back
// and compares pixels.
#pragma once

#include <cstdint>
#include <filesystem>
#include <string_view>

namespace waypoint {

// Writes `pixels` (width * height * 4 bytes, tightly packed RGBA8) to `path`.
// The format comes from the extension: .png, .bmp, .tga or .jpg. Returns false
// and fills `error` when the extension is unknown or the file cannot be written.
[[nodiscard]] bool writeImage(const std::filesystem::path& path,
                              std::uint32_t width,
                              std::uint32_t height,
                              const std::uint8_t* pixels,
                              std::string* error = nullptr);

}  // namespace waypoint
