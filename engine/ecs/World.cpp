// Waypoint Engine — the ECS world implementation.
// SPDX-License-Identifier: Apache-2.0
#include "ecs/World.hpp"

#include <algorithm>
#include <iterator>
#include <stdexcept>

namespace waypoint {

EntityHandle World::createEntity(std::string_view name) {
    return createEntity(Uuid::generate(), name);
}

EntityHandle World::createEntity(const Uuid& id, std::string_view name) {
    const EntityHandle handle = m_nextHandle++;
    const entt::entity entity = m_registry.create();

    m_entities.emplace(handle, entity);
    m_handles.emplace(entity, handle);

    m_registry.emplace<HandleComponent>(entity, HandleComponent{handle});
    m_registry.emplace<TransformComponent>(entity);
    m_registry.emplace<NameComponent>(entity, NameComponent{std::string(name)});
    m_registry.emplace<UuidComponent>(entity, UuidComponent{id});

    return handle;
}

bool World::destroyEntity(EntityHandle handle) {
    const auto it = m_entities.find(handle);
    if (it == m_entities.end()) {
        return false;
    }
    const entt::entity entity = it->second;
    m_handles.erase(entity);
    m_entities.erase(it);
    m_registry.destroy(entity);
    return true;
}

void World::clear() {
    m_registry.clear();
    m_entities.clear();
    m_handles.clear();
    m_nextHandle = 1;
}

bool World::alive(EntityHandle handle) const {
    return m_entities.find(handle) != m_entities.end();
}

std::vector<EntityHandle> World::entities() const {
    std::vector<EntityHandle> result;
    result.reserve(m_entities.size());
    for (const auto& [handle, entity] : m_entities) {
        (void)entity;
        result.push_back(handle);
    }
    // Deterministic order keeps tests and editor listings stable.
    std::sort(result.begin(), result.end());
    return result;
}

std::optional<EntityHandle> World::findByName(std::string_view name) const {
    const auto view = m_registry.view<const NameComponent>();
    for (const entt::entity entity : view) {
        if (view.get<const NameComponent>(entity).value == name) {
            return toHandle(entity);
        }
    }
    return std::nullopt;
}

std::optional<EntityHandle> World::findByUuid(const Uuid& id) const {
    const auto view = m_registry.view<const UuidComponent>();
    for (const entt::entity entity : view) {
        if (view.get<const UuidComponent>(entity).id == id) {
            return toHandle(entity);
        }
    }
    return std::nullopt;
}

Transform* World::transform(EntityHandle handle) {
    auto* component = tryGet<TransformComponent>(handle);
    return component != nullptr ? &component->transform : nullptr;
}

const Transform* World::transform(EntityHandle handle) const {
    const auto* component = tryGet<TransformComponent>(handle);
    return component != nullptr ? &component->transform : nullptr;
}

std::string* World::name(EntityHandle handle) {
    auto* component = tryGet<NameComponent>(handle);
    return component != nullptr ? &component->value : nullptr;
}

const std::string* World::name(EntityHandle handle) const {
    const auto* component = tryGet<NameComponent>(handle);
    return component != nullptr ? &component->value : nullptr;
}

EntityHandle World::toHandle(entt::entity entity) const {
    const auto it = m_handles.find(entity);
    return it != m_handles.end() ? it->second : kInvalidEntity;
}

entt::entity World::toEntity(EntityHandle handle) const {
    const auto it = m_entities.find(handle);
    return it != m_entities.end() ? it->second : entt::null;
}

}  // namespace waypoint
