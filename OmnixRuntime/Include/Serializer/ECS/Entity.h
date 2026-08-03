#pragma once

#include <vector>
#include <cstdint>
#include <string>
#include <algorithm>

// Forward declarations
class Component;
// Entity type enumeration
enum class EntityType {
    UNKNOWN = 0,
    PLAYER,
    ENEMY,
    ITEM,
    ENVIRONMENT,
    UI_ELEMENT,
    COUNT
};

class Entity {
public:
    uint32_t id;                    // Public for ECS access
    uint32_t generation;            // Public for ECS access
    std::string name;
    EntityType type;
    std::vector<uint32_t> componentTypes;  // Component type IDs this entity owns

    // Constructors
    Entity() : id(0), generation(0), name(""), type(EntityType::UNKNOWN) {}

    Entity(uint32_t entityId, uint32_t gen = 0)
        : id(entityId), generation(gen), name(""), type(EntityType::UNKNOWN) {}

    // Getters for serializable data
    uint32_t GetEntityID() const { return id; }
    uint32_t GetGeneration() const { return generation; }

    // Component management methods
    void AddComponentType(uint32_t componentTypeID) {
        if (std::find(componentTypes.begin(), componentTypes.end(), componentTypeID) == componentTypes.end()) {
            componentTypes.push_back(componentTypeID);
        }
    }

    void RemoveComponentType(uint32_t componentTypeID) {
        auto it = std::find(componentTypes.begin(), componentTypes.end(), componentTypeID);
        if (it != componentTypes.end()) {
            componentTypes.erase(it);
        }
    }

    bool HasComponentType(uint32_t componentTypeID) const {
        return std::find(componentTypes.begin(), componentTypes.end(), componentTypeID) != componentTypes.end();
    }
};

