#pragma once

#include "Runtime/Public/RuntimeState.h"
#include "Runtime/Public/RuntimeContext.h"
#include "Runtime/Public/FrameStage.h"
#include "Runtime/Public/FrameTiming.h"
#include <memory>
#include <atomic>
#include <thread>

// Forward declarations
class ComponentSchemaRegistry;
namespace Omnix {
    class EventManager;
    class WorldManager;
}

namespace eng::renderer {
    class IRenderer;
}

namespace eng::physics {
    class PhysicsWorld;
}

namespace eng::runtime {
    class IAssetManager;
    class ISceneManager;
    class IScheduler;
    class IECSWorld;
    class EditorLayer;
    class AudioSystem;
    class GameplaySaveSystem;
}

class InputManager;

namespace eng::runtime {

    class EngineRuntime {
    public:
        EngineRuntime();
        ~EngineRuntime();

        // Non-copyable and non-movable
        EngineRuntime(const EngineRuntime&) = delete;
        EngineRuntime& operator=(const EngineRuntime&) = delete;
        EngineRuntime(EngineRuntime&&) = delete;
        EngineRuntime& operator=(EngineRuntime&&) = delete;

        [[nodiscard]] bool Initialize(int argc = 0, char* argv[] = nullptr);
        void Run();
        void Shutdown();
        std::unique_ptr<eng::runtime::IECSWorld> SetECS(std::unique_ptr<eng::runtime::IECSWorld> ecs);

        RuntimeState GetState() const { return m_State.load(std::memory_order_relaxed); }
        bool IsRunning() const { return GetState() == RuntimeState::Running; }
        const RuntimeContext& GetContext() const { return m_Context; }

    private:
        std::atomic<RuntimeState> m_State{ RuntimeState::Uninitialized };
        RuntimeContext m_Context;

        // Subsystems owned by the central runtime
        std::unique_ptr<eng::renderer::IRenderer> m_Renderer;
        std::unique_ptr<eng::physics::PhysicsWorld> m_PhysicsWorld;
        std::unique_ptr<eng::runtime::IAssetManager> m_Assets;
        std::unique_ptr<eng::runtime::AssetRegistry> m_AssetRegistry;
        std::unique_ptr<eng::runtime::ISceneManager> m_Scenes;
        std::unique_ptr<eng::runtime::IScheduler> m_Scheduler;
        std::unique_ptr<eng::runtime::IECSWorld> m_ECS;
        std::unique_ptr<InputManager> m_Input;
        std::unique_ptr<Omnix::EventManager> m_EventManager;
        std::unique_ptr<GameplayEventBus> m_GameplayEventBus;
        std::unique_ptr<AudioSystem> m_AudioSystem;
        std::unique_ptr<GameplaySaveSystem> m_GameplaySaveSystem;
        std::unique_ptr<Omnix::WorldManager> m_WorldManager;
        std::unique_ptr<ComponentSchemaRegistry> m_SchemaRegistry;
        std::unique_ptr<EditorLayer> m_Editor;

        // Managed CLI input thread
        std::thread m_InputThread;
        std::atomic<bool> m_InputThreadRunning{ false };
        void InputThreadWorker();

        // Temporal loop variables
        FrameTiming m_Timing;
        FrameStage m_CurrentStage = FrameStage::FrameEnd;

        // Diagnostic timers
        double m_StartupTimeMs = 0.0;
    };

} // namespace eng::runtime
