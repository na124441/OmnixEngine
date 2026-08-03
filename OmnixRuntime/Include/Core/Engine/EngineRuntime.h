#pragma once

#include "Core/Engine/RuntimeState.h"
#include "Runtime/RuntimeContext.h"
#include "Core/Engine/FrameStage.h"
#include "Core/Engine/FrameTiming.h"
#include "Core/Engine/LayerStack.h"
#include <memory>
#include <atomic>
#include <thread>
#include "Core/Platform/Thread.h"

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

namespace eng::core {
    class IScheduler;
    class ModuleManager;
    class TimeManager;
}

namespace eng::runtime {
    class IAssetManager;
    class AssetRegistry;
    class ISceneManager;
    class IECSWorld;
    class GameplayEventBus;
    class AudioSystem;
    class GameplaySaveSystem;
    class EditorLayer;
    class ServiceRegistry;
    class PluginManager;
    class ConfigSystem;
    class EventBus;
    class CVarSystem;
    class RuntimeConsole;
}

class InputManager;

namespace eng::core {

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
        const eng::runtime::RuntimeContext& GetContext() const { return m_Context; }

    private:
        std::atomic<RuntimeState> m_State{ RuntimeState::Uninitialized };
        eng::runtime::RuntimeContext m_Context;

        // Subsystems owned by the central runtime
        std::unique_ptr<eng::renderer::IRenderer> m_Renderer;
        std::unique_ptr<eng::physics::PhysicsWorld> m_PhysicsWorld;
        std::unique_ptr<eng::runtime::IAssetManager> m_Assets;
        std::unique_ptr<eng::runtime::AssetRegistry> m_AssetRegistry;
        std::unique_ptr<eng::runtime::ISceneManager> m_Scenes;
        std::unique_ptr<eng::core::IScheduler> m_Scheduler;
        std::unique_ptr<eng::runtime::IECSWorld> m_ECS;
        std::unique_ptr<::InputManager> m_Input;
        std::unique_ptr<Omnix::EventManager> m_EventManager;
        std::unique_ptr<eng::runtime::GameplayEventBus> m_GameplayEventBus;
        std::unique_ptr<eng::runtime::AudioSystem> m_AudioSystem;
        std::unique_ptr<eng::runtime::GameplaySaveSystem> m_GameplaySaveSystem;
        std::unique_ptr<Omnix::WorldManager> m_WorldManager;
        std::unique_ptr<ComponentSchemaRegistry> m_SchemaRegistry;
        std::unique_ptr<eng::runtime::EditorLayer> m_Editor;

        std::unique_ptr<eng::core::ModuleManager> m_ModuleManager;
        std::unique_ptr<eng::runtime::ServiceRegistry> m_ServiceRegistry;
        std::unique_ptr<eng::runtime::PluginManager> m_PluginManager;
        std::unique_ptr<eng::runtime::ConfigSystem> m_ConfigSystem;
        std::unique_ptr<eng::runtime::EventBus> m_EventBus;
        std::unique_ptr<eng::runtime::CVarSystem> m_CVarSystem;
        std::unique_ptr<eng::runtime::RuntimeConsole> m_RuntimeConsole;
        std::unique_ptr<eng::core::TimeManager> m_TimeManager;
        std::unique_ptr<eng::core::LayerStack> m_LayerStack;

        // Managed CLI input thread
        eng::platform::Thread m_InputThread;
        std::atomic<bool> m_InputThreadRunning{ false };
        void InputThreadWorker();

        // Temporal loop variables
        FrameTiming m_Timing;
        FrameStage m_CurrentStage = FrameStage::FrameEnd;

        // Diagnostic timers
        double m_StartupTimeMs = 0.0;
    };

} // namespace eng::core
