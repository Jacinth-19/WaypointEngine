// Waypoint Engine — the ECS world.
// SPDX-License-Identifier: Apache-2.0
//
// World owns an EnTT registry and hands out *stable handles* (plain 64-bit
// numbers) instead of EnTT's internal entity ids. Handles never change, and a
// destroyed handle is never handed out again, which keeps gameplay code honest.
//
//   EntityHandle crate = world.createEntity("Crate");
//   world.add<MeshRendererComponent>(crate);
//   if (auto* body = world.tryGet<RigidBodyComponent>(crate)) { ... }
#pragma once

#include <cstddef>
#include <optional>
#include <stdexcept>
#include <string>
#include <string_view>
#include <unordered_map>
#include <utility>
#include <vector>

#include <entt/entity/registry.hpp>

#include "core/Types.hpp"
#include "core/Uuid.hpp"
#include "ecs/Components.hpp"

namespace waypoint {

using EntityHandle = u64;

inline constexpr EntityHandle kInvalidEntity = 0;

class World {
public:
    World() = default;
    ~World() = default;

    World(const World&) = delete;
    World& operator=(const World&) = delete;
    World(World&&) noexcept = default;
    World& operator=(World&&) noexcept = default;

    // Creates an entity with a transform, name, generated UUID and handle.
    EntityHandle createEntity(std::string_view name = {});
    EntityHandle createEntity(const Uuid& id, std::string_view name = {});

    bool destroyEntity(EntityHandle handle);
    void clear();

    [[nodiscard]] bool alive(EntityHandle handle) const;
    [[nodiscard]] std::size_t entityCount() const { return m_entities.size(); }
    [[nodiscard]] std::vector<EntityHandle> entities() const;

    [[nodiscard]] std::optional<EntityHandle> findByName(std::string_view name) const;
    [[nodiscard]] std::optional<EntityHandle> findByUuid(const Uuid& id) const;

    [[nodiscard]] entt::registry& registry() { return m_registry; }
    [[nodiscard]] const entt::registry& registry() const { return m_registry; }

    // Component access. All of these are no-ops / null when the handle is dead.
    template <typename T, typename... Args>
    T& add(EntityHandle handle, Args&&... args) {
        entt::entity entity = toEntity(handle);
        if (entity == entt::null) {
            // Returning a reference would require storage we do not have; make
            // the mistake loud instead of corrupting memory.
            throw std::invalid_argument("World::add called with an invalid entity handle");
        }
        return m_registry.emplace_or_replace<T>(entity, std::forward<Args>(args)...);
    }

    template <typename T>
    [[nodiscard]] bool has(EntityHandle handle) const {
        const entt::entity entity = toEntity(handle);
        return entity != entt::null && m_registry.all_of<T>(entity);
    }

    template <typename T>
    T* tryGet(EntityHandle handle) {
        const entt::entity entity = toEntity(handle);
        if (entity == entt::null) {
            return nullptr;
        }
        return m_registry.try_get<T>(entity);
    }

    template <typename T>
    const T* tryGet(EntityHandle handle) const {
        const entt::entity entity = toEntity(handle);
        if (entity == entt::null) {
            return nullptr;
        }
        return m_registry.try_get<T>(entity);
    }

    template <typename T>
    void remove(EntityHandle handle) {
        const entt::entity entity = toEntity(handle);
        if (entity != entt::null) {
            m_registry.remove<T>(entity);
        }
    }

    // Convenience accessors for the two components nearly every system needs.
    [[nodiscard]] Transform* transform(EntityHandle handle);
    [[nodiscard]] const Transform* transform(EntityHandle handle) const;
    [[nodiscard]] std::string* name(EntityHandle handle);
    [[nodiscard]] const std::string* name(EntityHandle handle) const;

    // The handle an entity was given, or kInvalidEntity for entt::null.
    [[nodiscard]] EntityHandle toHandle(entt::entity entity) const;

private:
    [[nodiscard]] entt::entity toEntity(EntityHandle handle) const;

    entt::registry m_registry;
    std::unordered_map<EntityHandle, entt::entity> m_entities;
    std::unordered_map<entt::entity, EntityHandle> m_handles;
    EntityHandle m_nextHandle = 1;
};

}  // namespace waypoint
