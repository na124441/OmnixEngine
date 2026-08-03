#include "ECS/EntityManager.h"
#include <cassert>
#include <algorithm>

EntityManager::EntityManager() : m_LivingEntityCount(0) {
    m_EntityAlive.fill(false);
    // Initially, all entity IDs are available (excluding 0, which is reserved as invalid/sentinel)
    for (Entity entity = 1; entity < MAX_ENTITIES; ++entity) {
        m_AvailableEntities.push(entity);
    }
}

Entity EntityManager::CreateEntity() {
    assert(m_LivingEntityCount < MAX_ENTITIES && "Too many entities in existence.");
    Entity id = m_AvailableEntities.front();
    m_AvailableEntities.pop();
    m_EntityAlive[id] = true;
    m_ActiveEntities.push_back(id);
    ++m_LivingEntityCount;
    return id;
}

void EntityManager::DestroyEntity(Entity entity) {
    assert(entity < MAX_ENTITIES && "Entity out of range.");
    m_Signatures[entity].reset(); // clear its component signature
    m_EntityAlive[entity] = false;
    m_ActiveEntities.erase(std::remove(m_ActiveEntities.begin(), m_ActiveEntities.end(), entity), m_ActiveEntities.end());
    m_AvailableEntities.push(entity); // make the ID available again
    --m_LivingEntityCount;
}

void EntityManager::SetSignature(Entity entity, Signature signature) {
    assert(entity < MAX_ENTITIES && "Entity out of range.");
    m_Signatures[entity] = signature;
}

Signature EntityManager::GetSignature(Entity entity) const {
    assert(entity < MAX_ENTITIES && "Entity out of range.");
    return m_Signatures[entity];
}