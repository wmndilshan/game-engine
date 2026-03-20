#pragma once

#include <glm/glm.hpp>
#include <reactphysics3d/reactphysics3d.h>

#include "../scripting/ScriptableEntity.h"

namespace engine {

struct TransformComponent
{
    glm::vec3 position{0.0f, 0.0f, 0.0f};
    glm::vec3 rotation{0.0f, 0.0f, 0.0f};
    glm::vec3 scale   {1.0f, 1.0f, 1.0f};
};

struct RigidBodyComponent
{
    reactphysics3d::RigidBody* body = nullptr;
};

struct BoxColliderComponent
{
    reactphysics3d::Collider* collider = nullptr;
};

struct NativeScriptComponent
{
    ScriptableEntity* Instance = nullptr;

    ScriptableEntity* (*InstantiateScript)() = nullptr;
    void (*DestroyScript)(NativeScriptComponent*) = nullptr;

    template <typename T>
    void Bind()
    {
        InstantiateScript = []()
        {
            return static_cast<ScriptableEntity*>(new T());
        };

        DestroyScript = [](NativeScriptComponent* nsc)
        {
            delete nsc->Instance;
            nsc->Instance = nullptr;
        };
    }
};

} // namespace engine
