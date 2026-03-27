#pragma once

#include <cstdint>
#include <typeindex>
#include <unordered_map>
#include <cassert>
#include <any>

namespace engine {

/// An Entity is just a lightweight numeric handle.
using Entity = std::uint32_t;

/// Minimal, data-oriented ECS registry.
///
/// Component storage is a type-erased map of maps:
///   type_index  →  std::any  (which holds std::unordered_map<Entity, T>)
///
/// This keeps everything header-only, template-friendly, and avoids
/// virtual dispatch or RTTI beyond std::type_index.
class Registry
{
public:
    Registry()  = default;
    ~Registry() = default;

    // ── Entity management ───────────────────────────────────────────────

    Entity createEntity()
    {
        return m_nextEntity++;
    }

    Entity createEntity(Entity entity)
    {
        if (entity >= m_nextEntity)
            m_nextEntity = entity + 1;
        return entity;
    }

    /// Reset the registry to a blank state (removes all entities and components).
    void clear()
    {
        m_pools.clear();
        m_nextEntity = 0;
    }

    // ── Component management ────────────────────────────────────────────

    /// Attach a component of type T to an entity (moves / copies the value).
    template <typename T>
    void addComponent(Entity entity, T component)
    {
        getOrCreateStorage<T>()[entity] = std::move(component);
    }

    /// Get a mutable reference to an entity's component of type T.
    /// Asserts if the entity does not have this component.
    template <typename T>
    T& getComponent(Entity entity)
    {
        auto& storage = getOrCreateStorage<T>();
        auto it = storage.find(entity);
        assert(it != storage.end() && "Entity does not have the requested component");
        return it->second;
    }

    template <typename T>
    bool hasComponent(Entity entity)
    {
        auto& storage = getOrCreateStorage<T>();
        return storage.find(entity) != storage.end();
    }

    /// Return the entire map of (Entity → T) for a given component type.
    /// Useful for iterating over every entity that owns a T.
    template <typename T>
    std::unordered_map<Entity, T>& getView()
    {
        return getOrCreateStorage<T>();
    }

private:
    /// Return (or lazily create) the typed storage map for T.
    template <typename T>
    std::unordered_map<Entity, T>& getOrCreateStorage()
    {
        std::type_index ti(typeid(T));
        auto it = m_pools.find(ti);
        if (it == m_pools.end())
        {
            m_pools[ti] = std::unordered_map<Entity, T>{};
            it = m_pools.find(ti);
        }
        return std::any_cast<std::unordered_map<Entity, T>&>(it->second);
    }

    Entity m_nextEntity = 0;

    /// Type-erased component pools:  type_index → any(unordered_map<Entity,T>)
    std::unordered_map<std::type_index, std::any> m_pools;
};

} // namespace engine
