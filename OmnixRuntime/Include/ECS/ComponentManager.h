#pragma once

#include <memory>
#include <unordered_map>
#include <array>
#include <string>
#include <cassert>
#include <typeinfo>
#include "ECS/ECSconfig.h"

// Interface for component arrays
class IComponentArray {
public:
    virtual ~IComponentArray() = default;
    virtual void EntityDestroyed(Entity entity) = 0;
    virtual std::shared_ptr<IComponentArray> Clone() const = 0;
};

// Component array for a specific component type
template<typename T>
class ComponentArray : public IComponentArray {
public:
    std::shared_ptr<IComponentArray> Clone() const override {
        auto clone = std::make_shared<ComponentArray<T>>();
        clone->m_ComponentArray = this->m_ComponentArray;
        clone->m_EntityToIndexMap = this->m_EntityToIndexMap;
        clone->m_IndexToEntityMap = this->m_IndexToEntityMap;
        clone->m_Size = this->m_Size;
        return clone;
    }

    void InsertData(Entity entity, T component) {
        assert(m_EntityToIndexMap.find(entity) == m_EntityToIndexMap.end() &&
               "Component added to same entity more than once.");

        size_t newIndex = m_Size;
        m_EntityToIndexMap[entity] = newIndex;
        m_IndexToEntityMap[newIndex] = entity;
        m_ComponentArray[newIndex] = component;
        ++m_Size;
    }

    void RemoveData(Entity entity) {
        assert(m_EntityToIndexMap.find(entity) != m_EntityToIndexMap.end() &&
               "Removing non-existent component.");

        size_t indexOfRemovedEntity = m_EntityToIndexMap[entity];
        size_t indexOfLastElement = m_Size - 1;
        m_ComponentArray[indexOfRemovedEntity] = m_ComponentArray[indexOfLastElement];

        Entity entityOfLastElement = m_IndexToEntityMap[indexOfLastElement];
        m_EntityToIndexMap[entityOfLastElement] = indexOfRemovedEntity;
        m_IndexToEntityMap[indexOfRemovedEntity] = entityOfLastElement;

        m_EntityToIndexMap.erase(entity);
        m_IndexToEntityMap.erase(indexOfLastElement);

        --m_Size;
    }

    T& GetData(Entity entity) {
        assert(m_EntityToIndexMap.find(entity) != m_EntityToIndexMap.end() &&
               "Retrieving non-existent component.");
        return m_ComponentArray[m_EntityToIndexMap[entity]];
    }

    void EntityDestroyed(Entity entity) override {
        if (m_EntityToIndexMap.find(entity) != m_EntityToIndexMap.end()) {
            RemoveData(entity);
        }
    }

private:
    std::array<T, MAX_ENTITIES> m_ComponentArray;
    std::unordered_map<Entity, size_t> m_EntityToIndexMap;
    std::unordered_map<size_t, Entity> m_IndexToEntityMap;
    size_t m_Size = 0;
};

// Component Manager
class ComponentManager {
public:
    std::unique_ptr<ComponentManager> Clone() const {
        auto clone = std::make_unique<ComponentManager>();
        for (const auto& pair : m_ComponentArrays) {
            if (pair.second) {
                clone->m_ComponentArrays[pair.first] = pair.second->Clone();
            }
        }
        return clone;
    }

    template<typename T>
    void RegisterComponent() {
        std::string typeName = std::string(typeid(T).name());

        assert(m_ComponentArrays.find(typeName) == m_ComponentArrays.end() &&
               "Registering component type more than once.");

        m_ComponentArrays[typeName] = std::make_shared<ComponentArray<T>>();
    }

    template<typename T>
    void AddComponent(Entity entity, T component) {
        GetComponentArray<T>()->InsertData(entity, component);
    }

    template<typename T>
    void RemoveComponent(Entity entity) {
        GetComponentArray<T>()->RemoveData(entity);
    }

    template<typename T>
    T& GetComponent(Entity entity) {
        return GetComponentArray<T>()->GetData(entity);
    }

    void EntityDestroyed(Entity entity) {
        for (auto const& pair : m_ComponentArrays) {
            pair.second->EntityDestroyed(entity);
        }
    }

private:
    std::unordered_map<std::string, std::shared_ptr<IComponentArray>> m_ComponentArrays;

    template<typename T>
    std::shared_ptr<ComponentArray<T>> GetComponentArray() {
        std::string typeName = std::string(typeid(T).name());

        assert(m_ComponentArrays.find(typeName) != m_ComponentArrays.end() &&
               "Component not registered before use.");

        return std::static_pointer_cast<ComponentArray<T>>(m_ComponentArrays[typeName]);
    }
};
