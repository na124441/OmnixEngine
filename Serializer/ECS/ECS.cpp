#include "ECS.h"

bool ECS::Initialize(ComponentSchemaRegistry* schemaRegistry) {
    m_SchemaRegistry = schemaRegistry;
    LOG_INFO("ECS initialized.");
    return true;
}

void ECS::Shutdown() {
    // Clean up component pools
    for (auto& pair : m_ComponentPools) {
        delete pair.second;
    }
    m_ComponentPools.clear();
    m_Entities.clear();
    m_EntityIDsSorted.clear();
    LOG_INFO("ECS shut down.");
}

void ECS::Update(float deltaTime) {
    ProcessDeferredOps();
    for (auto& system : m_Systems) {
        system->Update(deltaTime);
    }
    CleanupDeadEntities();
}

uint32_t ECS::CreateEntity() {
    uint32_t id = m_NextEntityID++;
    m_Entities[id] = {id, 0, ENTITY_ALIVE, {}};
    m_EntityIDsSorted.push_back(id);
    std::sort(m_EntityIDsSorted.begin(), m_EntityIDsSorted.end());
    return id;
}

bool ECS::DestroyEntity(uint32_t entityID) {
    if (m_Entities.find(entityID) == m_Entities.end()) {
        return false;
    }
    m_Entities[entityID].state = ENTITY_DEAD;
    return true;
}

bool ECS::IsEntityAlive(uint32_t entityID) const {
    auto it = m_Entities.find(entityID);
    return it != m_Entities.end() && it->second.state == ENTITY_ALIVE;
}

const ECS::EntityMetadata* ECS::GetEntityMetadata(uint32_t entityID) const {
    auto it = m_Entities.find(entityID);
    if (it != m_Entities.end()) {
        return &it->second;
    }
    return nullptr;
}

const std::vector<uint32_t>& ECS::GetAllEntities() const {
    return m_EntityIDsSorted;
}

const void* ECS::GetComponent(uint32_t entityID, ComponentTypeID componentTypeID) const {
    auto it = m_ComponentPools.find(componentTypeID);
    if (it != m_ComponentPools.end()) {
        return it->second->GetComponentRaw(entityID);
    }
    return nullptr;
}

void ECS::RegisterSystem(std::unique_ptr<ISystem> system) {
    m_Systems.push_back(std::move(system));
}

void ECS::ProcessDeferredOps() {
    // Implementation for deferred operations would go here
}

void ECS::CleanupDeadEntities() {
    // Implementation for cleaning up dead entities would go here
}
