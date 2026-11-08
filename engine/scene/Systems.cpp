// Waypoint Engine — fly camera implementation.
// SPDX-License-Identifier: Apache-2.0
#include "scene/Systems.hpp"

#include <algorithm>
#include <cmath>

#include <glm/gtc/quaternion.hpp>

#include "ecs/Components.hpp"

namespace waypoint {
namespace {
constexpr f32 kMaxPitchDegrees = 89.0f;
constexpr f32 kDegreesPerPixelDefault = 0.12f;

f32 wrapDegrees(f32 degrees) {
    // Keep yaw in (-180, 180] so it never grows without bound.
    degrees = std::fmod(degrees + 180.0f, 360.0f);
    if (degrees < 0.0f) {
        degrees += 360.0f;
    }
    return degrees - 180.0f;
}
}  // namespace

Quat FlyCameraSystem::rotationFromYawPitch(f32 yawDegrees, f32 pitchDegrees) {
    const f32 pitch = std::clamp(pitchDegrees, -kMaxPitchDegrees, kMaxPitchDegrees);
    const Quat yawRotation = glm::angleAxis(glm::radians(yawDegrees), Vec3(0.0f, 1.0f, 0.0f));
    const Quat pitchRotation = glm::angleAxis(glm::radians(pitch), Vec3(1.0f, 0.0f, 0.0f));
    return glm::normalize(yawRotation * pitchRotation);
}

Vec3 FlyCameraSystem::movementDirection(const Input& input, const Quat& rotation) {
    // Cameras look down their local -Z axis, so "forward" is -Z.
    const Vec3 forward = rotation * Vec3(0.0f, 0.0f, -1.0f);
    const Vec3 right = rotation * Vec3(1.0f, 0.0f, 0.0f);

    Vec3 direction(0.0f);
    if (input.isKeyDown(Key::W)) direction += forward;
    if (input.isKeyDown(Key::S)) direction -= forward;
    if (input.isKeyDown(Key::D)) direction += right;
    if (input.isKeyDown(Key::A)) direction -= right;
    if (input.isKeyDown(Key::Space)) direction += Vec3(0.0f, 1.0f, 0.0f);
    if (input.isKeyDown(Key::LeftControl) || input.isKeyDown(Key::RightControl)) {
        direction -= Vec3(0.0f, 1.0f, 0.0f);
    }

    const f32 length = glm::length(direction);
    if (length < 1e-6f) {
        return Vec3(0.0f);
    }
    // Normalising means diagonal movement is not faster than straight movement.
    return direction / length;
}

std::size_t FlyCameraSystem::update(World& world, const Input& input, f32 deltaSeconds,
                                    bool mouseLookEnabled) {
    if (deltaSeconds < 0.0f) {
        deltaSeconds = 0.0f;
    }

    std::size_t updated = 0;
    const auto view = world.registry().view<FlyCameraComponent, TransformComponent>();
    for (const entt::entity entity : view) {
        const EntityHandle handle = world.toHandle(entity);
        auto& fly = view.get<FlyCameraComponent>(entity);
        auto& transform = view.get<TransformComponent>(entity).transform;

        const f32 sensitivity =
            fly.lookSensitivity > 0.0f ? fly.lookSensitivity : kDegreesPerPixelDefault;

        if (mouseLookEnabled) {
            const auto [deltaX, deltaY] = input.mouseDelta();
            fly.yaw = wrapDegrees(fly.yaw - deltaX * sensitivity);
            fly.pitch = std::clamp(fly.pitch - deltaY * sensitivity, -kMaxPitchDegrees,
                                   kMaxPitchDegrees);
        }

        transform.rotation = rotationFromYawPitch(fly.yaw, fly.pitch);

        const Vec3 direction = movementDirection(input, transform.rotation);
        if (glm::length(direction) > 0.0f) {
            f32 speed = fly.moveSpeed > 0.0f ? fly.moveSpeed : 6.0f;
            if (input.isKeyDown(Key::LeftShift) || input.isKeyDown(Key::RightShift)) {
                speed *= fly.boostMultiplier > 0.0f ? fly.boostMultiplier : 4.0f;
            }
            transform.position += direction * speed * deltaSeconds;
        }

        (void)handle;
        ++updated;
    }
    return updated;
}

}  // namespace waypoint
