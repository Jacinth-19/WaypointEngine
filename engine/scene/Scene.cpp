// Waypoint Engine — scene JSON loading and saving.
// SPDX-License-Identifier: Apache-2.0
#include "scene/Scene.hpp"

#include <algorithm>
#include <array>
#include <cctype>
#include <cmath>
#include <sstream>

#include "core/FileSystem.hpp"
#include "core/Log.hpp"

namespace waypoint {
namespace {

using json = nlohmann::json;

// ---------------------------------------------------------------------------
// Small reading helpers. Every one reports a readable message through `error`
// instead of throwing so a broken scene never crashes the editor.
// ---------------------------------------------------------------------------

void setError(std::string* error, std::string message) {
    if (error != nullptr) {
        *error = std::move(message);
    }
}

bool readNumber(const json& object, const char* key, f32& out, std::string* error,
                const std::string& context) {
    const auto it = object.find(key);
    if (it == object.end()) {
        return true;  // optional: keep the default
    }
    if (!it->is_number()) {
        setError(error, context + ": '" + key + "' must be a number");
        return false;
    }
    out = it->get<f32>();
    return true;
}

bool readBool(const json& object, const char* key, bool& out, std::string* error,
              const std::string& context) {
    const auto it = object.find(key);
    if (it == object.end()) {
        return true;
    }
    if (!it->is_boolean()) {
        setError(error, context + ": '" + key + "' must be true or false");
        return false;
    }
    out = it->get<bool>();
    return true;
}

bool readString(const json& object, const char* key, std::string& out, std::string* error,
                const std::string& context) {
    const auto it = object.find(key);
    if (it == object.end()) {
        return true;
    }
    if (!it->is_string()) {
        setError(error, context + ": '" + key + "' must be a string");
        return false;
    }
    out = it->get<std::string>();
    return true;
}

bool readVec3(const json& object, const char* key, Vec3& out, std::string* error,
              const std::string& context) {
    const auto it = object.find(key);
    if (it == object.end()) {
        return true;
    }
    if (!it->is_array() || it->size() != 3) {
        setError(error, context + ": '" + key + "' must be an array of 3 numbers");
        return false;
    }
    for (std::size_t i = 0; i < 3; ++i) {
        const json& element = (*it)[i];
        if (!element.is_number()) {
            setError(error, context + ": '" + key + "' must contain only numbers");
            return false;
        }
        out[static_cast<glm::length_t>(i)] = element.get<f32>();
    }
    return true;
}

bool readQuat(const json& object, const char* key, Quat& out, std::string* error,
              const std::string& context) {
    const auto it = object.find(key);
    if (it == object.end()) {
        return true;
    }
    if (!it->is_array() || it->size() != 4) {
        setError(error, context + ": '" + key + "' must be an array of 4 numbers [w, x, y, z]");
        return false;
    }
    std::array<f32, 4> values{};
    for (std::size_t i = 0; i < 4; ++i) {
        const json& element = (*it)[i];
        if (!element.is_number()) {
            setError(error, context + ": '" + key + "' must contain only numbers");
            return false;
        }
        values[i] = element.get<f32>();
    }
    Quat rotation{values[0], values[1], values[2], values[3]};
    const f32 length = glm::length(rotation);
    if (!std::isfinite(length) || length < 1e-6f) {
        Log::warn("{}: rotation is not usable, falling back to identity", context);
        rotation = kIdentityRotation;
    } else {
        rotation = glm::normalize(rotation);
    }
    out = rotation;
    return true;
}

std::string lowercase(std::string text) {
    std::transform(text.begin(), text.end(), text.begin(),
                   [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
    return text;
}

bool readTransform(const json& entityJson, Transform& out, std::string* error,
                   const std::string& context) {
    const auto it = entityJson.find("transform");
    if (it == entityJson.end()) {
        return true;  // identity transform
    }
    if (!it->is_object()) {
        setError(error, context + ": 'transform' must be an object");
        return false;
    }
    return readVec3(*it, "position", out.position, error, context + ".transform") &&
           readQuat(*it, "rotation", out.rotation, error, context + ".transform") &&
           readVec3(*it, "scale", out.scale, error, context + ".transform");
}

// Reads one entity object and populates the world. `index` is only used in
// error messages so users can find the broken entry.
bool readEntity(const json& entityJson, World& world, std::size_t index, std::string* error) {
    std::ostringstream contextStream;
    contextStream << "entity[" << index << "]";
    std::string context = contextStream.str();

    if (!entityJson.is_object()) {
        setError(error, context + ": each entity must be a JSON object");
        return false;
    }

    std::string name;
    if (!readString(entityJson, "name", name, error, context)) {
        return false;
    }
    if (name.empty()) {
        name = "Entity " + std::to_string(index);
    }
    context += " ('" + name + "')";

    Uuid id = Uuid::generate();
    if (const auto idIt = entityJson.find("id"); idIt != entityJson.end()) {
        if (!idIt->is_string()) {
            setError(error, context + ": 'id' must be a string");
            return false;
        }
        const auto parsed = Uuid::fromString(idIt->get<std::string>());
        if (!parsed.has_value()) {
            Log::warn("{}: 'id' is not a valid UUID, generating a new one", context);
        } else if (world.findByUuid(*parsed).has_value()) {
            Log::warn("{}: duplicate 'id' {}, generating a new one", context, parsed->toString());
        } else {
            id = *parsed;
        }
    }

    Transform transform;
    if (!readTransform(entityJson, transform, error, context)) {
        return false;
    }

    const EntityHandle handle = world.createEntity(id, name);
    if (auto* transformComponent = world.tryGet<TransformComponent>(handle)) {
        transformComponent->transform = transform;
    }

    const auto componentsIt = entityJson.find("components");
    if (componentsIt == entityJson.end()) {
        return true;
    }
    if (!componentsIt->is_object()) {
        setError(error, context + ": 'components' must be an object");
        return false;
    }
    const json& components = *componentsIt;

    for (auto it = components.begin(); it != components.end(); ++it) {
        const std::string& key = it.key();
        const json& value = it.value();

        if (key == "camera" || key == "flyCamera" || key == "directionalLight" ||
            key == "meshRenderer" || key == "rigidBody") {
            if (!value.is_object()) {
                setError(error, context + ": component '" + key + "' must be an object");
                return false;
            }
        }

        if (key == "camera") {
            CameraComponent camera;
            if (!readNumber(value, "fieldOfView", camera.fieldOfViewDegrees, error, context) ||
                !readNumber(value, "nearClip", camera.nearClip, error, context) ||
                !readNumber(value, "farClip", camera.farClip, error, context) ||
                !readBool(value, "primary", camera.primary, error, context)) {
                return false;
            }
            if (camera.nearClip <= 0.0f || camera.farClip <= camera.nearClip ||
                camera.fieldOfViewDegrees <= 0.0f || camera.fieldOfViewDegrees >= 179.0f) {
                setError(error, context + ": camera clip planes and field of view are out of range");
                return false;
            }
            world.add<CameraComponent>(handle, camera);
        } else if (key == "flyCamera") {
            FlyCameraComponent fly;
            if (!readNumber(value, "moveSpeed", fly.moveSpeed, error, context) ||
                !readNumber(value, "boostMultiplier", fly.boostMultiplier, error, context) ||
                !readNumber(value, "lookSensitivity", fly.lookSensitivity, error, context) ||
                !readNumber(value, "yaw", fly.yaw, error, context) ||
                !readNumber(value, "pitch", fly.pitch, error, context)) {
                return false;
            }
            world.add<FlyCameraComponent>(handle, fly);
        } else if (key == "directionalLight") {
            DirectionalLightComponent light;
            if (!readVec3(value, "direction", light.direction, error, context) ||
                !readVec3(value, "color", light.color, error, context) ||
                !readNumber(value, "intensity", light.intensity, error, context) ||
                !readBool(value, "castShadows", light.castShadows, error, context)) {
                return false;
            }
            if (glm::length(light.direction) < 1e-5f) {
                light.direction = DirectionalLightComponent{}.direction;
            }
            light.direction = glm::normalize(light.direction);
            world.add<DirectionalLightComponent>(handle, light);
        } else if (key == "meshRenderer") {
            MeshRendererComponent mesh;
            if (!readString(value, "mesh", mesh.mesh, error, context) ||
                !readVec3(value, "color", mesh.color, error, context) ||
                !readBool(value, "castShadows", mesh.castShadows, error, context)) {
                return false;
            }
            if (mesh.mesh.empty()) {
                mesh.mesh = "builtin:cube";
            }
            world.add<MeshRendererComponent>(handle, mesh);
        } else if (key == "rigidBody") {
            RigidBodyComponent body;
            std::string motion = "dynamic";
            if (!readString(value, "motion", motion, error, context) ||
                !readVec3(value, "halfExtents", body.halfExtents, error, context) ||
                !readNumber(value, "mass", body.mass, error, context) ||
                !readBool(value, "useGravity", body.useGravity, error, context)) {
                return false;
            }
            const std::string normalized = lowercase(motion);
            if (normalized == "static") {
                body.motion = RigidBodyComponent::Motion::Static;
            } else if (normalized == "dynamic") {
                body.motion = RigidBodyComponent::Motion::Dynamic;
            } else if (normalized == "kinematic") {
                body.motion = RigidBodyComponent::Motion::Kinematic;
            } else {
                setError(error, context + ": rigidBody 'motion' must be static, dynamic or kinematic");
                return false;
            }
            if (body.halfExtents.x <= 0.0f || body.halfExtents.y <= 0.0f ||
                body.halfExtents.z <= 0.0f) {
                setError(error, context + ": rigidBody 'halfExtents' must all be positive");
                return false;
            }
            if (body.mass <= 0.0f) {
                setError(error, context + ": rigidBody 'mass' must be positive");
                return false;
            }
            world.add<RigidBodyComponent>(handle, body);
        } else if (key == "script") {
            ScriptComponent script;
            if (!value.is_object()) {
                setError(error, context + ": component 'script' must be an object");
                return false;
            }
            if (!readString(value, "path", script.path, error, context)) {
                return false;
            }
            if (script.path.empty()) {
                setError(error, context + ": script 'path' cannot be empty");
                return false;
            }
            world.add<ScriptComponent>(handle, script);
        } else {
            Log::warn("{}: ignoring unknown component '{}'", context, key);
        }
    }

    return true;
}

json vec3ToJson(const Vec3& value) {
    return json::array({value.x, value.y, value.z});
}

json quatToJson(const Quat& value) {
    return json::array({value.w, value.x, value.y, value.z});
}

const char* motionToString(RigidBodyComponent::Motion motion) {
    switch (motion) {
        case RigidBodyComponent::Motion::Static: return "static";
        case RigidBodyComponent::Motion::Dynamic: return "dynamic";
        case RigidBodyComponent::Motion::Kinematic: return "kinematic";
    }
    return "dynamic";
}

json entityToJson(EntityHandle handle, const World& world) {
    json entity = json::object();
    entity["id"] = world.tryGet<UuidComponent>(handle)->id.toString();

    if (const auto* name = world.name(handle)) {
        entity["name"] = *name;
    }

    if (const auto* transformComponent = world.tryGet<TransformComponent>(handle)) {
        const Transform& transform = transformComponent->transform;
        entity["transform"] = {
            {"position", vec3ToJson(transform.position)},
            {"rotation", quatToJson(transform.rotation)},
            {"scale", vec3ToJson(transform.scale)},
        };
    }

    json components = json::object();
    if (const auto* camera = world.tryGet<CameraComponent>(handle)) {
        components["camera"] = {
            {"fieldOfView", camera->fieldOfViewDegrees},
            {"nearClip", camera->nearClip},
            {"farClip", camera->farClip},
            {"primary", camera->primary},
        };
    }
    if (const auto* fly = world.tryGet<FlyCameraComponent>(handle)) {
        components["flyCamera"] = {
            {"moveSpeed", fly->moveSpeed},
            {"boostMultiplier", fly->boostMultiplier},
            {"lookSensitivity", fly->lookSensitivity},
            {"yaw", fly->yaw},
            {"pitch", fly->pitch},
        };
    }
    if (const auto* light = world.tryGet<DirectionalLightComponent>(handle)) {
        components["directionalLight"] = {
            {"direction", vec3ToJson(light->direction)},
            {"color", vec3ToJson(light->color)},
            {"intensity", light->intensity},
            {"castShadows", light->castShadows},
        };
    }
    if (const auto* mesh = world.tryGet<MeshRendererComponent>(handle)) {
        components["meshRenderer"] = {
            {"mesh", mesh->mesh},
            {"color", vec3ToJson(mesh->color)},
            {"castShadows", mesh->castShadows},
        };
    }
    if (const auto* body = world.tryGet<RigidBodyComponent>(handle)) {
        components["rigidBody"] = {
            {"motion", motionToString(body->motion)},
            {"halfExtents", vec3ToJson(body->halfExtents)},
            {"mass", body->mass},
            {"useGravity", body->useGravity},
        };
    }
    if (const auto* script = world.tryGet<ScriptComponent>(handle)) {
        components["script"] = {{"path", script->path}};
    }

    if (!components.empty()) {
        entity["components"] = std::move(components);
    }
    return entity;
}

}  // namespace

Scene::Scene(std::string name) : m_name(std::move(name)) {}

std::optional<Scene> Scene::loadFromFile(const std::filesystem::path& path, std::string* error) {
    const auto text = FileSystem::readTextFile(path);
    if (!text.has_value()) {
        setError(error, "could not read scene file: " + path.generic_string());
        return std::nullopt;
    }

    json parsed;
    try {
        parsed = json::parse(*text);
    } catch (const json::exception& exception) {
        setError(error, "invalid JSON in " + path.generic_string() + ": " + exception.what());
        return std::nullopt;
    }

    auto scene = fromJson(parsed, error);
    if (!scene.has_value()) {
        return std::nullopt;
    }
    scene->setSourcePath(path);
    return scene;
}

std::optional<Scene> Scene::fromJson(const json& jsonValue, std::string* error) {
    if (!jsonValue.is_object()) {
        setError(error, "scene file must contain a JSON object");
        return std::nullopt;
    }

    Scene scene;
    const auto nameIt = jsonValue.find("name");
    if (nameIt != jsonValue.end()) {
        if (!nameIt->is_string()) {
            setError(error, "scene 'name' must be a string");
            return std::nullopt;
        }
        scene.m_name = nameIt->get<std::string>();
    }

    const auto entitiesIt = jsonValue.find("entities");
    if (entitiesIt != jsonValue.end()) {
        if (!entitiesIt->is_array()) {
            setError(error, "scene 'entities' must be an array");
            return std::nullopt;
        }
        std::size_t index = 0;
        for (const json& entityJson : *entitiesIt) {
            if (!readEntity(entityJson, scene.m_world, index, error)) {
                return std::nullopt;
            }
            ++index;
        }
    }

    for (const char* key : {"engineVersion", "version"}) {
        if (const auto it = jsonValue.find(key); it != jsonValue.end() && it->is_string()) {
            Log::debug("Scene '{}' was saved with engine version {}", scene.m_name,
                       it->get<std::string>());
        }
    }

    return scene;
}

bool Scene::saveToFile(const std::filesystem::path& path, std::string* error) const {
    const std::string text = toJson().dump(2) + "\n";
    if (!FileSystem::writeTextFile(path, text, error)) {
        return false;
    }
    return true;
}

json Scene::toJson() const {
    json root = json::object();
    root["name"] = m_name;
    root["engineVersion"] = "0.1.0";

    json entities = json::array();
    for (const EntityHandle handle : m_world.entities()) {
        entities.push_back(entityToJson(handle, m_world));
    }
    root["entities"] = std::move(entities);
    return root;
}

std::filesystem::path Scene::sourceDirectory() const {
    return FileSystem::parentDirectory(m_sourcePath);
}

EntityHandle Scene::createEntity(std::string_view name) {
    return m_world.createEntity(name);
}

void Scene::clear() {
    m_world.clear();
}

}  // namespace waypoint
