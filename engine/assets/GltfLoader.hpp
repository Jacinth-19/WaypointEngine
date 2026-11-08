// Waypoint Engine — glTF geometry loading.
// SPDX-License-Identifier: Apache-2.0
//
// The engine's first real asset format. Everything here is CPU-side: positions,
// normals, colours and indices in plain vectors, with no GPU types and no bgfx.
// That is deliberate - the headless test suite loads a .glb and checks the
// geometry, and turning these arrays into vertex buffers is the renderer's job.
#pragma once

#include <cstdint>
#include <filesystem>
#include <string>
#include <vector>

#include "core/Types.hpp"

namespace waypoint {

// One triangle primitive from a glTF file. Indices are always 32-bit after
// loading, whatever width the file used, so callers never have to care.
struct GltfPrimitive {
    // Name of the glTF mesh this came from, or "mesh <i>" when it has none.
    std::string meshName;
    // Index of the primitive inside its mesh, for "file.glb#<primitiveIndex>".
    std::uint32_t primitiveIndex = 0;
    // Material name, or empty when the primitive has none.
    std::string materialName;

    std::vector<Vec3> positions;
    std::vector<Vec3> normals;   // empty when the file has no NORMAL attribute
    std::vector<Vec3> colors;    // empty when the file has no COLOR_0 attribute
    std::vector<std::uint32_t> indices;
};

// Everything a .glb or .gltf file contributed. A file with no triangles at all
// is an error rather than an empty document, so a mis-authored asset cannot
// quietly render nothing.
struct GltfDocument {
    std::vector<GltfPrimitive> primitives;
    // glTF's own warnings, kept because "asset has no normals" is worth saying.
    std::vector<std::string> warnings;
};

// Loads `path` (.glb or .gltf). Returns false and fills `error` when the file is
// missing, is not valid glTF, or contains no triangles.
[[nodiscard]] bool loadGltf(const std::filesystem::path& path,
                            GltfDocument& document,
                            std::string* error = nullptr);

// Builds the reference a scene uses to point at one primitive:
// "assets/cube.glb#2" for the third primitive of that file, or the bare path
// when the file has a single primitive. Used by the mesh registry.
[[nodiscard]] std::string gltfReference(const std::filesystem::path& path,
                                        std::uint32_t primitiveIndex,
                                        std::size_t primitiveCount);

}  // namespace waypoint
