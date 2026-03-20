#include "PhysicsSystem.h"
#include "../ecs/Components.h"

#include <glm/glm.hpp>
#include <glm/gtc/quaternion.hpp>

#include <cstdio>

namespace engine {

// ── Initialise ──────────────────────────────────────────────────────────────

void PhysicsSystem::init()
{
    m_world = m_physicsCommon.createPhysicsWorld();
    std::printf("[PhysicsSystem] World created.\n");
}

// ── Step simulation & sync transforms ───────────────────────────────────────

void PhysicsSystem::stepPhysics(Registry& registry, float deltaTime)
{
    if (!m_world) return;

    // Advance the physics simulation
    m_world->update(deltaTime);

    // Sync: Physics Engine → ECS TransformComponent
    auto& transforms  = registry.getView<TransformComponent>();
    auto& rigidBodies = registry.getView<RigidBodyComponent>();

    for (auto& [entity, rbComp] : rigidBodies)
    {
        if (!rbComp.body) continue;

        // Only update if this entity also has a TransformComponent
        auto it = transforms.find(entity);
        if (it == transforms.end()) continue;

        TransformComponent& tc = it->second;

        // Get the physics body's transform
        const reactphysics3d::Transform& physTransform = rbComp.body->getTransform();

        // Position
        const reactphysics3d::Vector3& pos = physTransform.getPosition();
        tc.position = glm::vec3(pos.x, pos.y, pos.z);

        // Orientation → Euler angles
        const reactphysics3d::Quaternion& q = physTransform.getOrientation();
        glm::quat glmQuat(q.w, q.x, q.y, q.z);  // glm::quat is (w,x,y,z)
        tc.rotation = glm::eulerAngles(glmQuat);
    }
}

// ── Add a rigid body + box collider ─────────────────────────────────────────

void PhysicsSystem::addRigidBody(Entity entity, Registry& registry, bool isStatic)
{
    auto& tc = registry.getComponent<TransformComponent>(entity);

    // Convert ECS transform → RP3D transform
    reactphysics3d::Vector3 rpPos(tc.position.x, tc.position.y, tc.position.z);

    // Build orientation quaternion from Euler angles
    glm::quat glmQuat = glm::quat(tc.rotation);  // pitch, yaw, roll
    reactphysics3d::Quaternion rpQuat(glmQuat.x, glmQuat.y, glmQuat.z, glmQuat.w);

    reactphysics3d::Transform rpTransform(rpPos, rpQuat);

    // Create the rigid body
    reactphysics3d::RigidBody* body = m_world->createRigidBody(rpTransform);
    body->setType(isStatic ? reactphysics3d::BodyType::STATIC
                           : reactphysics3d::BodyType::DYNAMIC);

    // Register the RigidBodyComponent in the ECS
    RigidBodyComponent rbComp;
    rbComp.body = body;
    registry.addComponent(entity, rbComp);

    // Create a box collider that matches the entity's scale (half-extents)
    reactphysics3d::Vector3 halfExtents(tc.scale.x * 0.5f,
                                         tc.scale.y * 0.5f,
                                         tc.scale.z * 0.5f);
    reactphysics3d::BoxShape* boxShape = m_physicsCommon.createBoxShape(halfExtents);
    reactphysics3d::Collider* collider = body->addCollider(boxShape,
                                                            reactphysics3d::Transform::identity());

    // Register the BoxColliderComponent in the ECS
    BoxColliderComponent bcComp;
    bcComp.collider = collider;
    registry.addComponent(entity, bcComp);

    std::printf("[PhysicsSystem] Entity %u → %s body + box collider (%.1fx%.1fx%.1f)\n",
                entity,
                isStatic ? "STATIC" : "DYNAMIC",
                tc.scale.x, tc.scale.y, tc.scale.z);
}

// ── Shutdown ────────────────────────────────────────────────────────────────

void PhysicsSystem::shutdown()
{
    if (m_world)
    {
        m_physicsCommon.destroyPhysicsWorld(m_world);
        m_world = nullptr;
        std::printf("[PhysicsSystem] World destroyed.\n");
    }
}

} // namespace engine
