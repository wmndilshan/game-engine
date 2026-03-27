#pragma once

#include <reactphysics3d/reactphysics3d.h>
#include "../ecs/ECS.h"

namespace engine {

class PhysicsSystem
{
public:
    PhysicsSystem()  = default;
    ~PhysicsSystem() = default;

    /// Create the physics world.
    void init();

    /// Advance the simulation by deltaTime, then copy transforms back to ECS.
    void stepPhysics(Registry& registry, float deltaTime);

    /// Create a rigid body (and a unit-box collider) for the given entity.
    /// @param isStatic  true → STATIC body, false → DYNAMIC body.
    void addRigidBody(Entity entity, Registry& registry, bool isStatic);
    void rebuildWorld(Registry& registry);

    /// Clean up the physics world.
    void shutdown();

private:
    reactphysics3d::PhysicsCommon  m_physicsCommon;
    reactphysics3d::PhysicsWorld*  m_world = nullptr;
};

} // namespace engine
