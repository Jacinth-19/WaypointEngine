// Waypoint Engine — rigid body physics implementation.
// SPDX-License-Identifier: Apache-2.0
//
// Jolt 5.3 notes (these cost real debugging time):
//   * JPH_SUPPRESS_WARNINGS must be defined before including Jolt headers.
//   * A Factory must be installed before any shape/body is created, and
//     JPH::UnregisterTypes() must run before the Factory is deleted.
//   * PhysicsSystem::Init takes the layer interfaces; Jolt does not own them, so
//     they live here and outlive the system.
#include "physics/JoltPhysicsWorld.hpp"

#include <algorithm>
#include <cstdarg>
#include <cstdio>
#include <thread>

// Jolt defines JPH_SUPPRESS_WARNINGS itself once Core.h is in; defining it here
// first only produces a redefinition warning.
#include <Jolt/Jolt.h>

#include <Jolt/Core/Factory.h>
#include <Jolt/Core/JobSystemThreadPool.h>
#include <Jolt/Core/TempAllocator.h>
#include <Jolt/Physics/Body/BodyCreationSettings.h>
#include <Jolt/Physics/Body/BodyInterface.h>
#include <Jolt/Physics/Collision/Shape/BoxShape.h>
#include <Jolt/Physics/PhysicsSettings.h>
#include <Jolt/Physics/PhysicsSystem.h>
#include <Jolt/RegisterTypes.h>

#include "core/Log.hpp"

namespace waypoint {
namespace {

// Two collision layers are enough: scenery that never moves, and everything
// that does. Adding more (player, trigger, debris...) is a one-line change.
namespace Layers {
constexpr JPH::ObjectLayer kNonMoving = 0;
constexpr JPH::ObjectLayer kMoving = 1;
// Only read from JPH_ASSERT, which compiles to nothing in release builds.
[[maybe_unused]] constexpr JPH::ObjectLayer kCount = 2;
}  // namespace Layers

namespace BroadPhaseLayers {
constexpr JPH::BroadPhaseLayer kNonMoving(0);
constexpr JPH::BroadPhaseLayer kMoving(1);
constexpr JPH::uint kCount = 2;
}  // namespace BroadPhaseLayers

class BroadPhaseLayerInterfaceImpl final : public JPH::BroadPhaseLayerInterface {
public:
    JPH::uint GetNumBroadPhaseLayers() const override { return BroadPhaseLayers::kCount; }

    JPH::BroadPhaseLayer GetBroadPhaseLayer(JPH::ObjectLayer layer) const override {
        JPH_ASSERT(layer < Layers::kCount);
        return layer == Layers::kNonMoving ? BroadPhaseLayers::kNonMoving
                                           : BroadPhaseLayers::kMoving;
    }

#if defined(JPH_EXTERNAL_PROFILE) || defined(JPH_PROFILE_ENABLED)
    const char* GetBroadPhaseLayerName(JPH::BroadPhaseLayer layer) const override {
        return layer == BroadPhaseLayers::kNonMoving ? "NonMoving" : "Moving";
    }
#endif
};

class ObjectVsBroadPhaseLayerFilterImpl final : public JPH::ObjectVsBroadPhaseLayerFilter {
public:
    bool ShouldCollide(JPH::ObjectLayer layer1, JPH::BroadPhaseLayer layer2) const override {
        if (layer1 == Layers::kNonMoving) {
            return layer2 == BroadPhaseLayers::kMoving;  // static vs moving only
        }
        return true;  // moving collides with everything
    }
};

class ObjectLayerPairFilterImpl final : public JPH::ObjectLayerPairFilter {
public:
    bool ShouldCollide(JPH::ObjectLayer layer1, JPH::ObjectLayer layer2) const override {
        if (layer1 == Layers::kNonMoving && layer2 == Layers::kNonMoving) {
            return false;  // static scenery never needs to collide with itself
        }
        return true;
    }
};

// Jolt reports through function pointers. Route them into the engine log so a
// physics complaint shows up in the same console as everything else.
void joltTrace(const char* format, ...) {
    char buffer[1024];
    va_list args;
    va_start(args, format);
    std::vsnprintf(buffer, sizeof(buffer), format, args);
    va_end(args);
    Log::warn("[Jolt] {}", buffer);
}

// Only Jolt Debug builds have this hook, so the definition follows the same
// condition as the assignment in initialize().
#ifdef JPH_ENABLE_ASSERTS
bool joltAssertFailed(const char* expression, const char* message, const char* file,
                      JPH::uint line) {
    Log::error("[Jolt] assertion failed: {} ({}) at {}:{}", expression,
               message != nullptr ? message : "", file, line);
    return true;  // true = break into the debugger when one is attached
}
#endif

JPH::EMotionType toJoltMotion(RigidBodyComponent::Motion motion) {
    switch (motion) {
        case RigidBodyComponent::Motion::Static: return JPH::EMotionType::Static;
        case RigidBodyComponent::Motion::Kinematic: return JPH::EMotionType::Kinematic;
        case RigidBodyComponent::Motion::Dynamic: break;
    }
    return JPH::EMotionType::Dynamic;
}

JPH::ObjectLayer toObjectLayer(RigidBodyComponent::Motion motion) {
    return motion == RigidBodyComponent::Motion::Static ? Layers::kNonMoving : Layers::kMoving;
}

}  // namespace

struct JoltPhysicsWorld::Impl {
    Settings settings;
    bool initialized = false;
    bool factoryInstalled = false;

    BroadPhaseLayerInterfaceImpl broadPhaseLayerInterface;
    ObjectVsBroadPhaseLayerFilterImpl objectVsBroadPhaseFilter;
    ObjectLayerPairFilterImpl objectLayerPairFilter;

    std::unique_ptr<JPH::TempAllocatorImpl> tempAllocator;
    std::unique_ptr<JPH::JobSystemThreadPool> jobSystem;
    std::unique_ptr<JPH::PhysicsSystem> physicsSystem;

    std::unordered_map<EntityHandle, JPH::BodyID> bodies;
    f32 accumulator = 0.0f;

    ~Impl() { teardown(); }

    void teardown() {
        if (physicsSystem) {
            JPH::BodyInterface& bodyInterface = physicsSystem->GetBodyInterface();
            for (const auto& [entity, id] : bodies) {
                (void)entity;
                if (!id.IsInvalid()) {
                    bodyInterface.RemoveBody(id);
                    bodyInterface.DestroyBody(id);
                }
            }
            bodies.clear();
            physicsSystem.reset();
        }
        jobSystem.reset();
        tempAllocator.reset();
        if (factoryInstalled) {
            JPH::UnregisterTypes();
            delete JPH::Factory::sInstance;
            JPH::Factory::sInstance = nullptr;
            factoryInstalled = false;
        }
        initialized = false;
    }
};

JoltPhysicsWorld::JoltPhysicsWorld() : m_impl(std::make_unique<Impl>()) {}

JoltPhysicsWorld::~JoltPhysicsWorld() = default;

JoltPhysicsWorld::JoltPhysicsWorld(JoltPhysicsWorld&&) noexcept = default;
JoltPhysicsWorld& JoltPhysicsWorld::operator=(JoltPhysicsWorld&&) noexcept = default;

bool JoltPhysicsWorld::initialize(const Settings& settings, std::string* error) {
    if (m_impl->initialized) {
        return true;
    }
    m_impl->settings = settings;
    if (settings.fixedTimeStep <= 0.0f || settings.maxSubSteps < 1) {
        if (error != nullptr) {
            *error = "physics settings are invalid: fixedTimeStep must be positive and maxSubSteps >= 1";
        }
        return false;
    }

    // A process-wide factory; Jolt requires exactly one.
    if (JPH::Factory::sInstance == nullptr) {
        // Must happen before RegisterTypes(), which asserts that an allocator
        // exists. Skipping it crashes inside Jolt's own assertion handler.
        JPH::RegisterDefaultAllocator();
        JPH::Trace = joltTrace;
        // Jolt only declares the hook when its asserts are on (Debug builds);
        // in Release the symbol does not exist at all.
#ifdef JPH_ENABLE_ASSERTS
        JPH::AssertFailed = joltAssertFailed;
#endif
        JPH::Factory::sInstance = new JPH::Factory();
        JPH::RegisterTypes();
        m_impl->factoryInstalled = true;
    }

    m_impl->tempAllocator = std::make_unique<JPH::TempAllocatorImpl>(settings.tempAllocatorSizeBytes);

    int threads = settings.workerThreads;
    if (threads <= 0) {
        const unsigned hardware = std::thread::hardware_concurrency();
        threads = hardware > 1 ? static_cast<int>(hardware) - 1 : 1;
    }
    m_impl->jobSystem = std::make_unique<JPH::JobSystemThreadPool>(
        JPH::cMaxPhysicsJobs, JPH::cMaxPhysicsBarriers, threads);

    m_impl->physicsSystem = std::make_unique<JPH::PhysicsSystem>();
    m_impl->physicsSystem->Init(
        static_cast<JPH::uint>(settings.maxBodies),  // max bodies
        0,                                          // body mutexes (0 = default)
        static_cast<JPH::uint>(settings.maxBodies * 4),  // max body pairs
        static_cast<JPH::uint>(settings.maxBodies * 4),  // max contact constraints
        m_impl->broadPhaseLayerInterface, m_impl->objectVsBroadPhaseFilter,
        m_impl->objectLayerPairFilter);
    m_impl->physicsSystem->SetGravity(
        JPH::Vec3(settings.gravity.x, settings.gravity.y, settings.gravity.z));

    m_impl->initialized = true;
    m_impl->accumulator = 0.0f;
    Log::info("Physics ready (Jolt {}.{}.{}): {} bodies max, fixed step {:.4f}s, {} worker thread(s)",
              JPH_VERSION_MAJOR, JPH_VERSION_MINOR, JPH_VERSION_PATCH, settings.maxBodies,
              static_cast<double>(settings.fixedTimeStep), threads);
    return true;
}

void JoltPhysicsWorld::shutdown() {
    m_impl->teardown();
}

bool JoltPhysicsWorld::initialized() const {
    return m_impl->initialized;
}

const JoltPhysicsWorld::Settings& JoltPhysicsWorld::settings() const {
    return m_impl->settings;
}

bool JoltPhysicsWorld::addBoxBody(EntityHandle entity, const Transform& transform,
                                  const Vec3& halfExtents, RigidBodyComponent::Motion motion,
                                  f32 mass, bool useGravity) {
    if (!m_impl->initialized) {
        Log::error("Physics: addBoxBody called before initialize()");
        return false;
    }
    if (entity == kInvalidEntity) {
        return false;
    }
    if (halfExtents.x <= 0.0f || halfExtents.y <= 0.0f || halfExtents.z <= 0.0f) {
        Log::warn("Physics: entity {} has a non-positive half extent, ignoring", entity);
        return false;
    }

    removeBody(entity);

    const JPH::BoxShapeSettings shapeSettings(
        JPH::Vec3(halfExtents.x, halfExtents.y, halfExtents.z));
    const JPH::ShapeSettings::ShapeResult shapeResult = shapeSettings.Create();
    if (shapeResult.HasError()) {
        Log::error("Physics: could not build box shape: {}", shapeResult.GetError().c_str());
        return false;
    }

    JPH::BodyCreationSettings bodySettings(
        shapeResult.Get(), JPH::RVec3(transform.position.x, transform.position.y,
                                      transform.position.z),
        JPH::Quat(transform.rotation.x, transform.rotation.y, transform.rotation.z,
                  transform.rotation.w),
        toJoltMotion(motion), toObjectLayer(motion));

    if (motion == RigidBodyComponent::Motion::Dynamic) {
        bodySettings.mAllowDynamicOrKinematic = false;
        bodySettings.mOverrideMassProperties = JPH::EOverrideMassProperties::CalculateInertia;
        bodySettings.mMassPropertiesOverride.mMass = std::max(mass, 0.001f);
        bodySettings.mMotionQuality = JPH::EMotionQuality::LinearCast;
    }
    bodySettings.mAllowSleeping = true;
    bodySettings.mGravityFactor = useGravity ? 1.0f : 0.0f;

    JPH::BodyInterface& bodyInterface = m_impl->physicsSystem->GetBodyInterface();
    const JPH::BodyID id = bodyInterface.CreateAndAddBody(
        bodySettings,
        motion == RigidBodyComponent::Motion::Static ? JPH::EActivation::DontActivate
                                                     : JPH::EActivation::Activate);
    if (id.IsInvalid()) {
        Log::error("Physics: Jolt refused to create a body for entity {}", entity);
        return false;
    }

    bodyInterface.SetUserData(id, static_cast<JPH::uint64>(entity));
    m_impl->bodies.emplace(entity, id);
    return true;
}

void JoltPhysicsWorld::removeBody(EntityHandle entity) {
    const auto it = m_impl->bodies.find(entity);
    if (it == m_impl->bodies.end()) {
        return;
    }
    if (m_impl->physicsSystem) {
        JPH::BodyInterface& bodyInterface = m_impl->physicsSystem->GetBodyInterface();
        bodyInterface.RemoveBody(it->second);
        bodyInterface.DestroyBody(it->second);
    }
    m_impl->bodies.erase(it);
}

bool JoltPhysicsWorld::hasBody(EntityHandle entity) const {
    return m_impl->bodies.find(entity) != m_impl->bodies.end();
}

std::size_t JoltPhysicsWorld::bodyCount() const {
    return m_impl->bodies.size();
}

int JoltPhysicsWorld::step(f32 deltaSeconds) {
    if (!m_impl->initialized) {
        return 0;
    }
    const f32 fixedStep = m_impl->settings.fixedTimeStep;

    // Ignore nonsense frames (debugger pauses, first frame, window drag). The
    // `!(x >= 0)` form also rejects NaN.
    if (!(deltaSeconds >= 0.0f) || deltaSeconds > 5.0f) {
        deltaSeconds = 0.0f;
    }
    // Never accumulate more than the substep budget, so a slow frame cannot
    // queue up seconds of simulation.
    deltaSeconds = std::min(deltaSeconds, fixedStep * static_cast<f32>(m_impl->settings.maxSubSteps));
    m_impl->accumulator += deltaSeconds;

    int steps = 0;
    while (m_impl->accumulator >= fixedStep && steps < m_impl->settings.maxSubSteps) {
        m_impl->physicsSystem->Update(fixedStep, 1, m_impl->tempAllocator.get(),
                                      m_impl->jobSystem.get());
        m_impl->accumulator -= fixedStep;
        ++steps;
    }

    if (steps == m_impl->settings.maxSubSteps) {
        // We could not keep up; drop the backlog instead of spiralling.
        m_impl->accumulator = 0.0f;
    }
    return steps;
}

std::size_t JoltPhysicsWorld::syncTransforms(World& world) {
    if (!m_impl->initialized) {
        return 0;
    }
    std::size_t synced = 0;
    JPH::BodyInterface& bodyInterface = m_impl->physicsSystem->GetBodyInterface();

    for (const auto& [entity, id] : m_impl->bodies) {
        Transform* transform = world.transform(entity);
        if (transform == nullptr) {
            continue;
        }
        if (bodyInterface.GetMotionType(id) == JPH::EMotionType::Static) {
            continue;
        }
        const JPH::RVec3 position = bodyInterface.GetCenterOfMassPosition(id);
        const JPH::Quat rotation = bodyInterface.GetRotation(id);
        transform->position = Vec3(static_cast<f32>(position.GetX()),
                                   static_cast<f32>(position.GetY()),
                                   static_cast<f32>(position.GetZ()));
        transform->rotation = glm::normalize(
            Quat(rotation.GetW(), rotation.GetX(), rotation.GetY(), rotation.GetZ()));
        ++synced;
    }
    return synced;
}

void JoltPhysicsWorld::clearBodies() {
    if (!m_impl->physicsSystem) {
        m_impl->bodies.clear();
        return;
    }
    JPH::BodyInterface& bodyInterface = m_impl->physicsSystem->GetBodyInterface();
    for (const auto& [entity, id] : m_impl->bodies) {
        (void)entity;
        bodyInterface.RemoveBody(id);
        bodyInterface.DestroyBody(id);
    }
    m_impl->bodies.clear();
    m_impl->accumulator = 0.0f;
}

}  // namespace waypoint
