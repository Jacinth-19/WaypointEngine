// Waypoint Engine — core scalar and math types.
// SPDX-License-Identifier: Apache-2.0
//
// "Type alias" means a short second name for an existing type. `u32` is a
// 32-bit unsigned integer, `f32` is a 32-bit float, and so on. Beginners can
// ignore the aliases and use the standard names; both work everywhere.
#pragma once

#include <cstddef>
#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/quaternion.hpp>

namespace waypoint {

using u8 = std::uint8_t;
using u16 = std::uint16_t;
using u32 = std::uint32_t;
using u64 = std::uint64_t;
using i8 = std::int8_t;
using i16 = std::int16_t;
using i32 = std::int32_t;
using i64 = std::int64_t;
using f32 = float;
using f64 = double;

using Vec2 = glm::vec2;
using Vec3 = glm::vec3;
using Vec4 = glm::vec4;
using Quat = glm::quat;
using Mat3 = glm::mat3;
using Mat4 = glm::mat4;

// Identity rotation. glm quaternions are laid out (w, x, y, z) in memory,
// which is also the order used by the scene file format.
inline constexpr Quat kIdentityRotation{1.0f, 0.0f, 0.0f, 0.0f};

// Position, rotation and scale of one object in a world.
//
// `scale` is stored as a full Vec3 so non-uniform scaling works, but the
// rotation is always a quaternion: quaternions never "gimbal lock" the way
// Euler angles (yaw/pitch/roll numbers) do.
struct Transform {
    Vec3 position{0.0f, 0.0f, 0.0f};
    Quat rotation{1.0f, 0.0f, 0.0f, 0.0f};
    Vec3 scale{1.0f, 1.0f, 1.0f};

    // Combined "model" matrix: scale first, then rotate, then translate.
    [[nodiscard]] Mat4 matrix() const {
        const Mat4 translation = glm::translate(Mat4(1.0f), position);
        const Mat4 rotationMatrix = glm::mat4_cast(rotation);
        const Mat4 scaleMatrix = glm::scale(Mat4(1.0f), scale);
        return translation * rotationMatrix * scaleMatrix;
    }

    // The direction the object's local +Z axis points at in world space.
    [[nodiscard]] Vec3 forward() const { return rotation * Vec3(0.0f, 0.0f, 1.0f); }
    [[nodiscard]] Vec3 right() const { return rotation * Vec3(1.0f, 0.0f, 0.0f); }
    [[nodiscard]] Vec3 up() const { return rotation * Vec3(0.0f, 1.0f, 0.0f); }
};

}  // namespace waypoint
