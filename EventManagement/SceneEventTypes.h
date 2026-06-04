// ============================================================================
// SceneEventTypes.h - Scene Lifecycle Events
// ============================================================================

#pragma once

#include "GameEvent.h"
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

} // namespace Omnix
