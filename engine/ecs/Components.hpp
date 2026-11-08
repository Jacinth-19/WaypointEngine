// Waypoint Engine — components that can be attached to an entity.
// SPDX-License-Identifier: Apache-2.0
//
// In an ECS ("entity component system") an *entity* is just an id. All of its
// data lives in *components* like the ones below. Systems then loop over every
// entity that has a given component.
#pragma once

#include <string>

#include "core/Types.hpp"
#include "core/Uuid.hpp"

namespace waypoint {

// Where the entity sits in the world. Every entity gets one.
struct TransformComponent {
    Transform transform;
};

// Makes the entity the camera used for rendering.
struct CameraComponent {
    f32 fieldOfViewDegrees = 60.0f;
    f32 nearClip = 0.1f;
    f32 farClip = 1000.0f;
    bool primary = true;
};

// Lets the player fly the entity around with WASD + mouse (editor camera).
struct FlyCameraComponent {
    f32 moveSpeed = 6.0f;         // metres per second
    f32 boostMultiplier = 4.0f;   // multiplies speed while Shift is held
    f32 lookSensitivity = 0.12f;  // degrees per pixel of mouse movement
    f32 yaw = 0.0f;               // degrees, kept so the camera can be re-aimed
    f32 pitch = 0.0f;             // degrees
};

// Sun-like light. `direction` is the direction the light travels.
struct DirectionalLightComponent {
    Vec3 direction{-0.4f, -1.0f, -0.3f};
    Vec3 color{1.0f, 0.98f, 0.94f};
    f32 intensity = 1.0f;
    bool castShadows = true;
};

// Draws a mesh. `mesh` is either "builtin:cube" / "builtin:grid" or a path to a
// glTF file relative to the project (glTF support is the next milestone).
struct MeshRendererComponent {
    std::string mesh = "builtin:cube";
    Vec3 color{0.85f, 0.55f, 0.25f};
    bool castShadows = true;
};

// Gives the entity a box-shaped physics body.
struct RigidBodyComponent {
    enum class Motion {
        Static,    // never moves; other bodies collide with it
        Dynamic,   // moved by gravity and collisions
        Kinematic  // moved by script/animation, pushes dynamic bodies
    };

    Motion motion = Motion::Dynamic;
    Vec3 halfExtents{0.5f, 0.5f, 0.5f};
    f32 mass = 1.0f;
    bool useGravity = true;
};

// Runs a Lua script for this entity.
struct ScriptComponent {
    std::string path;  // relative to the project folder, e.g. "scripts/main.lua"
};

// ---------------------------------------------------------------------------
// Bookkeeping components added automatically by World::createEntity().
// ---------------------------------------------------------------------------

// Stable handle: stays the same for the entity's whole life, unlike the
// internal storage index which can be reused after the entity is destroyed.
struct HandleComponent {
    u64 value = 0;
};

// Human-readable name, used by the editor scene tree.
struct NameComponent {
    std::string value;
};

// Identity that is preserved when a scene is saved and loaded again.
struct UuidComponent {
    Uuid id;
};

}  // namespace waypoint
