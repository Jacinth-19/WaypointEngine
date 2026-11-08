// Waypoint Engine — mesh geometry, shared by the renderer and its tests.
// SPDX-License-Identifier: Apache-2.0
//
// The vertex format the shader expects, plus the function that turns a loaded
// glTF primitive into it. Keeping the conversion here - rather than inside
// BgfxRenderer - means the headless suite can check that a file's positions and
// colours survive the trip, without a GPU to upload them to.
#pragma once

#include <cstdint>
#include <vector>

#include "assets/GltfLoader.hpp"
#include "core/Types.hpp"

namespace waypoint {

// Position + packed colour, 16 bytes. bgfx expects ABGR packed into a uint32
// on little-endian machines, hence the unusual shift order.
struct MeshVertex {
    f32 x = 0.0f;
    f32 y = 0.0f;
    f32 z = 0.0f;
    u32 abgr = 0xFFFFFFFFu;
};
static_assert(sizeof(MeshVertex) == 16, "vertex must stay tightly packed");

// Packs RGBA floats into the ABGR word the shader reads. Values are clamped to
// [0, 1] so a colour outside that range saturates instead of wrapping.
[[nodiscard]] u32 packColor(f32 r, f32 g, f32 b, f32 a = 1.0f);

// Converts a glTF primitive into vertices in draw order.
//
// `tint` is the entity's colour, used for every vertex when the file has no
// COLOR_0 attribute; a file that does carry per-vertex colours keeps them, which
// is what makes a coloured model stay coloured.
[[nodiscard]] std::vector<MeshVertex> buildMeshVertices(const GltfPrimitive& primitive,
                                                        Vec3 tint);

}  // namespace waypoint
