#pragma once

#include "ECS/ECSconfig.h" // For Entity definition
#include "Scene/Vector3.h"
#include "Scene/Quaternion.h"
#include "Physics/Public/PhysicsQueries.h"
#include <unordered_map>
#include <vector>

// Forward declarations of PhysX types to avoid header pollution
namespace physx {
    class PxFoundation;
    class PxPhysics;
    class PxDefaultCpuDispatcher;
    class PxScene;
    class PxMaterial;
    class PxRigidStatic;
    class PxActor;
}

// Forward declaration of Coordinator
class Coordinator;

namespace eng::physics {
    
    class PhysicsWorld {
    public:
        PhysicsWorld();
        ~PhysicsWorld();

        bool Initialize();
        void FixedUpdate(float fixedDeltaTime);
        void Shutdown();

        // Scene Lifecycle management
        void RegisterStaticColliders(Coordinator& coordinator);
        bool RegisterStaticCollider(Coordinator& coordinator, Entity entity);
        void UnregisterEntity(Entity entity);
        void RebuildStaticActor(Coordinator& coordinator, Entity entity);
        void ClearScene();

        // Physics Material Presets
        struct PhysicsMaterialData {
            float staticFriction = 0.5f;
            float dynamicFriction = 0.5f;
            float restitution = 0.6f;
        };

        // Physics Queries & Sweeps
        bool Raycast(const PhysicsRay& ray, float maxDistance, RaycastHit& outHit, uint32_t layer = 1, uint32_t mask = 0xFFFFFFFF);
        bool Raycast(const Vector3& origin, const Vector3& direction, float maxDistance, RaycastHit& outHit, uint32_t layer = 1, uint32_t mask = 0xFFFFFFFF);
        bool OverlapBox(const Vector3& center, const Vector3& halfExtents, std::vector<Entity>& outEntities, uint32_t layer = 1, uint32_t mask = 0xFFFFFFFF);
        bool OverlapSphere(const Vector3& center, float radius, std::vector<Entity>& outEntities, uint32_t layer = 1, uint32_t mask = 0xFFFFFFFF);
        bool OverlapCapsule(const Vector3& center, float radius, float height, std::vector<Entity>& outEntities, uint32_t layer = 1, uint32_t mask = 0xFFFFFFFF);
        bool SweepBox(const Vector3& center, const Vector3& halfExtents, const Vector3& direction, float maxDistance, SweepHit& outHit, uint32_t layer = 1, uint32_t mask = 0xFFFFFFFF);
        bool SweepSphere(const Vector3& center, float radius, const Vector3& direction, float maxDistance, SweepHit& outHit, uint32_t layer = 1, uint32_t mask = 0xFFFFFFFF);
        bool SweepCapsule(const Vector3& center, float radius, float height, const Vector3& direction, float maxDistance, SweepHit& outHit, uint32_t layer = 1, uint32_t mask = 0xFFFFFFFF);

        // Accessors & Diagnostics
        bool IsInitialized() const;
        size_t GetStaticActorCount() const;
        size_t GetRegisteredEntityCount() const;
        float GetFixedTimestep() const;
        int GetStepsThisFrame() const;

    private:
        physx::PxFoundation* m_Foundation = nullptr;
        physx::PxPhysics* m_Physics = nullptr;
        physx::PxDefaultCpuDispatcher* m_Dispatcher = nullptr;
        physx::PxScene* m_Scene = nullptr;
        physx::PxMaterial* m_DefaultMaterial = nullptr;

        float m_Accumulator = 0.0f;
        const float m_FixedTimestep = 1.0f / 60.0f;
        int m_StepsThisFrame = 0;

        std::unordered_map<Entity, physx::PxRigidStatic*> m_StaticActors;
        std::unordered_map<physx::PxActor*, Entity> m_ActorToEntity;
    };

} // namespace eng::physics
