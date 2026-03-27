#include "PhysicsSystem.h"
#include "../ecs/Components.h"

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/quaternion.hpp>
#define GLM_ENABLE_EXPERIMENTAL
#include <glm/gtx/euler_angles.hpp>

#include "../core/Log.h"

#include <string>
#include <algorithm>
#include <cmath>

namespace engine {

static reactphysics3d::Quaternion ToRp3dQuaternionFromEulerXYZ(const glm::vec3& eulerRadians)
{
    glm::mat4 rot(1.0f);
    rot = glm::rotate(rot, eulerRadians.x, glm::vec3(1.0f, 0.0f, 0.0f));
    rot = glm::rotate(rot, eulerRadians.y, glm::vec3(0.0f, 1.0f, 0.0f));
    rot = glm::rotate(rot, eulerRadians.z, glm::vec3(0.0f, 0.0f, 1.0f));

    const glm::quat q = glm::quat_cast(rot); // (w,x,y,z)
    return reactphysics3d::Quaternion(q.x, q.y, q.z, q.w); // RP3D is (x,y,z,w)
}

static reactphysics3d::Vector3 ToHalfExtents(const glm::vec3& scale)
{
    return reactphysics3d::Vector3(
        std::max(0.05f, std::abs(scale.x) * 0.5f),
        std::max(0.05f, std::abs(scale.y) * 0.5f),
        std::max(0.05f, std::abs(scale.z) * 0.5f)
    );
}

// ── Initialise ──────────────────────────────────────────────────────────────

void PhysicsSystem::init()
{
    m_world = m_physicsCommon.createPhysicsWorld();
    GE_INFO("[PhysicsSystem] World created.");
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
        const glm::mat4 rotMat = glm::mat4_cast(glmQuat);
        float rx = 0.0f, ry = 0.0f, rz = 0.0f;
        glm::extractEulerAngleXYZ(rotMat, rx, ry, rz);
        tc.rotation = glm::vec3(rx, ry, rz);
    }
}

// ── Add a rigid body + box collider ─────────────────────────────────────────

void PhysicsSystem::addRigidBody(Entity entity, Registry& registry, bool isStatic)
{
    if (!m_world) return;

    auto& tc = registry.getComponent<TransformComponent>(entity);

    // Convert ECS transform → RP3D transform
    reactphysics3d::Vector3 rpPos(tc.position.x, tc.position.y, tc.position.z);

    // Build orientation quaternion from Euler angles using the same XYZ order as rendering.
    const reactphysics3d::Quaternion rpQuat = ToRp3dQuaternionFromEulerXYZ(tc.rotation);

    reactphysics3d::Transform rpTransform(rpPos, rpQuat);

    // Create the rigid body
    reactphysics3d::RigidBody* body = m_world->createRigidBody(rpTransform);
    body->setType(isStatic ? reactphysics3d::BodyType::STATIC
                           : reactphysics3d::BodyType::DYNAMIC);

    // Register the RigidBodyComponent in the ECS
    RigidBodyComponent rbComp;
    rbComp.body = body;
    rbComp.isStatic = isStatic;
    registry.addComponent(entity, rbComp);

    // Create a box collider that matches the entity's scale (half-extents)
    reactphysics3d::Vector3 halfExtents = ToHalfExtents(tc.scale);
    reactphysics3d::BoxShape* boxShape = m_physicsCommon.createBoxShape(halfExtents);
    reactphysics3d::Collider* collider = body->addCollider(boxShape,
                                                            reactphysics3d::Transform::identity());

    // Register the BoxColliderComponent in the ECS
    BoxColliderComponent bcComp;
    bcComp.collider = collider;
    registry.addComponent(entity, bcComp);
    GE_INFO(std::string("[PhysicsSystem] Entity ") + std::to_string(entity) + " -> " + (isStatic ? "STATIC" : "DYNAMIC") +
            " body + box collider (" + std::to_string(tc.scale.x) + "x" + std::to_string(tc.scale.y) + "x" + std::to_string(tc.scale.z) + ")");
}

void PhysicsSystem::rebuildWorld(Registry& registry)
{
    shutdown();
    init();

    auto rigidBodies = registry.getView<RigidBodyComponent>();
    for (const auto& [entity, rbComp] : rigidBodies)
    {
        if (!registry.hasComponent<TransformComponent>(entity))
            continue;

        addRigidBody(entity, registry, rbComp.isStatic);
    }
}

// ── Shutdown ────────────────────────────────────────────────────────────────

void PhysicsSystem::shutdown()
{
    if (m_world)
    {
        m_physicsCommon.destroyPhysicsWorld(m_world);
        m_world = nullptr;
        GE_INFO("[PhysicsSystem] World destroyed.");
    }
}

} // namespace engine
