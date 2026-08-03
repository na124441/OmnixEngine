#pragma once

#include "Core/Engine/RuntimeState.h"
#include "Gameplay/Interaction/InteractionPromptData.h"
#include <functional>
#include <memory>

// Forward declarations for subsystem interfaces
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
    class LayerStack;
    class ModuleManager;
    class TimeManager;
    struct FrameTiming;
    enum class FrameStage : uint8_t;
}

namespace eng::runtime {
    class IAssetManager;
    class ISceneManager;
    class IECSWorld;
    class AssetRegistry;
    class GameMode;
    class GameplayEventBus;
    class AudioSystem;
    class GameplaySaveSystem;
    class ServiceRegistry;
    class PluginManager;
    class ConfigSystem;
    class EventBus;
    class CVarSystem;
    class RuntimeConsole;
}

class InputManager;

namespace eng::core {

    struct RuntimeContext {
        RuntimeMode mode = RuntimeMode::Game;
        EditorSimulationState editorSimulationState = EditorSimulationState::Edit;
        eng::renderer::IRenderer* renderer = nullptr;
        eng::physics::PhysicsWorld* physicsWorld = nullptr;
        eng::runtime::IAssetManager* assets = nullptr;
        eng::runtime::AssetRegistry* assetRegistry = nullptr;
        eng::runtime::ISceneManager* scenes = nullptr;
        eng::core::IScheduler* scheduler = nullptr;
        eng::runtime::IECSWorld* ecs = nullptr;
        InputManager* input = nullptr;
        Omnix::EventManager* events = nullptr;
        eng::runtime::GameMode* gameMode = nullptr;
        eng::runtime::GameplayEventBus* gameplayEventBus = nullptr;
        eng::runtime::AudioSystem* audioSystem = nullptr;
        eng::runtime::GameplaySaveSystem* saveSystem = nullptr;
        Omnix::WorldManager* worldManager = nullptr;
        eng::core::ModuleManager* moduleManager = nullptr;
        eng::runtime::ServiceRegistry* serviceRegistry = nullptr;
        eng::runtime::PluginManager* pluginManager = nullptr;
        eng::runtime::ConfigSystem* configSystem = nullptr;
        eng::runtime::EventBus* eventBus = nullptr;
        eng::runtime::CVarSystem* cvarSystem = nullptr;
        eng::runtime::RuntimeConsole* runtimeConsole = nullptr;
        eng::core::TimeManager* timeManager = nullptr;
        eng::core::LayerStack* layerStack = nullptr;
        
        eng::runtime::InteractionPromptData interactionPrompt;
        std::function<std::unique_ptr<eng::runtime::IECSWorld>(std::unique_ptr<eng::runtime::IECSWorld>)> swapECS;

        const eng::core::FrameTiming* timing = nullptr;
        const eng::core::FrameStage* currentStage = nullptr;

        bool IsValid() const {
            return renderer != nullptr &&
                   assets != nullptr &&
                   scenes != nullptr &&
                   scheduler != nullptr &&
                   ecs != nullptr &&
                   input != nullptr &&
                   events != nullptr &&
                   timing != nullptr &&
                   currentStage != nullptr;
        }
    };

} // namespace eng::core
