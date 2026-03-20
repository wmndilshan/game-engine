#pragma once

#include <glm/glm.hpp>
#include <reactphysics3d/reactphysics3d.h>

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

} // namespace engine
