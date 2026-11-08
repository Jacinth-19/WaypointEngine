// Waypoint Engine — glTF geometry loading.
// SPDX-License-Identifier: Apache-2.0
//
// tinygltf parses the file; this file turns its accessors into flat arrays and
// applies node transforms, because a glTF node is free to place a mesh anywhere
// in its parent's space and ignoring that would silently move every asset.
#include "assets/GltfLoader.hpp"

#include <cmath>
#include <cstdint>
#include <map>
#include <string>

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>

// tinygltf's inline helpers reference both stb headers, so the declarations come
// first. The implementations are compiled exactly once each: stb_image's in
// assets/StbImage.cpp, stb_image_write's in core/ImageWrite.cpp. TINYGLTF's own
// implementation belongs to this file, since this is the only translation unit
// that parses glTF.
#include <stb_image.h>
#include <stb_image_write.h>

#define TINYGLTF_IMPLEMENTATION
#include <tiny_gltf.h>

#include "core/FileSystem.hpp"

namespace waypoint {

namespace {

constexpr std::uint32_t kModeTriangles = 4;

// A read cursor over one accessor: where the elements start, how far apart they
// are, and how wide each one is.
struct AttributeView {
    const std::uint8_t* data = nullptr;
    std::size_t count = 0;
    std::size_t stride = 0;       // bytes between consecutive elements
    std::size_t width = 0;        // components per element (1, 2, 3 or 4)
    std::size_t componentSize = 0;  // bytes per component
    bool normalized = false;      // integer types arrive as normalized floats
    int componentType = 0;
};

// Maps an accessor into the loaded buffer. Returns false when the accessor is
// out of range, uses an unsupported component type, or has an impossible count.
bool viewAccessor(const tinygltf::Model& model, int accessorIndex, AttributeView& out) {
    if (accessorIndex < 0 || static_cast<std::size_t>(accessorIndex) >= model.accessors.size()) {
        return false;
    }
    const tinygltf::Accessor& accessor = model.accessors[static_cast<std::size_t>(accessorIndex)];
    if (accessor.bufferView < 0 ||
        static_cast<std::size_t>(accessor.bufferView) >= model.bufferViews.size()) {
        return false;
    }
    const tinygltf::BufferView& view = model.bufferViews[static_cast<std::size_t>(accessor.bufferView)];
    // The accessor points at a buffer *view*, and the view at the buffer; glTF
    // has no accessor-to-buffer link of its own.
    if (view.buffer < 0 || static_cast<std::size_t>(view.buffer) >= model.buffers.size()) {
        return false;
    }
    const tinygltf::Buffer& buffer = model.buffers[static_cast<std::size_t>(view.buffer)];

    std::size_t componentSize = 0;
    switch (accessor.componentType) {
        case TINYGLTF_COMPONENT_TYPE_BYTE:
        case TINYGLTF_COMPONENT_TYPE_UNSIGNED_BYTE: componentSize = 1; break;
        case TINYGLTF_COMPONENT_TYPE_SHORT:
        case TINYGLTF_COMPONENT_TYPE_UNSIGNED_SHORT: componentSize = 2; break;
        case TINYGLTF_COMPONENT_TYPE_UNSIGNED_INT:
        case TINYGLTF_COMPONENT_TYPE_FLOAT: componentSize = 4; break;
        default: return false;  // double and friends are not worth the trouble
    }

    std::size_t components = 0;
    switch (accessor.type) {
        case TINYGLTF_TYPE_SCALAR: components = 1; break;
        case TINYGLTF_TYPE_VEC2: components = 2; break;
        case TINYGLTF_TYPE_VEC3: components = 3; break;
        case TINYGLTF_TYPE_VEC4: components = 4; break;
        default: return false;  // matrices are not vertex attributes
    }

    const std::size_t elementSize = componentSize * components;
    // A buffer view may hold several attributes; accessor.byteOffset is relative
    // to the view, and the view's own byteOffset is relative to the buffer.
    const std::size_t viewStride =
        view.byteStride == 0 ? elementSize : static_cast<std::size_t>(view.byteStride);
    if (viewStride < elementSize) {
        return false;
    }

    const std::size_t offset =
        static_cast<std::size_t>(view.byteOffset) + static_cast<std::size_t>(accessor.byteOffset);
    if (offset + viewStride * (accessor.count - 1) + elementSize > buffer.data.size()) {
        return false;
    }

    out.data = buffer.data.data() + offset;
    out.count = static_cast<std::size_t>(accessor.count);
    out.stride = viewStride;
    out.width = components;
    out.componentSize = componentSize;
    out.normalized = accessor.normalized;
    out.componentType = accessor.componentType;
    return true;
}

// Reads element `index` of a float-ish attribute as a Vec3. W component is
// dropped, and integer types are divided rather than normalized when the
// accessor says so.
Vec3 readVec3(const AttributeView& view, std::size_t index) {
    const std::uint8_t* element = view.data + index * view.stride;
    float raw[3] = {0.0f, 0.0f, 0.0f};
    for (std::size_t c = 0; c < 3 && c < view.width; ++c) {
        const std::uint8_t* component = element + c * view.componentSize;
        switch (view.componentType) {
            case TINYGLTF_COMPONENT_TYPE_FLOAT:
                raw[c] = *reinterpret_cast<const float*>(component);  // NOLINT
                break;
            case TINYGLTF_COMPONENT_TYPE_UNSIGNED_BYTE: {
                const auto value = static_cast<float>(*component);
                raw[c] = view.normalized ? value / 255.0f : value;
                break;
            }
            case TINYGLTF_COMPONENT_TYPE_UNSIGNED_SHORT: {
                const auto value = static_cast<float>(
                    *reinterpret_cast<const std::uint16_t*>(component));  // NOLINT
                raw[c] = view.normalized ? value / 65535.0f : value;
                break;
            }
            default: break;
        }
    }
    return Vec3(raw[0], raw[1], raw[2]);
}

// Reads a scalar index accessor. Returns false for a component type that cannot
// address vertices.
bool readIndex(const AttributeView& view, std::size_t index, std::uint32_t& out) {
    const std::uint8_t* element = view.data + index * view.stride;
    switch (view.componentType) {
        case TINYGLTF_COMPONENT_TYPE_UNSIGNED_BYTE: out = *element; return true;
        case TINYGLTF_COMPONENT_TYPE_UNSIGNED_SHORT:
            out = *reinterpret_cast<const std::uint16_t*>(element);  // NOLINT
            return true;
        case TINYGLTF_COMPONENT_TYPE_UNSIGNED_INT:
            out = *reinterpret_cast<const std::uint32_t*>(element);  // NOLINT
            return true;
        default: return false;
    }
}

// Composes the node hierarchy, so a mesh's world transform is the product of
// every ancestor's translation, rotation and scale.
Mat4 worldMatrix(const tinygltf::Model& model, int nodeIndex,
                 const std::map<int, int>& parents, std::map<int, Mat4>& cache) {
    if (const auto cached = cache.find(nodeIndex); cached != cache.end()) {
        return cached->second;
    }
    const tinygltf::Node& node = model.nodes[static_cast<std::size_t>(nodeIndex)];

    Mat4 local(1.0f);
    if (node.matrix.size() == 16) {
        // glTF matrices are column-major, which is what glm stores too.
        local = glm::make_mat4(node.matrix.data());
    } else {
        Vec3 translation(0.0f);
        if (node.translation.size() == 3) {
            translation = Vec3(static_cast<float>(node.translation[0]),
                               static_cast<float>(node.translation[1]),
                               static_cast<float>(node.translation[2]));
        }
        Quat rotation(1.0f, 0.0f, 0.0f, 0.0f);
        if (node.rotation.size() == 4) {
            rotation = Quat(static_cast<float>(node.rotation[3]),
                            static_cast<float>(node.rotation[0]),
                            static_cast<float>(node.rotation[1]),
                            static_cast<float>(node.rotation[2]));
        }
        Vec3 scale(1.0f);
        if (node.scale.size() == 3) {
            scale = Vec3(static_cast<float>(node.scale[0]), static_cast<float>(node.scale[1]),
                         static_cast<float>(node.scale[2]));
        }
        local = glm::translate(Mat4(1.0f), translation) * glm::mat4_cast(rotation) *
                glm::scale(Mat4(1.0f), scale);
    }

    const auto parent = parents.find(nodeIndex);
    const Mat4 world = parent == parents.end() || parent->second < 0
                           ? local
                           : worldMatrix(model, parent->second, parents, cache) * local;
    cache.emplace(nodeIndex, world);
    return world;
}

// Which node, if any, references this mesh. A mesh used by several nodes is
// loaded once per node, because each placement is a different instance.
std::vector<int> nodesForMesh(const tinygltf::Model& model) {
    std::vector<int> result(model.meshes.size(), -1);
    for (std::size_t node = 0; node < model.nodes.size(); ++node) {
        const int mesh = model.nodes[node].mesh;
        if (mesh >= 0 && static_cast<std::size_t>(mesh) < result.size() &&
            result[static_cast<std::size_t>(mesh)] < 0) {
            result[static_cast<std::size_t>(mesh)] = static_cast<int>(node);
        }
    }
    return result;
}

}  // namespace

bool loadGltf(const std::filesystem::path& path, GltfDocument& document, std::string* error) {
    auto fail = [&error](std::string message) {
        if (error != nullptr) {
            *error = std::move(message);
        }
        return false;
    };

    if (!FileSystem::isRegularFile(path)) {
        return fail("no such file: " + FileSystem::genericString(path));
    }

    tinygltf::TinyGLTF loader;
    tinygltf::Model model;
    std::string parseError;
    std::string parseWarning;
    const std::string filename = path.string();
    const bool loaded = path.extension() == ".glb"
                            ? loader.LoadBinaryFromFile(&model, &parseError, &parseWarning, filename)
                            : loader.LoadASCIIFromFile(&model, &parseError, &parseWarning, filename);
    if (!loaded) {
        return fail("could not read " + FileSystem::genericString(path) + ": " +
                    (parseError.empty() ? "not a glTF file" : parseError));
    }
    if (!parseWarning.empty()) {
        document.warnings.push_back(parseWarning);
    }

    if (model.meshes.empty()) {
        return fail(FileSystem::genericString(path) + " contains no meshes");
    }

    // Parents, so a node's world transform can walk up to the scene root.
    std::map<int, int> parents;
    for (std::size_t node = 0; node < model.nodes.size(); ++node) {
        for (const int child : model.nodes[node].children) {
            if (child >= 0 && static_cast<std::size_t>(child) < model.nodes.size()) {
                parents[child] = static_cast<int>(node);
            }
        }
    }
    std::map<int, Mat4> matrixCache;
    const std::vector<int> meshNodes = nodesForMesh(model);

    for (std::size_t meshIndex = 0; meshIndex < model.meshes.size(); ++meshIndex) {
        const tinygltf::Mesh& mesh = model.meshes[meshIndex];
        const std::string meshName = mesh.name.empty()
                                         ? "mesh " + std::to_string(meshIndex)
                                         : mesh.name;
        const int nodeIndex = meshNodes[meshIndex];
        // A mesh no node references is still part of the asset; draw it where it
        // was authored rather than dropping it.
        const Mat4 transform =
            nodeIndex < 0 ? Mat4(1.0f) : worldMatrix(model, nodeIndex, parents, matrixCache);
        const Mat3 normalMatrix = glm::mat3(glm::transpose(glm::inverse(transform)));

        for (std::uint32_t primitiveIndex = 0;
             primitiveIndex < static_cast<std::uint32_t>(mesh.primitives.size());
             ++primitiveIndex) {
            const tinygltf::Primitive& primitive = mesh.primitives[primitiveIndex];
            if (primitive.mode >= 0 && primitive.mode != static_cast<int>(kModeTriangles)) {
                continue;  // points, lines and strips are not meshes yet
            }

            const auto positionAttribute = primitive.attributes.find("POSITION");
            if (positionAttribute == primitive.attributes.end()) {
                continue;  // nothing to draw without positions
            }

            AttributeView positions;
            if (!viewAccessor(model, positionAttribute->second, positions)) {
                return fail(meshName + ": the POSITION attribute is not readable");
            }

            GltfPrimitive out;
            out.meshName = meshName;
            out.primitiveIndex = primitiveIndex;
            if (primitive.material >= 0 &&
                static_cast<std::size_t>(primitive.material) < model.materials.size()) {
                out.materialName = model.materials[static_cast<std::size_t>(primitive.material)].name;
            }
            out.positions.reserve(positions.count);
            for (std::size_t i = 0; i < positions.count; ++i) {
                const Vec3 local = readVec3(positions, i);
                out.positions.push_back(Vec3(transform * Vec4(local, 1.0f)));
            }

            if (const auto normalAttribute = primitive.attributes.find("NORMAL");
                normalAttribute != primitive.attributes.end()) {
                AttributeView normals;
                if (viewAccessor(model, normalAttribute->second, normals) &&
                    normals.count == positions.count) {
                    out.normals.reserve(normals.count);
                    for (std::size_t i = 0; i < normals.count; ++i) {
                        Vec3 normal = normalMatrix * readVec3(normals, i);
                        const float length = glm::length(normal);
                        // A zero-length normal would shade the triangle black;
                        // point it up instead, which is wrong but visible.
                        out.normals.push_back(length > 0.0f ? normal / length
                                                            : Vec3(0.0f, 1.0f, 0.0f));
                    }
                }
            }

            if (const auto colorAttribute = primitive.attributes.find("COLOR_0");
                colorAttribute != primitive.attributes.end()) {
                AttributeView colors;
                if (viewAccessor(model, colorAttribute->second, colors) &&
                    colors.count == positions.count) {
                    out.colors.reserve(colors.count);
                    for (std::size_t i = 0; i < colors.count; ++i) {
                        out.colors.push_back(readVec3(colors, i));
                    }
                }
            }

            if (primitive.indices >= 0) {
                AttributeView indices;
                if (!viewAccessor(model, primitive.indices, indices)) {
                    return fail(meshName + ": the index buffer is not readable");
                }
                out.indices.reserve(indices.count);
                for (std::size_t i = 0; i < indices.count; ++i) {
                    std::uint32_t index = 0;
                    if (!readIndex(indices, i, index) || index >= positions.count) {
                        return fail(meshName + ": an index points past the end of the vertices");
                    }
                    out.indices.push_back(index);
                }
            } else {
                // No index buffer: the vertices are already in draw order.
                out.indices.reserve(positions.count);
                for (std::size_t i = 0; i < positions.count; ++i) {
                    out.indices.push_back(static_cast<std::uint32_t>(i));
                }
            }

            if (out.indices.size() % 3 != 0) {
                return fail(meshName + ": " + std::to_string(out.indices.size()) +
                            " indices is not a whole number of triangles");
            }
            document.primitives.push_back(std::move(out));
        }
    }

    if (document.primitives.empty()) {
        return fail(FileSystem::genericString(path) + " has no triangles to draw");
    }
    return true;
}

std::string gltfReference(const std::filesystem::path& path, std::uint32_t primitiveIndex,
                          std::size_t primitiveCount) {
    std::string reference = path.generic_string();
    if (primitiveCount > 1) {
        reference += "#" + std::to_string(primitiveIndex);
    }
    return reference;
}

}  // namespace waypoint
