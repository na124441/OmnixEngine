// ============================================================================
// GameEvent.h - Base Event Class & Type Registry
// ============================================================================
// All events inherit from this class and implement getType()/getTypeName()
// ============================================================================

#pragma once

#include <cstdint>
#include <memory>
#include <chrono>
#include <functional>

// ============================================================================
// NAMESPACE WRAPPER (FIX #2: Prevents "Ambiguous symbol Event")
// ============================================================================
namespace Omnix {

// ============================================================================
// ENUM: Event Types Registry
// ============================================================================
// Defines all event categories in the system. Each concrete event class
// maps to exactly one EventType value.
// ============================================================================

enum class EventType : uint16_t {
    // Input Events (0-49)
    INPUT_KEY_PRESS = 0,
    INPUT_KEY_RELEASE = 1,
    INPUT_MOUSE_MOVE = 2,
    INPUT_MOUSE_CLICK = 3,
    INPUT_CONTROLLER_BUTTON = 4,

    // Entity Events (100-149)
    ENTITY_CREATED = 100,
    ENTITY_DESTROYED = 101,
    ENTITY_UPDATED = 102,

    // Scene Events (200-249)
    SCENE_LOADED = 200,
    SCENE_UNLOADED = 201,
    SCENE_CHANGED = 202,

    // Physics Events (300-349)
    PHYSICS_COLLISION_ENTER = 300,
    PHYSICS_COLLISION_EXIT = 301,
    PHYSICS_TRIGGER_ENTER = 302,
    PHYSICS_TRIGGER_EXIT = 303,
    PHYSICS_TRIGGER_STAY = 304,
    GAMEPLAY_EVENT = 305,

    // Custom events start here (1000+)
    CUSTOM_START = 1000
};

// ============================================================================
// LISTENER HANDLE (FIX #3: Safe listener removal)
// ============================================================================
// A ListenerHandle uniquely identifies a registered listener and enables
// safe removal even during event dispatch. ID 0 is reserved as invalid.
// ============================================================================

struct ListenerHandle {
    uint32_t id = 0;

    /// Check if handle is valid (non-zero ID)
    bool isValid() const { return id != 0; }

    /// Equality comparison for handle lookups
    bool operator==(const ListenerHandle& other) const {
        return id == other.id;
    }
};

// ============================================================================
// ABSTRACT EVENT BASE CLASS (FIX #1: Single definition only)
// ============================================================================
// All events inherit from GameEvent and must implement getType() and
// getTypeName(). The base class manages:
// - priority: event importance (0=highest, 255=lowest). Default=128.
// - timestamp: creation time in nanoseconds since epoch.
// - consumed: flag to stop event propagation to remaining listeners.
// ============================================================================

class GameEvent {
public:
    /// Smart pointer alias for ownership transfer semantics
    using EventPtr = std::unique_ptr<GameEvent>;

    virtual ~GameEvent() = default;

    // ────────────────────────────────────────────────────────────────────
    // PURE VIRTUAL INTERFACE - Implemented by concrete event classes
    // ────────────────────────────────────────────────────────────────────

    /// Get the event type ID (used for routing to listeners)
    virtual EventType getType() const = 0;

    /// Get human-readable event type name (for logging/debugging)
    virtual const char* getTypeName() const = 0;

    // ────────────────────────────────────────────────────────────────────
    // METADATA ACCESSORS
    // ────────────────────────────────────────────────────────────────────

    /// Returns the event creation time in nanoseconds since epoch
    uint64_t getTimestamp() const { return timestamp; }

    /// Returns event priority (0=highest, 255=lowest)
    uint8_t getPriority() const { return priority; }

    /// Check if event has been consumed (propagation stopped)
    bool isConsumed() const { return consumed; }

    // ────────────────────────────────────────────────────────────────────
    // EVENT PROPAGATION CONTROL
    // ────────────────────────────────────────────────────────────────────

    /// Set consumed flag to prevent propagation to subsequent listeners
    void consume() { consumed = true; }

protected:
    /// Protected constructor for derived classes
    /// @param priority Event importance: 0=highest, 255=lowest, 128=default
    explicit GameEvent(uint8_t priority = 128)
        : priority(priority),
          timestamp(std::chrono::high_resolution_clock::now()
                        .time_since_epoch()
                        .count()),
          consumed(false) {}

private:
    uint8_t priority;     // Event importance level
    uint64_t timestamp;   // Creation time in nanoseconds
    bool consumed;        // Propagation control flag
};

// ============================================================================
// HELPER MACRO: DEFINE_EVENT_TYPE
// ============================================================================
// Purpose: Reduce boilerplate by automatically implementing getType() and
// getTypeName() for concrete event classes.
//
// Usage in derived class:
// class KeyPressEvent : public InputEvent {
// public:
//     KeyPressEvent(int keyCode) : InputEvent(128), keyCode(keyCode) {}
//     DEFINE_EVENT_TYPE(KeyPressEvent, EventType::INPUT_KEY_PRESS, "KeyPress")
// private:
//     int keyCode;
// };
// ============================================================================

#define DEFINE_EVENT_TYPE(ClassName, EventTypeEnum, TypeName) \
    EventType getType() const override { \
        return EventTypeEnum; \
    } \
    const char* getTypeName() const override { \
        return TypeName; \
    }

} // namespace Omnix

