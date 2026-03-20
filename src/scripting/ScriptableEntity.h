#pragma once

#include "../ecs/ECS.h"

namespace engine {

class ScriptableEntity
{
public:
    virtual ~ScriptableEntity() = default;

    virtual void OnCreate() {}
    virtual void OnUpdate(float deltaTime) {}
    virtual void OnDestroy() {}

    template <typename T>
    T& GetComponent()
    {
        return m_Registry->getComponent<T>(m_Entity);
    }

    Entity    m_Entity  = 0;
    Registry* m_Registry = nullptr;
};

} // namespace engine
