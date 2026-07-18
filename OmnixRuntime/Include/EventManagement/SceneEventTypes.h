// ============================================================================
// SceneEventTypes.h - Scene Lifecycle Events
// ============================================================================

#pragma once

#include "EventManagement/GameEvent.h"
#include <cassert>

namespace Omnix {

    // ============================================================================
    // BASE SCENE EVENT CLASS
    // ============================================================================

    class SceneEvent : public GameEvent {
    public:
        virtual uint32_t getSceneID() const = 0;

    protected:
        explicit SceneEvent(uint8_t priority = 128)
            : GameEvent(priority) {}
    };

    // ============================================================================
    // SCENE LOADED EVENT
    // ============================================================================

    class SceneLoadedEvent : public SceneEvent {
    public:
        explicit SceneLoadedEvent(uint32_t sceneID, uint8_t priority = 100)
            : SceneEvent(priority), sceneID(sceneID) {
            assert(sceneID != 0 && "Scene ID cannot be zero");
        }

        DEFINE_EVENT_TYPE(SceneLoadedEvent, EventType::SCENE_LOADED, "SceneLoaded")

        uint32_t getSceneID() const override { return sceneID; }

    private:
        uint32_t sceneID;
    };

    // ============================================================================
    // SCENE UNLOADED EVENT
    // ============================================================================

    class SceneUnloadedEvent : public SceneEvent {
    public:
        explicit SceneUnloadedEvent(uint32_t sceneID, uint8_t priority = 100)
            : SceneEvent(priority), sceneID(sceneID) {
            assert(sceneID != 0 && "Scene ID cannot be zero");
        }

        DEFINE_EVENT_TYPE(SceneUnloadedEvent, EventType::SCENE_UNLOADED, "SceneUnloaded")

        uint32_t getSceneID() const override { return sceneID; }

    private:
        uint32_t sceneID;
    };

    // ============================================================================
    // ZONE ENTER EVENT
    // ============================================================================

    class ZoneEnterEvent : public GameEvent {
    public:
        ZoneEnterEvent(uint64_t zoneUUIDHigh, uint64_t zoneUUIDLow, uint32_t playerEntityID, uint8_t priority = 100)
            : GameEvent(priority)
            , m_ZoneUUIDHigh(zoneUUIDHigh)
            , m_ZoneUUIDLow(zoneUUIDLow)
            , m_PlayerEntityID(playerEntityID) {}

        DEFINE_EVENT_TYPE(ZoneEnterEvent, EventType::ZONE_ENTER, "ZoneEnter")

        uint64_t getZoneUUIDHigh() const { return m_ZoneUUIDHigh; }
        uint64_t getZoneUUIDLow() const { return m_ZoneUUIDLow; }
        uint32_t getPlayerEntityID() const { return m_PlayerEntityID; }

    private:
        uint64_t m_ZoneUUIDHigh;
        uint64_t m_ZoneUUIDLow;
        uint32_t m_PlayerEntityID;
    };

    // ============================================================================
    // ZONE EXIT EVENT
    // ============================================================================

    class ZoneExitEvent : public GameEvent {
    public:
        ZoneExitEvent(uint64_t zoneUUIDHigh, uint64_t zoneUUIDLow, uint32_t playerEntityID, uint8_t priority = 100)
            : GameEvent(priority)
            , m_ZoneUUIDHigh(zoneUUIDHigh)
            , m_ZoneUUIDLow(zoneUUIDLow)
            , m_PlayerEntityID(playerEntityID) {}

        DEFINE_EVENT_TYPE(ZoneExitEvent, EventType::ZONE_EXIT, "ZoneExit")

        uint64_t getZoneUUIDHigh() const { return m_ZoneUUIDHigh; }
        uint64_t getZoneUUIDLow() const { return m_ZoneUUIDLow; }
        uint32_t getPlayerEntityID() const { return m_PlayerEntityID; }

    private:
        uint64_t m_ZoneUUIDHigh;
        uint64_t m_ZoneUUIDLow;
        uint32_t m_PlayerEntityID;
    };

} // namespace Omnix

