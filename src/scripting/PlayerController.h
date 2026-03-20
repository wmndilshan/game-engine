#pragma once

#include "ScriptableEntity.h"
#include "../core/Input.h"
#include "../ecs/Components.h"

#include <GLFW/glfw3.h>
#include <reactphysics3d/reactphysics3d.h>

namespace engine {

class PlayerController : public ScriptableEntity
{
public:
    void OnUpdate(float deltaTime) override
    {
        (void)deltaTime;

        auto& transform = GetComponent<TransformComponent>();

        GLFWwindow* window = glfwGetCurrentContext();
        if (!window) return;

        float moveX = 0.0f;
        float moveZ = 0.0f;

        if (Input::isKeyPressed(window, GLFW_KEY_UP))    moveZ -= 1.0f;
        if (Input::isKeyPressed(window, GLFW_KEY_DOWN))  moveZ += 1.0f;
        if (Input::isKeyPressed(window, GLFW_KEY_LEFT))  moveX -= 1.0f;
        if (Input::isKeyPressed(window, GLFW_KEY_RIGHT)) moveX += 1.0f;

        // Prefer driving physics if this entity has a rigid body
        auto& rbView = m_Registry->getView<RigidBodyComponent>();
        auto rbIt = rbView.find(m_Entity);
        if (rbIt != rbView.end() && rbIt->second.body)
        {
            const float speed = 5.0f;
            const reactphysics3d::Vector3 currentVel = rbIt->second.body->getLinearVelocity();
            rbIt->second.body->setLinearVelocity(
                reactphysics3d::Vector3(moveX * speed, currentVel.y, moveZ * speed)
            );
            return;
        }

        // Fallback: direct transform edit
        const float speed = 3.0f;
        transform.position.x += moveX * speed * deltaTime;
        transform.position.z += moveZ * speed * deltaTime;
    }
};

} // namespace engine
