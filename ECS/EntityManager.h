#pragma once

#include <queue>
#include <array>
#include <vector>
#include <memory>
#include "ECSConfig.h"

class EntityManager {
public:
    EntityManager();

    std::unique_ptr<EntityManager> Clone() const {
        auto clone = std::make_unique<EntityManager>();
        clone->m_LivingEntityCount = m_LivingEntityCount;
        clone->m_AvailableEntities = m_AvailableEntities;
        clone->m_Signatures = m_Signatures;
        clone->m_ActiveEntities = m_ActiveEntities;
        clone->m_EntityAlive = m_EntityAlive;
        return clone;
    }

    // Create a new entity and return its ID
    Entity CreateEntity();

    // Destroy an existing entity
    void DestroyEntity(Entity entity);

    // Set the component signature for an entity
    void SetSignature(Entity entity, Signature signature);

    // Get the component signature for an entity
    Signature GetSignature(Entity entity) const;

    const std::vector<Entity>& GetActiveEntities() const { return m_ActiveEntities; }
    bool IsEntityAlive(Entity entity) const { return entity < MAX_ENTITIES && m_EntityAlive[entity]; }

    std::uint32_t m_LivingEntityCount;          // Current number of active entities

private:
    std::queue<Entity> m_AvailableEntities;     // Queue of unused entity IDs
    std::array<Signature, MAX_ENTITIES> m_Signatures;  // Signature for each entity
    std::vector<Entity> m_ActiveEntities;       // List of active entities
    std::array<bool, MAX_ENTITIES> m_EntityAlive; // Flag indicating if an entity is alive
};