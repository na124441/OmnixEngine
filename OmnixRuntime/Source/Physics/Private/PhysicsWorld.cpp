#include "Physics/Public/PhysicsWorld.h"
#include "ECS/Coordinator.h"
#include "ECS/ECSComponents.h"
#include "Physics/Public/PhysicsConversion.h"
#include "Core/Logging/Logger.h"
#include <PxPhysicsAPI.h>
#include <algorithm>

namespace eng::physics {

    static physx::PxDefaultAllocator gAllocator;
    static physx::PxDefaultErrorCallback gErrorCallback;

    PhysicsWorld::PhysicsWorld() = default;

    PhysicsWorld::~PhysicsWorld() {
        Shutdown();
    }

    bool PhysicsWorld::Initialize() {
        CORE_LOG_INFO("[Physics] Initializing PhysicsWorld...");

        m_Foundation = PxCreateFoundation(PX_PHYSICS_VERSION, gAllocator, gErrorCallback);
        if (!m_Foundation) {
            CORE_LOG_ERROR("[Physics] Failed to create PxFoundation!");
            return false;
        }

        physx::PxTolerancesScale scale;
        m_Physics = PxCreatePhysics(PX_PHYSICS_VERSION, *m_Foundation, scale, true);
        if (!m_Physics) {
            CORE_LOG_ERROR("[Physics] Failed to create PxPhysics!");
            return false;
        }

        physx::PxSceneDesc sceneDesc(m_Physics->getTolerancesScale());
        sceneDesc.gravity = physx::PxVec3(0.0f, -9.81f, 0.0f);
        m_Dispatcher = physx::PxDefaultCpuDispatcherCreate(1);
        sceneDesc.cpuDispatcher = m_Dispatcher;
        sceneDesc.filterShader = physx::PxDefaultSimulationFilterShader;

        m_Scene = m_Physics->createScene(sceneDesc);
        if (!m_Scene) {
            CORE_LOG_ERROR("[Physics] Failed to create PxScene!");
            return false;
        }

        m_DefaultMaterial = m_Physics->createMaterial(0.5f, 0.5f, 0.6f);
        if (!m_DefaultMaterial) {
            CORE_LOG_ERROR("[Physics] Failed to create default PxMaterial!");
            return false;
        }

        CORE_LOG_INFO("[Physics] PhysicsWorld successfully initialized using real NVIDIA PhysX SDK.");
        return true;
    }

    void PhysicsWorld::FixedUpdate(float fixedDeltaTime) {
        if (!m_Scene) return;

        m_StepsThisFrame = 0;
        m_Accumulator += fixedDeltaTime;

        // Cap accumulator to prevent spiral of death during lag spikes
        constexpr float kMaxAccumulator = 5.0f * (1.0f / 60.0f);
        if (m_Accumulator > kMaxAccumulator) {
            m_Accumulator = kMaxAccumulator;
        }

        while (m_Accumulator >= m_FixedTimestep) {
            m_Scene->simulate(m_FixedTimestep);
            m_Scene->fetchResults(true);
            m_Accumulator -= m_FixedTimestep;
            m_StepsThisFrame++;
        }
    }

    void PhysicsWorld::Shutdown() {
        CORE_LOG_INFO("[Physics] Shutting down PhysicsWorld...");
        ClearScene();

        if (m_DefaultMaterial) {
            m_DefaultMaterial->release();
            m_DefaultMaterial = nullptr;
        }
        if (m_Scene) {
            m_Scene->release();
            m_Scene = nullptr;
        }
        if (m_Dispatcher) {
            m_Dispatcher->release();
            m_Dispatcher = nullptr;
        }
        if (m_Physics) {
            m_Physics->release();
            m_Physics = nullptr;
        }
        if (m_Foundation) {
            m_Foundation->release();
            m_Foundation = nullptr;
        }
        CORE_LOG_INFO("[Physics] PhysicsWorld shutdown complete.");
    }

    namespace {
        class PhysicsQueryFilterCallback : public physx::PxQueryFilterCallback {
        public:
            uint32_t queryLayer;
            uint32_t queryMask;
            bool isOverlap;

            PhysicsQueryFilterCallback(uint32_t layer, uint32_t mask, bool overlap = false)
                : queryLayer(layer), queryMask(mask), isOverlap(overlap) {}

            virtual physx::PxQueryHitType::Enum preFilter(
                const physx::PxFilterData& filterData, const physx::PxShape* shape, const physx::PxRigidActor* actor, physx::PxHitFlags& queryFlags) override 
            {
                physx::PxFilterData shapeFilter = shape->getQueryFilterData();
                uint32_t shapeLayer = shapeFilter.word0;
                uint32_t shapeMask = shapeFilter.word1;

                CORE_LOG_INFO("[Physics debug] preFilter check: shapeLayer=%u, shapeMask=%u, queryLayer=%u, queryMask=%u",
                              shapeLayer, shapeMask, queryLayer, queryMask);

                if ((shapeLayer & queryMask) && (queryLayer & shapeMask)) {
                    return isOverlap ? physx::PxQueryHitType::Enum::eTOUCH : physx::PxQueryHitType::Enum::eBLOCK;
                }
                return physx::PxQueryHitType::Enum::eNONE;
            }

            virtual physx::PxQueryHitType::Enum postFilter(
                const physx::PxFilterData& filterData, const physx::PxQueryHit& hit, const physx::PxShape* shape, const physx::PxRigidActor* actor) override 
            {
                return isOverlap ? physx::PxQueryHitType::Enum::eTOUCH : physx::PxQueryHitType::Enum::eBLOCK;
            }
        };
    }

    void PhysicsWorld::RegisterStaticColliders(Coordinator& coordinator) {
        ClearScene(); // Start fresh to avoid duplicates

        const auto& entities = coordinator.GetActiveEntities();
        for (Entity entity : entities) {
            RegisterStaticCollider(coordinator, entity);
        }
    }

    bool PhysicsWorld::RegisterStaticCollider(Coordinator& coordinator, Entity entity) {
        auto signature = coordinator.GetSignature(entity);
        bool hasTransform = signature.test(coordinator.GetComponentType<TransformComponent>());
        bool hasStaticBody = signature.test(coordinator.GetComponentType<StaticBodyComponent>());
        if (!hasTransform || !hasStaticBody) {
            return false;
        }

        auto& staticBody = coordinator.GetComponent<StaticBodyComponent>(entity);
        CORE_LOG_INFO("[Physics debug] RegisterStaticCollider: entity=%u, enabled=%d, layer=%u, mask=%u",
                      (unsigned int)entity, (int)staticBody.enabled, staticBody.collisionLayer, staticBody.collisionMask);
        if (!staticBody.enabled) {
            return false;
        }

        bool hasBox = signature.test(coordinator.GetComponentType<BoxColliderComponent>());
        bool hasSphere = signature.test(coordinator.GetComponentType<SphereColliderComponent>());
        bool hasCapsule = signature.test(coordinator.GetComponentType<CapsuleColliderComponent>());

        if (!hasBox && !hasSphere && !hasCapsule) {
            return false;
        }

        // If already registered, unregister first
        UnregisterEntity(entity);

        auto& transform = coordinator.GetComponent<TransformComponent>(entity);
        physx::PxTransform pxTransform = ToPxTransform(transform.position, transform.rotation);
        
        physx::PxRigidStatic* actor = m_Physics->createRigidStatic(pxTransform);
        if (!actor) {
            CORE_LOG_ERROR("[Physics] Failed to create PxRigidStatic for Entity {}", entity);
            return false;
        }

        actor->userData = reinterpret_cast<void*>(static_cast<uintptr_t>(entity));

        // Create shape filter data
        physx::PxFilterData filterData;
        filterData.word0 = staticBody.collisionLayer;
        filterData.word1 = staticBody.collisionMask;

        // 1. Box Collider
        if (hasBox) {
            auto& box = coordinator.GetComponent<BoxColliderComponent>(entity);
            float sx = std::max(box.size.x, 0.001f);
            float sy = std::max(box.size.y, 0.001f);
            float sz = std::max(box.size.z, 0.001f);

            physx::PxVec3 halfExtents(
                sx * 0.5f * std::max(transform.scale.x, 0.001f),
                sy * 0.5f * std::max(transform.scale.y, 0.001f),
                sz * 0.5f * std::max(transform.scale.z, 0.001f)
            );
            halfExtents.x = std::max(halfExtents.x, 0.001f);
            halfExtents.y = std::max(halfExtents.y, 0.001f);
            halfExtents.z = std::max(halfExtents.z, 0.001f);

            physx::PxBoxGeometry geometry(halfExtents.x, halfExtents.y, halfExtents.z);
            physx::PxShape* shape = m_Physics->createShape(geometry, *m_DefaultMaterial, true);
            if (shape) {
                shape->setLocalPose(physx::PxTransform(ToPxVec3(box.offset)));
                shape->setSimulationFilterData(filterData);
                shape->setQueryFilterData(filterData);
                actor->attachShape(*shape);
                shape->release();
            }
        }

        // 2. Sphere Collider
        if (hasSphere) {
            auto& sphere = coordinator.GetComponent<SphereColliderComponent>(entity);
            float r = std::max(sphere.radius, 0.001f);
            float maxScale = std::max({transform.scale.x, transform.scale.y, transform.scale.z});
            float finalRadius = std::max(r * maxScale, 0.001f);

            physx::PxSphereGeometry geometry(finalRadius);
            physx::PxShape* shape = m_Physics->createShape(geometry, *m_DefaultMaterial, true);
            if (shape) {
                shape->setLocalPose(physx::PxTransform(ToPxVec3(sphere.offset)));
                shape->setSimulationFilterData(filterData);
                shape->setQueryFilterData(filterData);
                actor->attachShape(*shape);
                shape->release();
            }
        }

        // 3. Capsule Collider
        if (hasCapsule) {
            auto& capsule = coordinator.GetComponent<CapsuleColliderComponent>(entity);
            float r = std::max(capsule.radius, 0.001f);
            float h = std::max(capsule.height, 2.0f * r);

            // Radius scales with XZ plane, height with Y
            float scaleXZ = std::max(transform.scale.x, transform.scale.z);
            float finalRadius = std::max(r * scaleXZ, 0.001f);
            float finalHeight = std::max(h * transform.scale.y, 2.0f * finalRadius);

            float halfHeight = (finalHeight - 2.0f * finalRadius) * 0.5f;
            if (halfHeight < 0.001f) halfHeight = 0.001f;

            physx::PxCapsuleGeometry geometry(finalRadius, halfHeight);
            physx::PxShape* shape = m_Physics->createShape(geometry, *m_DefaultMaterial, true);
            if (shape) {
                // PhysX capsule is X-axis aligned, rotate it by 90 degrees around Z to align with Y-axis
                physx::PxQuat rotZ(physx::PxHalfPi, physx::PxVec3(0.0f, 0.0f, 1.0f));
                physx::PxTransform localPose(ToPxVec3(capsule.offset), rotZ);
                shape->setLocalPose(localPose);
                shape->setSimulationFilterData(filterData);
                shape->setQueryFilterData(filterData);

                actor->attachShape(*shape);
                shape->release();
            }
        }

        m_Scene->addActor(*actor);
        m_StaticActors[entity] = actor;
        m_ActorToEntity[actor] = entity;
        return true;
    }

    void PhysicsWorld::UnregisterEntity(Entity entity) {
        auto it = m_StaticActors.find(entity);
        if (it != m_StaticActors.end()) {
            physx::PxRigidStatic* actor = it->second;
            if (m_Scene && actor) {
                m_Scene->removeActor(*actor);
            }
            m_ActorToEntity.erase(actor);
            if (actor) {
                actor->release();
            }
            m_StaticActors.erase(it);
        }
    }

    void PhysicsWorld::RebuildStaticActor(Coordinator& coordinator, Entity entity) {
        UnregisterEntity(entity);
        RegisterStaticCollider(coordinator, entity);
    }

    void PhysicsWorld::ClearScene() {
        for (auto& [entity, actor] : m_StaticActors) {
            if (m_Scene && actor) {
                m_Scene->removeActor(*actor);
                actor->release();
            }
        }
        m_StaticActors.clear();
        m_ActorToEntity.clear();
    }

    bool PhysicsWorld::Raycast(const PhysicsRay& ray, float maxDistance, RaycastHit& outHit, uint32_t layer, uint32_t mask) {
        return Raycast(ray.origin, ray.direction, maxDistance, outHit, layer, mask);
    }

    bool PhysicsWorld::Raycast(const Vector3& origin, const Vector3& direction, float maxDistance, RaycastHit& outHit, uint32_t layer, uint32_t mask) {
        if (!m_Scene) return false;

        physx::PxVec3 pxOrigin = ToPxVec3(origin);
        physx::PxVec3 pxDir = ToPxVec3(direction);
        if (pxDir.magnitudeSquared() < 0.0001f) {
            return false;
        }
        pxDir.normalize();

        CORE_LOG_INFO("[Physics debug] Raycast query: origin=(%f, %f, %f), dir=(%f, %f, %f), maxDist=%f, layer=%u, mask=%u",
                      origin.x, origin.y, origin.z, direction.x, direction.y, direction.z, maxDistance, layer, mask);

        PhysicsQueryFilterCallback filterCallback(layer, mask);
        physx::PxQueryFilterData filterData;
        filterData.flags |= physx::PxQueryFlag::ePREFILTER;

        physx::PxRaycastBuffer hit;
        bool status = m_Scene->raycast(pxOrigin, pxDir, maxDistance, hit, physx::PxHitFlag::eDEFAULT, filterData, &filterCallback);

        CORE_LOG_INFO("[Physics debug] Raycast status=%d, hasBlock=%d", (int)status, (int)hit.hasBlock);

        if (status && hit.hasBlock) {
            outHit.hit = true;
            outHit.distance = hit.block.distance;
            outHit.position = ToVector3(hit.block.position);
            outHit.normal = ToVector3(hit.block.normal);

            auto it = m_ActorToEntity.find(hit.block.actor);
            if (it != m_ActorToEntity.end()) {
                outHit.entity = it->second;
            } else {
                outHit.entity = 0;
            }
            return true;
        }

        return false;
    }

    bool PhysicsWorld::OverlapBox(const Vector3& center, const Vector3& halfExtents, std::vector<Entity>& outEntities, uint32_t layer, uint32_t mask) {
        if (!m_Scene) return false;

        physx::PxBoxGeometry geometry(ToPxVec3(halfExtents));
        physx::PxTransform pose(ToPxVec3(center));

        PhysicsQueryFilterCallback filterCallback(layer, mask, true);
        physx::PxQueryFilterData filterData;
        filterData.flags |= physx::PxQueryFlag::ePREFILTER;

        physx::PxOverlapHit hits[256];
        physx::PxOverlapBuffer buf(hits, 256);

        bool status = m_Scene->overlap(geometry, pose, buf, filterData, &filterCallback);
        if (status) {
            for (physx::PxU32 i = 0; i < buf.getNbAnyHits(); ++i) {
                const auto& h = buf.getAnyHit(i);
                auto it = m_ActorToEntity.find(h.actor);
                if (it != m_ActorToEntity.end()) {
                    if (std::find(outEntities.begin(), outEntities.end(), it->second) == outEntities.end()) {
                        outEntities.push_back(it->second);
                    }
                }
            }
            return !outEntities.empty();
        }
        return false;
    }

    bool PhysicsWorld::OverlapSphere(const Vector3& center, float radius, std::vector<Entity>& outEntities, uint32_t layer, uint32_t mask) {
        if (!m_Scene) return false;

        physx::PxSphereGeometry geometry(std::max(radius, 0.001f));
        physx::PxTransform pose(ToPxVec3(center));

        PhysicsQueryFilterCallback filterCallback(layer, mask, true);
        physx::PxQueryFilterData filterData;
        filterData.flags |= physx::PxQueryFlag::ePREFILTER;

        physx::PxOverlapHit hits[256];
        physx::PxOverlapBuffer buf(hits, 256);

        bool status = m_Scene->overlap(geometry, pose, buf, filterData, &filterCallback);
        if (status) {
            for (physx::PxU32 i = 0; i < buf.getNbAnyHits(); ++i) {
                const auto& h = buf.getAnyHit(i);
                auto it = m_ActorToEntity.find(h.actor);
                if (it != m_ActorToEntity.end()) {
                    if (std::find(outEntities.begin(), outEntities.end(), it->second) == outEntities.end()) {
                        outEntities.push_back(it->second);
                    }
                }
            }
            return !outEntities.empty();
        }
        return false;
    }

    bool PhysicsWorld::OverlapCapsule(const Vector3& center, float radius, float height, std::vector<Entity>& outEntities, uint32_t layer, uint32_t mask) {
        if (!m_Scene) return false;

        float r = std::max(radius, 0.001f);
        float h = std::max(height, 2.0f * r);
        float halfHeight = (h - 2.0f * r) * 0.5f;
        if (halfHeight < 0.001f) halfHeight = 0.001f;

        physx::PxCapsuleGeometry geometry(r, halfHeight);
        physx::PxQuat rotZ(physx::PxHalfPi, physx::PxVec3(0.0f, 0.0f, 1.0f));
        physx::PxTransform pose(ToPxVec3(center), rotZ);

        PhysicsQueryFilterCallback filterCallback(layer, mask, true);
        physx::PxQueryFilterData filterData;
        filterData.flags |= physx::PxQueryFlag::ePREFILTER;

        physx::PxOverlapHit hits[256];
        physx::PxOverlapBuffer buf(hits, 256);

        bool status = m_Scene->overlap(geometry, pose, buf, filterData, &filterCallback);
        if (status) {
            for (physx::PxU32 i = 0; i < buf.getNbAnyHits(); ++i) {
                const auto& h = buf.getAnyHit(i);
                auto it = m_ActorToEntity.find(h.actor);
                if (it != m_ActorToEntity.end()) {
                    if (std::find(outEntities.begin(), outEntities.end(), it->second) == outEntities.end()) {
                        outEntities.push_back(it->second);
                    }
                }
            }
            return !outEntities.empty();
        }
        return false;
    }

    bool PhysicsWorld::SweepBox(const Vector3& center, const Vector3& halfExtents, const Vector3& direction, float maxDistance, SweepHit& outHit, uint32_t layer, uint32_t mask) {
        if (!m_Scene) return false;

        physx::PxVec3 pxDir = ToPxVec3(direction);
        if (pxDir.magnitudeSquared() < 0.0001f) return false;
        pxDir.normalize();

        physx::PxBoxGeometry geometry(ToPxVec3(halfExtents));
        physx::PxTransform pose(ToPxVec3(center));

        PhysicsQueryFilterCallback filterCallback(layer, mask);
        physx::PxQueryFilterData filterData;
        filterData.flags |= physx::PxQueryFlag::ePREFILTER;

        physx::PxSweepBuffer hit;
        bool status = m_Scene->sweep(geometry, pose, pxDir, maxDistance, hit, physx::PxHitFlag::eDEFAULT, filterData, &filterCallback);
        if (status && hit.hasBlock) {
            outHit.hit = true;
            outHit.distance = hit.block.distance;
            outHit.position = ToVector3(hit.block.position);
            outHit.normal = ToVector3(hit.block.normal);

            auto it = m_ActorToEntity.find(hit.block.actor);
            if (it != m_ActorToEntity.end()) {
                outHit.entity = it->second;
            } else {
                outHit.entity = 0;
            }
            return true;
        }
        return false;
    }

    bool PhysicsWorld::SweepSphere(const Vector3& center, float radius, const Vector3& direction, float maxDistance, SweepHit& outHit, uint32_t layer, uint32_t mask) {
        if (!m_Scene) return false;

        physx::PxVec3 pxDir = ToPxVec3(direction);
        if (pxDir.magnitudeSquared() < 0.0001f) return false;
        pxDir.normalize();

        physx::PxSphereGeometry geometry(std::max(radius, 0.001f));
        physx::PxTransform pose(ToPxVec3(center));

        PhysicsQueryFilterCallback filterCallback(layer, mask);
        physx::PxQueryFilterData filterData;
        filterData.flags |= physx::PxQueryFlag::ePREFILTER;

        physx::PxSweepBuffer hit;
        bool status = m_Scene->sweep(geometry, pose, pxDir, maxDistance, hit, physx::PxHitFlag::eDEFAULT, filterData, &filterCallback);
        if (status && hit.hasBlock) {
            outHit.hit = true;
            outHit.distance = hit.block.distance;
            outHit.position = ToVector3(hit.block.position);
            outHit.normal = ToVector3(hit.block.normal);

            auto it = m_ActorToEntity.find(hit.block.actor);
            if (it != m_ActorToEntity.end()) {
                outHit.entity = it->second;
            } else {
                outHit.entity = 0;
            }
            return true;
        }
        return false;
    }

    bool PhysicsWorld::SweepCapsule(const Vector3& center, float radius, float height, const Vector3& direction, float maxDistance, SweepHit& outHit, uint32_t layer, uint32_t mask) {
        if (!m_Scene) return false;

        physx::PxVec3 pxDir = ToPxVec3(direction);
        if (pxDir.magnitudeSquared() < 0.0001f) return false;
        pxDir.normalize();

        float r = std::max(radius, 0.001f);
        float h = std::max(height, 2.0f * r);
        float halfHeight = (h - 2.0f * r) * 0.5f;
        if (halfHeight < 0.001f) halfHeight = 0.001f;

        physx::PxCapsuleGeometry geometry(r, halfHeight);
        physx::PxQuat rotZ(physx::PxHalfPi, physx::PxVec3(0.0f, 0.0f, 1.0f));
        physx::PxTransform pose(ToPxVec3(center), rotZ);

        PhysicsQueryFilterCallback filterCallback(layer, mask);
        physx::PxQueryFilterData filterData;
        filterData.flags |= physx::PxQueryFlag::ePREFILTER;

        physx::PxSweepBuffer hit;
        bool status = m_Scene->sweep(geometry, pose, pxDir, maxDistance, hit, physx::PxHitFlag::eDEFAULT, filterData, &filterCallback);
        if (status && hit.hasBlock) {
            outHit.hit = true;
            outHit.distance = hit.block.distance;
            outHit.position = ToVector3(hit.block.position);
            outHit.normal = ToVector3(hit.block.normal);

            auto it = m_ActorToEntity.find(hit.block.actor);
            if (it != m_ActorToEntity.end()) {
                outHit.entity = it->second;
            } else {
                outHit.entity = 0;
            }
            return true;
        }
        return false;
    }

    bool PhysicsWorld::IsInitialized() const {
        return m_Physics != nullptr;
    }

    size_t PhysicsWorld::GetStaticActorCount() const {
        return m_StaticActors.size();
    }

    size_t PhysicsWorld::GetRegisteredEntityCount() const {
        return m_StaticActors.size();
    }

    float PhysicsWorld::GetFixedTimestep() const {
        return m_FixedTimestep;
    }

    int PhysicsWorld::GetStepsThisFrame() const {
        return m_StepsThisFrame;
    }

} // namespace eng::physics
