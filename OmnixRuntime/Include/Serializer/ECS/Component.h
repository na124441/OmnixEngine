#pragma once

#include <vector>
#include <unordered_map>
#include <cstdint>
#include <typeinfo>
#include <utility> // For std::forward
#include "../../Core/Logger.h"

// Standardize ComponentTypeID to uint8_t to match ECSConfig.h's ComponentType
// MAX_COMPONENTS is 32, which fits in uint8_t.
using ComponentTypeID = uint8_t;

// Base interface for component pools to allow non-templated access
class IComponentPool {
public:
    virtual ~IComponentPool() = default;
    // Virtual method to get raw component data by entity ID
    virtual const void* GetComponentRaw(uint32_t entityID) const = 0;
};

template <typename ComponentType>
class ComponentPool : public IComponentPool { // Inherit from IComponentPool
private:
    std::vector<ComponentType> m_Components;
    std::unordered_map<uint32_t, uint32_t> m_EntityToIndex;
    std::vector<uint32_t> m_IndexToEntity;

public:
    [[nodiscard]] bool HasComponent(uint32_t entityID) const {
        return m_EntityToIndex.count(entityID);
    }

    const ComponentType* GetComponent(uint32_t entityID) const {
        auto it = m_EntityToIndex.find(entityID);
        return it != m_EntityToIndex.end() ? &m_Components[it->second] : nullptr;
    }

    ComponentType* GetComponent(uint32_t entityID) {
        auto it = m_EntityToIndex.find(entityID);
        return it != m_EntityToIndex.end() ? &m_Components[it->second] : nullptr;
    }

    // Implementation of the virtual method from IComponentPool
    const void* GetComponentRaw(uint32_t entityID) const override {
        return GetComponent(entityID);
    }

    // Use perfect forwarding and emplace_back to construct component in-place
    template<typename... Args>
    ComponentType* AddComponent(uint32_t entityID, Args&&... args) {
        if (HasComponent(entityID)) {
            LOG_ERROR("Entity %u already has this component", entityID);
            return nullptr;
        }

        uint32_t newIndex = m_Components.size();
        m_Components.emplace_back(std::forward<Args>(args)...);
        m_IndexToEntity.push_back(entityID);
        m_EntityToIndex[entityID] = newIndex;

        return &m_Components.back();
    }

    bool RemoveComponent(uint32_t entityID) {
        auto it = m_EntityToIndex.find(entityID);
        if (it == m_EntityToIndex.end()) {
            LOG_WARN("Entity %u doesn't have this component", entityID);
            return false;
        }

        uint32_t indexToRemove = it->second;
        uint32_t lastIndex = m_Components.size() - 1;
        uint32_t lastEntityID = m_IndexToEntity.back();

        m_Components[indexToRemove] = std::move(m_Components.back());
        m_IndexToEntity[indexToRemove] = lastEntityID;
        m_EntityToIndex[lastEntityID] = indexToRemove;

        m_Components.pop_back();
        m_IndexToEntity.pop_back();
        m_EntityToIndex.erase(entityID);

        return true;
    }
};
