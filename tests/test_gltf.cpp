// Tests for the glTF loader.
// SPDX-License-Identifier: Apache-2.0
//
// Geometry only: positions, normals, indices, and the node transform that places
// them. No GPU, no textures - which is exactly why this can be tested on every
// platform in the headless suite, against a real .glb in samples/.
#include "TestDoctest.hpp"

#include <cmath>
#include <filesystem>
#include <string>
#include <vector>

#include <glm/glm.hpp>
#include <glm/gtc/type_ptr.hpp>

#include <nlohmann/json.hpp>

#include "assets/GltfLoader.hpp"
#include "core/FileSystem.hpp"
#include "core/Types.hpp"
#include "TestSupport.hpp"

#ifndef WAYPOINT_SOURCE_DIR
#error "WAYPOINT_SOURCE_DIR must point at the repository root"
#endif

using namespace waypoint;
using waypoint::test::TempDir;

namespace {

std::filesystem::path sampleCube() {
    return std::filesystem::path(WAYPOINT_SOURCE_DIR) / "samples" / "hello_cube" / "assets" /
           "cube.glb";
}

}  // namespace

TEST_CASE("loadGltf reads the sample cube") {
    GltfDocument document;
    std::string error;
    REQUIRE(loadGltf(sampleCube(), document, &error));

    REQUIRE(document.primitives.size() == 1);
    const GltfPrimitive& cube = document.primitives.front();

    CHECK(cube.meshName == "Cube");
    CHECK(cube.primitiveIndex == 0);
    CHECK(cube.positions.size() == 24);
    CHECK(cube.normals.size() == 24);
    CHECK(cube.indices.size() == 36);

    // A unit cube centred on the origin: every coordinate is within half a unit.
    for (const Vec3& position : cube.positions) {
        for (int axis = 0; axis < 3; ++axis) {
            CHECK(std::abs(position[axis]) <= 0.5f + 1e-5f);
        }
    }

    // Normals are unit length and axis-aligned, and each face's four vertices
    // share one.
    for (const Vec3& normal : cube.normals) {
        CHECK(glm::length(normal) == doctest::Approx(1.0f));
        const float longest = std::max({std::abs(normal.x), std::abs(normal.y), std::abs(normal.z)});
        CHECK(longest == doctest::Approx(1.0f));
    }

    // Every index addresses a real vertex, and the triangles are wound so that
    // (v1 - v0) x (v2 - v0) points the same way as the normal.
    for (std::uint32_t index : cube.indices) {
        CHECK(index < cube.positions.size());
    }
    for (std::size_t triangle = 0; triangle + 2 < cube.indices.size(); triangle += 3) {
        const Vec3& a = cube.positions[cube.indices[triangle + 0]];
        const Vec3& b = cube.positions[cube.indices[triangle + 1]];
        const Vec3& c = cube.positions[cube.indices[triangle + 2]];
        const Vec3 faceNormal = glm::normalize(glm::cross(b - a, c - a));
        const Vec3& vertexNormal = cube.normals[cube.indices[triangle]];
        CHECK(glm::dot(faceNormal, vertexNormal) == doctest::Approx(1.0f).epsilon(0.01));
    }
}

TEST_CASE("loadGltf applies the node transform") {
    // The same cube, placed by a node that translates, rotates and scales it.
    // Ignoring node transforms would silently move every asset in a scene.
    const TempDir dir;
    const auto path = dir.path() / "moved.glb";

    const std::vector<Vec3> positions = {Vec3(0.0f), Vec3(1.0f, 0.0f, 0.0f), Vec3(0.0f, 1.0f, 0.0f)};
    const std::vector<Vec3> normals = {Vec3(0.0f, 0.0f, 1.0f), Vec3(0.0f, 0.0f, 1.0f),
                                       Vec3(0.0f, 0.0f, 1.0f)};
    const std::vector<std::uint16_t> indices = {0, 1, 2};

    std::string bytes;
    const auto append = [&bytes](const void* data, std::size_t size) {
        bytes.append(static_cast<const char*>(data), size);
    };
    for (const Vec3& position : positions) {
        append(glm::value_ptr(position), sizeof(Vec3));
    }
    for (const Vec3& normal : normals) {
        append(glm::value_ptr(normal), sizeof(Vec3));
    }
    for (std::uint16_t index : indices) {
        append(&index, sizeof(index));
    }

    const std::size_t positionBytes = positions.size() * sizeof(Vec3);
    const std::size_t normalBytes = normals.size() * sizeof(Vec3);
    const auto view = [](std::size_t offset, std::size_t length, int target) {
        return nlohmann::json{{"buffer", 0},
                              {"byteOffset", offset},
                              {"byteLength", length},
                              {"target", target}}
            .dump();
    };
    const nlohmann::json gltf = {
        {"asset", {{"version", "2.0"}}},
        {"scene", 0},
        {"scenes", nlohmann::json::array({{{"nodes", nlohmann::json::array({0})}}})},
        {"nodes",
         nlohmann::json::array({{{"mesh", 0},
                                 {"name", "Placed"},
                                 {"translation", nlohmann::json::array({10.0, 0.0, 0.0})},
                                 {"rotation", nlohmann::json::array({0.0, 0.0, 0.0, 1.0})},
                                 {"scale", nlohmann::json::array({2.0, 2.0, 2.0})}}})},
        {"meshes",
         nlohmann::json::array({{{"name", "Triangle"},
                                 {"primitives", nlohmann::json::array({{{"attributes",
                                                                            {{"POSITION", 0},
                                                                             {"NORMAL", 1}}},
                                                                        {"indices", 2},
                                                                        {"mode", 4}}})}}})},
        {"accessors",
         nlohmann::json::array(
             {{{"bufferView", 0}, {"componentType", 5126}, {"count", positions.size()}, {"type", "VEC3"}},
              {{"bufferView", 1}, {"componentType", 5126}, {"count", normals.size()}, {"type", "VEC3"}},
              {{"bufferView", 2}, {"componentType", 5123}, {"count", indices.size()}, {"type", "SCALAR"}}})},
        {"bufferViews",
         nlohmann::json::array({nlohmann::json::parse(view(0, positionBytes, 34962)),
                                nlohmann::json::parse(view(positionBytes, normalBytes, 34962)),
                                nlohmann::json::parse(
                                    view(positionBytes + normalBytes, indices.size() * 2, 34963))})},
        {"buffers", nlohmann::json::array({{{"byteLength", bytes.size()}}})},
    };

    const std::string json = gltf.dump();
    std::string paddedJson = json;
    paddedJson.append((4 - paddedJson.size() % 4) % 4, ' ');
    std::string paddedBin = bytes;
    paddedBin.append((4 - paddedBin.size() % 4) % 4, '\0');

    std::string glb;
    const auto append32 = [&glb](std::uint32_t value) {
        glb.append(reinterpret_cast<const char*>(&value), sizeof(value));
    };
    append32(0x46546C67);
    append32(2);
    append32(static_cast<std::uint32_t>(12 + 8 + paddedJson.size() + 8 + paddedBin.size()));
    append32(static_cast<std::uint32_t>(paddedJson.size()));
    append32(0x4E4F534A);
    glb += paddedJson;
    append32(static_cast<std::uint32_t>(paddedBin.size()));
    append32(0x004E4942);
    glb += paddedBin;

    REQUIRE(waypoint::FileSystem::writeTextFile(path, glb, nullptr));

    GltfDocument document;
    std::string error;
    REQUIRE(loadGltf(path, document, &error));
    REQUIRE(document.primitives.size() == 1);

    const GltfPrimitive& triangle = document.primitives.front();
    REQUIRE(triangle.positions.size() == 3);
    // scale 2, then translate +10 on X.
    const std::vector<Vec3> expected = {Vec3(10.0f, 0.0f, 0.0f), Vec3(12.0f, 0.0f, 0.0f),
                                        Vec3(10.0f, 2.0f, 0.0f)};
    for (std::size_t i = 0; i < expected.size(); ++i) {
        INFO("vertex " << i);
        for (int axis = 0; axis < 3; ++axis) {
            CHECK(triangle.positions[i][axis] == doctest::Approx(expected[i][axis]));
        }
    }
    // Normals survive the transform, renormalised.
    CHECK(glm::length(triangle.normals[0]) == doctest::Approx(1.0f));
}

TEST_CASE("loadGltf reports a missing file instead of throwing") {
    const TempDir dir;
    GltfDocument document;
    std::string error;
    CHECK_FALSE(loadGltf(dir.path() / "nope.glb", document, &error));
    CHECK(error.find("no such file") != std::string::npos);
}

TEST_CASE("loadGltf reports a file that is not glTF") {
    const TempDir dir;
    const auto path = dir.path() / "notgltf.glb";
    REQUIRE(waypoint::FileSystem::writeTextFile(path, "this is not a glb", nullptr));

    GltfDocument document;
    std::string error;
    CHECK_FALSE(loadGltf(path, document, &error));
    CHECK(error.find("could not read") != std::string::npos);
}

TEST_CASE("loadGltf reports a file with no triangles") {
    // A valid glTF with a mesh, but a primitive that draws points.
    const TempDir dir;
    const auto path = dir.path() / "points.glb";

    const std::vector<Vec3> positions = {Vec3(0.0f), Vec3(1.0f, 0.0f, 0.0f), Vec3(0.0f, 1.0f, 0.0f)};
    std::string bytes;
    for (const Vec3& position : positions) {
        bytes.append(reinterpret_cast<const char*>(glm::value_ptr(position)), sizeof(Vec3));
    }
    bytes.append((4 - bytes.size() % 4) % 4, '\0');

    const nlohmann::json gltf = {
        {"asset", {{"version", "2.0"}}},
        {"scene", 0},
        {"scenes", nlohmann::json::array({{{"nodes", nlohmann::json::array({0})}}})},
        {"nodes", nlohmann::json::array({{{"mesh", 0}}})},
        {"meshes",
         nlohmann::json::array({{{"primitives", nlohmann::json::array({{{"attributes", {{"POSITION", 0}}},
                                                                        {"mode", 0}}})}}})},
        {"accessors",
         nlohmann::json::array({{{"bufferView", 0},
                                 {"componentType", 5126},
                                 {"count", positions.size()},
                                 {"type", "VEC3"}}})},
        {"bufferViews",
         nlohmann::json::array({{{"buffer", 0}, {"byteLength", bytes.size()}}})},
        {"buffers", nlohmann::json::array({{{"byteLength", bytes.size()}}})},
    };
    std::string json = gltf.dump();
    json.append((4 - json.size() % 4) % 4, ' ');

    std::string glb;
    const auto append32 = [&glb](std::uint32_t value) {
        glb.append(reinterpret_cast<const char*>(&value), sizeof(value));
    };
    append32(0x46546C67);
    append32(2);
    append32(static_cast<std::uint32_t>(12 + 8 + json.size() + 8 + bytes.size()));
    append32(static_cast<std::uint32_t>(json.size()));
    append32(0x4E4F534A);
    glb += json;
    append32(static_cast<std::uint32_t>(bytes.size()));
    append32(0x004E4942);
    glb += bytes;
    REQUIRE(waypoint::FileSystem::writeTextFile(path, glb, nullptr));

    GltfDocument document;
    std::string error;
    CHECK_FALSE(loadGltf(path, document, &error));
    CHECK(error.find("no triangles") != std::string::npos);
}

TEST_CASE("gltfReference adds a primitive suffix only when it is needed") {
    CHECK(gltfReference("assets/cube.glb", 0, 1) == "assets/cube.glb");
    CHECK(gltfReference("assets/cube.glb", 2, 3) == "assets/cube.glb#2");
}
