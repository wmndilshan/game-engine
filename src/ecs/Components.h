#pragma once

#include <glm/glm.hpp>
#include <reactphysics3d/reactphysics3d.h>
#include <string>

#include "../scripting/ScriptableEntity.h"

namespace engine {

struct TagComponent
{
    std::string tag{"Entity"};
};

struct TransformComponent
{
    glm::vec3 position{0.0f, 0.0f, 0.0f};
    glm::vec3 rotation{0.0f, 0.0f, 0.0f};
    glm::vec3 scale   {1.0f, 1.0f, 1.0f};
};

struct ColorComponent
{
    glm::vec3 color{0.8f, 0.5f, 0.3f};
};

struct MaterialComponent
{
    std::string name{"Material"};
    glm::vec3 albedo{0.8f, 0.5f, 0.3f};
    bool useTexture = false;
    std::string texturePath{"assets/crate.jpg"};
};

struct RigidBodyComponent
{
    reactphysics3d::RigidBody* body = nullptr;
    bool isStatic = false;
};

struct BoxColliderComponent
{
    reactphysics3d::Collider* collider = nullptr;
};

enum class NativeScriptType
{
    None,
    PlayerController
};

struct NativeScriptComponent
{
    ScriptableEntity* Instance = nullptr;
    NativeScriptType Type = NativeScriptType::None;

    ScriptableEntity* (*InstantiateScript)() = nullptr;
    void (*DestroyScript)(NativeScriptComponent*) = nullptr;

    template <typename T>
    void Bind()
    {
        Type = NativeScriptType::None;
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

struct AudioSourceComponent
{
    std::string filepath;
    bool playOnAwake = false;
    bool hasPlayed   = false;
};

} // namespace engine
