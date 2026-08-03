// ============================================================================
// EntityEventTypes.h - Entity Lifecycle Events
// ============================================================================

#pragma once

#include "Core/Events/GameEvent.h"
#include <cassert>

namespace eng::core {

// ============================================================================
// BASE ENTITY EVENT CLASS
// ============================================================================

class EntityEvent : public GameEvent {
public:
    virtual uint32_t getEntityID() const = 0;

protected:
    explicit EntityEvent(uint8_t priority = 128)
        : GameEvent(priority) {}
};

// ============================================================================
// ENTITY CREATED EVENT
// ============================================================================

class EntityCreatedEvent : public EntityEvent {
public:
    explicit EntityCreatedEvent(uint32_t entityID, uint8_t priority = 100)
        : EntityEvent(priority), entityID(entityID) {
        assert(entityID != 0 && "Entity ID cannot be zero");
    }

    DEFINE_EVENT_TYPE(EntityCreatedEvent, EventType::ENTITY_CREATED, "EntityCreated")

    uint32_t getEntityID() const override { return entityID; }

private:
    uint32_t entityID;
};

// ============================================================================
// ENTITY DESTROYED EVENT
// ============================================================================

class EntityDestroyedEvent : public EntityEvent {
public:
    explicit EntityDestroyedEvent(uint32_t entityID, uint8_t priority = 100)
        : EntityEvent(priority), entityID(entityID) {
        assert(entityID != 0 && "Entity ID cannot be zero");
    }

    DEFINE_EVENT_TYPE(EntityDestroyedEvent, EventType::ENTITY_DESTROYED, "EntityDestroyed")

    uint32_t getEntityID() const override { return entityID; }

private:
    uint32_t entityID;
};

// ============================================================================
// ENTITY UPDATED EVENT
// ============================================================================

class EntityUpdatedEvent : public EntityEvent {
public:
    explicit EntityUpdatedEvent(uint32_t entityID, uint8_t priority = 128)
        : EntityEvent(priority), entityID(entityID) {
        assert(entityID != 0 && "Entity ID cannot be zero");
    }

    DEFINE_EVENT_TYPE(EntityUpdatedEvent, EventType::ENTITY_UPDATED, "EntityUpdated")

    uint32_t getEntityID() const override { return entityID; }

private:
    uint32_t entityID;
};

} // namespace eng::core
