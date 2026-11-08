// Waypoint Engine — rigid body physics.
// SPDX-License-Identifier: Apache-2.0
//
// A thin, friendly wrapper over Jolt Physics. Only boxes exist for now; that is
// enough for a first playable world and keeps the API honest.
//
// Physics always runs at a fixed 60 Hz, no matter how fast the frame rate is.
// That is what makes simulation repeatable: a 20 FPS frame runs three physics
// steps, a 240 FPS frame runs none or one.
//
//   JoltPhysicsWorld physics;
//   physics.initialize();
//   physics.addBoxBody(entity, transform, halfExtents, Motion::Dynamic, 1.0f, true);
//   physics.step(frameSeconds);
//   physics.syncTransforms(world);
//
// Jolt headers stay out of this file (pimpl), so the rest of the engine does not
// need to know about Jolt at all.
#pragma once

#include <cstddef>
#include <memory>
#include <string>
#include <unordered_map>

#include "core/Types.hpp"
#include "ecs/Components.hpp"
#include "ecs/World.hpp"

namespace waypoint {

// Everything you can tune about the simulation. Defined outside the class so it
// can be used as a default argument.
struct JoltPhysicsSettings {
    Vec3 gravity{0.0f, -9.81f, 0.0f};
    // The simulation step is fixed; `maxSubSteps` caps how much work one
    // slow frame may cause (4 steps = 66 ms of catch-up).
    f32 fixedTimeStep = 1.0f / 60.0f;
    int maxSubSteps = 4;
    int maxBodies = 4096;
    u32 tempAllocatorSizeBytes = 16u * 1024u * 1024u;
    int workerThreads = 0;  // 0 = pick automatically
};
class JoltPhysicsWorld {
public:
    using Settings = JoltPhysicsSettings;



    JoltPhysicsWorld();
    ~JoltPhysicsWorld();

    JoltPhysicsWorld(const JoltPhysicsWorld&) = delete;
    JoltPhysicsWorld& operator=(const JoltPhysicsWorld&) = delete;
    JoltPhysicsWorld(JoltPhysicsWorld&&) noexcept;
    JoltPhysicsWorld& operator=(JoltPhysicsWorld&&) noexcept;

    // Starts Jolt. Safe to call twice; the second call is a no-op.
    [[nodiscard]] bool initialize(const Settings& settings = Settings(),
                                 std::string* error = nullptr);
    void shutdown();

    [[nodiscard]] bool initialized() const;
    [[nodiscard]] const Settings& settings() const;

    // Creates a box body for an entity. Re-adding the same entity replaces its
    // body, which is what the editor does after an inspector edit.
    [[nodiscard]] bool addBoxBody(EntityHandle entity, const Transform& transform,
                                  const Vec3& halfExtents,
                                  RigidBodyComponent::Motion motion = RigidBodyComponent::Motion::Dynamic,
                                  f32 mass = 1.0f, bool useGravity = true);

    void removeBody(EntityHandle entity);
    [[nodiscard]] bool hasBody(EntityHandle entity) const;
    [[nodiscard]] std::size_t bodyCount() const;

    // Advances the simulation by `deltaSeconds` in fixed steps.
    // Returns how many steps actually ran (0 when the frame was very short).
    int step(f32 deltaSeconds);

    // Copies simulated positions and rotations back into TransformComponents.
    // Static bodies are skipped because nothing can move them.
    std::size_t syncTransforms(World& world);

    // Drops every body but keeps Jolt running.
    void clearBodies();

private:
    struct Impl;
    std::unique_ptr<Impl> m_impl;
};

}  // namespace waypoint
