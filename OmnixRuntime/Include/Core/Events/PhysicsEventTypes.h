// ============================================================================
// PhysicsEventTypes.h - Physics Interaction Events
// ============================================================================

#pragma once

#include "Core/Events/GameEvent.h"
#include <cassert>
#include <cmath>

namespace eng::core {

// ============================================================================
// BASE PHYSICS EVENT CLASS
// ============================================================================

class PhysicsEvent : public GameEvent {
protected:
    explicit PhysicsEvent(uint8_t priority = 150)
        : GameEvent(priority) {}
};

// ============================================================================
// COLLISION ENTER EVENT
// ============================================================================

class CollisionEvent : public PhysicsEvent {
public:
    CollisionEvent(uint32_t bodyA, uint32_t bodyB,
                   float x = 0.0f, float y = 0.0f, float z = 0.0f,
                   uint8_t priority = 150)
        : PhysicsEvent(priority), bodyA(bodyA), bodyB(bodyB), x(x), y(y), z(z) {
        assert(bodyA != bodyB && "Cannot collide with self");
        assert(std::isfinite(x) && "Contact X must be finite");
        assert(std::isfinite(y) && "Contact Y must be finite");
        assert(std::isfinite(z) && "Contact Z must be finite");
    }

    DEFINE_EVENT_TYPE(CollisionEvent, EventType::PHYSICS_COLLISION_ENTER, "Collision")

    uint32_t getBodyA() const { return bodyA; }
    uint32_t getBodyB() const { return bodyB; }

    void getContactPoint(float& outX, float& outY, float& outZ) const {
        outX = x;
        outY = y;
        outZ = z;
    }

private:
    uint32_t bodyA, bodyB;
    float x, y, z;
};

// ============================================================================
// COLLISION EXIT EVENT
// ============================================================================

class CollisionExitEvent : public PhysicsEvent {
public:
    CollisionExitEvent(uint32_t bodyA, uint32_t bodyB, uint8_t priority = 150)
        : PhysicsEvent(priority), bodyA(bodyA), bodyB(bodyB) {
        assert(bodyA != bodyB && "Cannot separate from self");
    }

    DEFINE_EVENT_TYPE(CollisionExitEvent, EventType::PHYSICS_COLLISION_EXIT, "CollisionExit")

    uint32_t getBodyA() const { return bodyA; }
    uint32_t getBodyB() const { return bodyB; }

private:
    uint32_t bodyA, bodyB;
};

// ============================================================================
// TRIGGER ENTER EVENT
// ============================================================================

class TriggerEnterEvent : public PhysicsEvent {
public:
    TriggerEnterEvent(uint32_t triggerID, uint32_t bodyID, const std::string& eventName, uint8_t priority = 150)
        : PhysicsEvent(priority), triggerID(triggerID), bodyID(bodyID), eventName(eventName) {
        assert(triggerID != 0 && "Trigger ID cannot be zero");
        assert(bodyID != 0 && "Body ID cannot be zero");
    }

    DEFINE_EVENT_TYPE(TriggerEnterEvent, EventType::PHYSICS_TRIGGER_ENTER, "TriggerEnter")

    uint32_t getTriggerID() const { return triggerID; }
    uint32_t getBodyID() const { return bodyID; }
    const std::string& getEventName() const { return eventName; }

private:
    uint32_t triggerID;
    uint32_t bodyID;
    std::string eventName;
};

// ============================================================================
// TRIGGER EXIT EVENT
// ============================================================================

class TriggerExitEvent : public PhysicsEvent {
public:
    TriggerExitEvent(uint32_t triggerID, uint32_t bodyID, const std::string& eventName, uint8_t priority = 150)
        : PhysicsEvent(priority), triggerID(triggerID), bodyID(bodyID), eventName(eventName) {
        assert(triggerID != 0 && "Trigger ID cannot be zero");
        assert(bodyID != 0 && "Body ID cannot be zero");
    }

    DEFINE_EVENT_TYPE(TriggerExitEvent, EventType::PHYSICS_TRIGGER_EXIT, "TriggerExit")

    uint32_t getTriggerID() const { return triggerID; }
    uint32_t getBodyID() const { return bodyID; }
    const std::string& getEventName() const { return eventName; }

private:
    uint32_t triggerID;
    uint32_t bodyID;
    std::string eventName;
};

// ============================================================================
// TRIGGER STAY EVENT
// ============================================================================

class TriggerStayEvent : public PhysicsEvent {
public:
    TriggerStayEvent(uint32_t triggerID, uint32_t bodyID, const std::string& eventName, uint8_t priority = 150)
        : PhysicsEvent(priority), triggerID(triggerID), bodyID(bodyID), eventName(eventName) {
        assert(triggerID != 0 && "Trigger ID cannot be zero");
        assert(bodyID != 0 && "Body ID cannot be zero");
    }

    DEFINE_EVENT_TYPE(TriggerStayEvent, EventType::PHYSICS_TRIGGER_STAY, "TriggerStay")

    uint32_t getTriggerID() const { return triggerID; }
    uint32_t getBodyID() const { return bodyID; }
    const std::string& getEventName() const { return eventName; }

private:
    uint32_t triggerID;
    uint32_t bodyID;
    std::string eventName;
};

// ============================================================================
// GAMEPLAY EVENT
// ============================================================================

class GameplayEvent : public GameEvent {
public:
    GameplayEvent(const std::string& eventName, uint32_t sourceID = 0, uint32_t targetID = 0, uint8_t priority = 100)
        : GameEvent(priority), eventName(eventName), sourceID(sourceID), targetID(targetID) {
        assert(!eventName.empty() && "Event name cannot be empty");
    }

    DEFINE_EVENT_TYPE(GameplayEvent, EventType::GAMEPLAY_EVENT, "Gameplay")

    const std::string& getEventName() const { return eventName; }
    uint32_t getSourceID() const { return sourceID; }
    uint32_t getTargetID() const { return targetID; }

private:
    std::string eventName;
    uint32_t sourceID;
    uint32_t targetID;
};

} // namespace eng::core
