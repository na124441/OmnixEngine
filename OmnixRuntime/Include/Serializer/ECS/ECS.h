// Be the only owner of entities and components 
//Maintain : 
// 1. Entity Registry
// 2. Component Pools
//Ensure deterministic iteration order : 
//  sort entities by ID before processing
//  sort components by type ID before processing

#pragma once

#include <vector>
#include <unordered_map>
#include <memory>
#include <algorithm>
#include <functional>
#include <typeindex>
#include "../../Core/Logger.h"
#include "Serializer/ECS/Component.h"      // Include the authoritative definition
#include "Serializer/ECS/ComponentTypes.h" // Includes Component.h, provides ComponentTypeID and constants
#include "../Components/Logical/Health.h"
#include "../Components/Spatial/Transform.h"
#include "../Components/Physical/RigidBody.h"

// Forward declarations
struct ComponentSchema;
class ComponentSchemaRegistry;

// Entity state
enum EntityState : uint8_t {
    ENTITY_ALIVE = 0,
    ENTITY_DEAD = 1,     // Marked for deletion
    ENTITY_INVALID = 2
};

// System interface (for processing components)
class ISystem {
public:
    virtual ~ISystem() = default;
    virtual void Update(float deltaTime) = 0;
    virtual const char* GetName() const = 0;
};

// Correct implementation of GetComponentTypeID
template<typename T>
uint32_t GetComponentTypeID() {
    if constexpr (std::is_same_v<T, Health>) return HEALTH_COMPONENT;
    if constexpr (std::is_same_v<T, Transform>) return TRANSFORM_COMPONENT;
    if constexpr (std::is_same_v<T, RigidBody>) return PHYSICS_COMPONENT;
    // Add other components here...
    return -1; // Invalid component
}

// ============================================================================
// ENTITY COMPONENT SYSTEM - Central Manager
// ============================================================================
class ECS {
private:
    // 2. ENTITY REGISTRY
    // Maps EntityID -> Entity metadata
    struct EntityMetadata {
        uint32_t id;
        uint32_t generation;
        EntityState state;
        std::vector<ComponentTypeID> componentTypes;  // Components this entity has
    };
    std::unordered_map<uint32_t, EntityMetadata> m_Entities;
    std::vector<uint32_t> m_EntityIDsSorted;  // Sorted for deterministic order

    // 3. ENTITY ID GENERATION
    uint32_t m_NextEntityID = 1;

    // 4. COMPONENT POOLS (type-erased storage)
    std::unordered_map<ComponentTypeID, IComponentPool*> m_ComponentPools;  // TypeID -> Pool ptr

    // 5. SYSTEMS
    std::vector<std::unique_ptr<ISystem>> m_Systems;

    // 6. SCHEMA REGISTRY (reference, not owned)
    ComponentSchemaRegistry* m_SchemaRegistry;

    // 7. DEFERRED OPERATIONS (end-of-frame processing)
    struct DeferredOp {
        enum OpType { ADD_COMPONENT, REMOVE_COMPONENT, DESTROY_ENTITY } type;
        uint32_t entityID;
        ComponentTypeID componentTypeID;
    };
    std::vector<DeferredOp> m_DeferredOps;

public:
    ECS() = default;

    // 10. LIFECYCLE
    bool Initialize(ComponentSchemaRegistry* schemaRegistry);
    void Shutdown();
    void Update(float deltaTime);

    // 11. ENTITY OPERATIONS
    uint32_t CreateEntity();
    bool DestroyEntity(uint32_t entityID);
    bool IsEntityAlive(uint32_t entityID) const;
    const EntityMetadata* GetEntityMetadata(uint32_t entityID) const;
    const std::vector<uint32_t>& GetAllEntities() const;

    // 12. COMPONENT OPERATIONS (templated)
    template <typename ComponentType>
    ComponentType* AddComponent(uint32_t entityID, const ComponentType& component);

    template <typename ComponentType>
    bool RemoveComponent(uint32_t entityID);

    template <typename ComponentType>
    bool HasComponent(uint32_t entityID) const;

    template <typename ComponentType>
    const ComponentType* GetComponent(uint32_t entityID) const;

    template <typename ComponentType>
    ComponentType* GetComponent(uint32_t entityID);

    const void* GetComponent(uint32_t entityID, ComponentTypeID componentTypeID) const;

    // 15. SYSTEM MANAGEMENT
    void RegisterSystem(std::unique_ptr<ISystem> system);

private:
    void ProcessDeferredOps();
    void CleanupDeadEntities();

    template <typename ComponentType>
    ComponentPool<ComponentType>* GetPool();

    template <typename ComponentType>
    const ComponentPool<ComponentType>* GetPool() const;
};

// ============================================================================
// TEMPLATE IMPLEMENTATIONS (Inline in header)
// ============================================================================

template <typename ComponentType>
ComponentType* ECS::AddComponent(uint32_t entityID, const ComponentType& component) {
    if (!IsEntityAlive(entityID)) {
        LOG_ERROR("Cannot add component to dead entity %u", entityID);
        return nullptr;
    }
    auto pool = GetPool<ComponentType>();
    ComponentType* result = pool->AddComponent(entityID, component);
    if (result) {
        auto it = m_Entities.find(entityID);
        if (it != m_Entities.end()) {
            it->second.componentTypes.push_back(GetComponentTypeID<ComponentType>());
            std::sort(it->second.componentTypes.begin(), it->second.componentTypes.end());
        }
    }
    return result;
}

template <typename ComponentType>
bool ECS::RemoveComponent(uint32_t entityID) {
    if (!IsEntityAlive(entityID)) {
        LOG_ERROR("Cannot remove component from dead entity %u", entityID);
        return false;
    }
    auto pool = GetPool<ComponentType>();
    if (!pool) return false;
    bool success = pool->RemoveComponent(entityID);
    if (success) {
        auto it = m_Entities.find(entityID);
        if (it != m_Entities.end()) {
            auto& types = it->second.componentTypes;
            uint32_t typeID = GetComponentTypeID<ComponentType>();
            types.erase(std::remove(types.begin(), types.end(), typeID), types.end());
        }
    }
    return success;
}

template <typename ComponentType>
bool ECS::HasComponent(uint32_t entityID) const {
    const auto pool = GetPool<ComponentType>();
    return pool && pool->HasComponent(entityID);
}

template <typename ComponentType>
const ComponentType* ECS::GetComponent(uint32_t entityID) const {
    const auto pool = GetPool<ComponentType>();
    return pool ? pool->GetComponent(entityID) : nullptr;
}

template <typename ComponentType>
ComponentType* ECS::GetComponent(uint32_t entityID) {
    auto pool = GetPool<ComponentType>();
    return pool ? pool->GetComponent(entityID) : nullptr;
}

template <typename ComponentType>
ComponentPool<ComponentType>* ECS::GetPool() {
    const uint32_t typeID = GetComponentTypeID<ComponentType>();
    auto it = m_ComponentPools.find(typeID);
    if (it != m_ComponentPools.end()) {
        return static_cast<ComponentPool<ComponentType>*>(it->second);
    }
    auto* newPool = new ComponentPool<ComponentType>();
    m_ComponentPools[typeID] = newPool;
    return newPool;
}

template <typename ComponentType>
const ComponentPool<ComponentType>* ECS::GetPool() const {
    const uint32_t typeID = GetComponentTypeID<ComponentType>();
    auto it = m_ComponentPools.find(typeID);
    if (it != m_ComponentPools.end()) {
        return static_cast<const ComponentPool<ComponentType>*>(it->second);
    }
    return nullptr;
}
