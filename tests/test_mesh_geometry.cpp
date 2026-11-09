// Tests for the glTF-to-vertex conversion.
// SPDX-License-Identifier: Apache-2.0
//
// The renderer uploads whatever buildMeshVertices() returns, so these checks are
// the only place a file's geometry can be verified without a GPU: same count,
// same positions, and colours that come from the file when it has them and from
// the entity's tint when it does not.
#include "TestDoctest.hpp"

#include <cstdint>
#include <vector>

#include "assets/GltfLoader.hpp"
#include "core/Types.hpp"
#include "render/MeshGeometry.hpp"

using namespace waypoint;

namespace {

GltfPrimitive triangle() {
    GltfPrimitive primitive;
    primitive.meshName = "Triangle";
    primitive.positions = {Vec3(0.0f, 0.0f, 0.0f), Vec3(1.0f, 0.0f, 0.0f), Vec3(0.0f, 1.0f, 0.0f)};
    primitive.indices = {0, 1, 2};
    return primitive;
}

std::uint8_t channel(u32 abgr, int shift) {
    return static_cast<std::uint8_t>((abgr >> shift) & 0xFFu);
}

}  // namespace

TEST_CASE("buildMeshVertices keeps every position and the order") {
    const GltfPrimitive primitive = triangle();
    const std::vector<MeshVertex> vertices = buildMeshVertices(primitive, Vec3(1.0f));

    REQUIRE(vertices.size() == primitive.positions.size());
    for (std::size_t i = 0; i < vertices.size(); ++i) {
        INFO("vertex " << i);
        CHECK(vertices[i].x == doctest::Approx(primitive.positions[i].x));
        CHECK(vertices[i].y == doctest::Approx(primitive.positions[i].y));
        CHECK(vertices[i].z == doctest::Approx(primitive.positions[i].z));
    }
}

TEST_CASE("buildMeshVertices tints a mesh that has no colours of its own") {
    const std::vector<MeshVertex> vertices =
        buildMeshVertices(triangle(), Vec3(0.25f, 0.5f, 0.75f));

    REQUIRE(vertices.size() == 3);
    for (const MeshVertex& vertex : vertices) {
        // ABGR: red in the low byte, alpha in the high one.
        CHECK(channel(vertex.abgr, 0) == 64);   // 0.25 * 255
        CHECK(channel(vertex.abgr, 8) == 128);  // 0.5 * 255
        CHECK(channel(vertex.abgr, 16) == 191); // 0.75 * 255
        CHECK(channel(vertex.abgr, 24) == 255); // opaque
    }
}

TEST_CASE("buildMeshVertices prefers the file's own colours") {
    GltfPrimitive primitive = triangle();
    primitive.colors = {Vec3(1.0f, 0.0f, 0.0f), Vec3(0.0f, 1.0f, 0.0f), Vec3(0.0f, 0.0f, 1.0f)};

    const std::vector<MeshVertex> vertices = buildMeshVertices(primitive, Vec3(1.0f));
    REQUIRE(vertices.size() == 3);

    CHECK(channel(vertices[0].abgr, 0) == 255);
    CHECK(channel(vertices[0].abgr, 8) == 0);
    CHECK(channel(vertices[1].abgr, 8) == 255);
    CHECK(channel(vertices[1].abgr, 0) == 0);
    CHECK(channel(vertices[2].abgr, 16) == 255);
    CHECK(channel(vertices[2].abgr, 0) == 0);
}

TEST_CASE("buildMeshVertices ignores a colour buffer of the wrong length") {
    // A file with two colours for three vertices must not shift every colour by
    // one; it falls back to the tint instead.
    GltfPrimitive primitive = triangle();
    primitive.colors = {Vec3(1.0f, 0.0f, 0.0f), Vec3(0.0f, 1.0f, 0.0f)};

    const std::vector<MeshVertex> vertices = buildMeshVertices(primitive, Vec3(0.0f, 0.0f, 1.0f));
    REQUIRE(vertices.size() == 3);
    for (const MeshVertex& vertex : vertices) {
        CHECK(channel(vertex.abgr, 16) == 255);  // blue tint everywhere
        CHECK(channel(vertex.abgr, 0) == 0);
    }
}

TEST_CASE("buildMeshVertices handles an empty primitive") {
    GltfPrimitive primitive;
    CHECK(buildMeshVertices(primitive, Vec3(1.0f)).empty());
}

TEST_CASE("packColor clamps instead of wrapping") {
    // A colour outside [0, 1] must saturate: wrapping would turn 1.5 into 0.5
    // and a bright red into a dark green.
    CHECK(channel(packColor(-1.0f, 2.0f, 0.5f), 0) == 0);
    CHECK(channel(packColor(-1.0f, 2.0f, 0.5f), 8) == 255);
    CHECK(channel(packColor(-1.0f, 2.0f, 0.5f), 16) == 128);
    CHECK(channel(packColor(1.0f, 1.0f, 1.0f), 24) == 255);
}

TEST_CASE("buildMeshVertices carries the file's normals so lighting has something to shade") {
    // Before this, the renderer baked a fake per-face brightness into the cube's
    // colours. Real lighting needs a normal per vertex, so a glTF file's normals
    // have to survive the conversion.
    GltfPrimitive primitive;
    primitive.positions = {Vec3(0.0f, 1.0f, 0.0f), Vec3(0.0f, 1.0f, 1.0f)};
    primitive.normals = {Vec3(0.0f, 1.0f, 0.0f), Vec3(0.0f, 1.0f, 0.0f)};

    const std::vector<MeshVertex> vertices = buildMeshVertices(primitive, Vec3(1.0f));
    REQUIRE(vertices.size() == 2);
    for (const MeshVertex& vertex : vertices) {
        CHECK(vertex.nx == doctest::Approx(0.0f));
        CHECK(vertex.ny == doctest::Approx(1.0f));
        CHECK(vertex.nz == doctest::Approx(0.0f));
    }
}

TEST_CASE("buildMeshVertices gives a normal to a file that has none") {
    // A mesh without normals must still be shaded: pointing them all up is
    // visibly wrong, but a zero normal is NaN in the shader and blacks the mesh
    // out entirely.
    GltfPrimitive primitive;
    primitive.positions = {Vec3(0.0f), Vec3(1.0f, 0.0f, 0.0f)};

    const std::vector<MeshVertex> vertices = buildMeshVertices(primitive, Vec3(1.0f));
    REQUIRE(vertices.size() == 2);
    for (const MeshVertex& vertex : vertices) {
        CHECK(vertex.nx == 0.0f);
        CHECK(vertex.ny == 1.0f);
        CHECK(vertex.nz == 0.0f);
    }
}
