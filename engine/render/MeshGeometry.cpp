// Waypoint Engine — mesh geometry.
// SPDX-License-Identifier: Apache-2.0
#include "render/MeshGeometry.hpp"

#include <algorithm>

namespace waypoint {

u32 packColor(f32 r, f32 g, f32 b, f32 a) {
    const auto toByte = [](f32 value) {
        const f32 clamped = value < 0.0f ? 0.0f : (value > 1.0f ? 1.0f : value);
        return static_cast<u32>(clamped * 255.0f + 0.5f);
    };
    return (toByte(a) << 24) | (toByte(b) << 16) | (toByte(g) << 8) | toByte(r);
}

std::vector<MeshVertex> buildMeshVertices(const GltfPrimitive& primitive, Vec3 tint) {
    std::vector<MeshVertex> vertices;
    vertices.reserve(primitive.positions.size());

    const bool fileHasColors = primitive.colors.size() == primitive.positions.size();
    const bool fileHasNormals = primitive.normals.size() == primitive.positions.size();
    for (std::size_t i = 0; i < primitive.positions.size(); ++i) {
        MeshVertex vertex;
        vertex.x = primitive.positions[i].x;
        vertex.y = primitive.positions[i].y;
        vertex.z = primitive.positions[i].z;
        if (fileHasNormals) {
            vertex.nx = primitive.normals[i].x;
            vertex.ny = primitive.normals[i].y;
            vertex.nz = primitive.normals[i].z;
        }
        if (fileHasColors) {
            const Vec3& color = primitive.colors[i];
            vertex.abgr = packColor(color.x, color.y, color.z, 1.0f);
        } else {
            vertex.abgr = packColor(tint.x, tint.y, tint.z, 1.0f);
        }
        vertices.push_back(vertex);
    }
    return vertices;
}

}  // namespace waypoint
