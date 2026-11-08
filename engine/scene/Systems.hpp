// Waypoint Engine — small systems that run every frame.
// SPDX-License-Identifier: Apache-2.0
//
// A "system" is a function that loops over entities with certain components and
// updates them. These helpers are pure maths where possible so they can be
// tested without a window.
#pragma once

#include <cstddef>
#include <utility>

#include "core/Types.hpp"
#include "ecs/World.hpp"
#include "platform/Input.hpp"

namespace waypoint {

// A camera that can be flown with WASD + mouse, the same way the editor camera
// and the sample player move.
//
// Engine convention: an object's front is its local +Z axis, but a *camera*
// looks along its local -Z axis. That is the standard right-handed OpenGL
// convention and keeps "+X is on the right of the screen" true.
class FlyCameraSystem {
public:
    // Aims a camera. Yaw rotates around the world up axis, pitch around the
    // camera's local right axis; pitch is clamped to +-89 degrees so the camera
    // never flips over.
    [[nodiscard]] static Quat rotationFromYawPitch(f32 yawDegrees, f32 pitchDegrees);

    // Unit direction from held movement keys, in world space.
    [[nodiscard]] static Vec3 movementDirection(const Input& input, const Quat& rotation);

    // Updates every entity that has both FlyCameraComponent and TransformComponent.
    // `mouseLookEnabled` is false when the mouse is free (so the editor can use it).
    // Returns the number of cameras that were updated.
    static std::size_t update(World& world, const Input& input, f32 deltaSeconds,
                              bool mouseLookEnabled);
};

}  // namespace waypoint
