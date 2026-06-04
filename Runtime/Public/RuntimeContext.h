#pragma once

#include "Runtime/Public/RuntimeState.h"
#include <functional>
#include <memory>

// Forward declarations for subsystem interfaces
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
    class AssetRegistry;
    enum class FrameStage : uint8_t;
    struct FrameTiming;
}

class InputManager;

namespace eng::runtime {

    struct RuntimeContext {
        RuntimeMode mode = RuntimeMode::Game;
        EditorSimulationState editorSimulationState = EditorSimulationState::Edit;
        eng::renderer::IRenderer* renderer = nullptr;
        eng::physics::PhysicsWorld* physicsWorld = nullptr;
        eng::runtime::IAssetManager* assets = nullptr;
        eng::runtime::AssetRegistry* assetRegistry = nullptr;
        eng::runtime::ISceneManager* scenes = nullptr;
        eng::runtime::IScheduler* scheduler = nullptr;
        eng::runtime::IECSWorld* ecs = nullptr;
        InputManager* input = nullptr;
        std::function<std::unique_ptr<eng::runtime::IECSWorld>(std::unique_ptr<eng::runtime::IECSWorld>)> swapECS;

        const eng::runtime::FrameTiming* timing = nullptr;
        const eng::runtime::FrameStage* currentStage = nullptr;

        bool IsValid() const {
            return renderer != nullptr &&
                   assets != nullptr &&
                   scenes != nullptr &&
                   scheduler != nullptr &&
                   ecs != nullptr &&
                   input != nullptr &&
                   timing != nullptr &&
                   currentStage != nullptr;
        }
    };

} // namespace eng::runtime
