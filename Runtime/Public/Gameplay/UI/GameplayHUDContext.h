#pragma once

#include "Runtime/Public/Gameplay/GameState.h"
#include "Runtime/Public/Gameplay/PlayerStateComponent.h"
#include "Runtime/Public/Gameplay/Interaction/InteractionPromptData.h"
#include "Runtime/Public/Gameplay/Objectives/ObjectiveSystem.h"

namespace eng::runtime {

    class IECSWorld;
    class GameplayEventBus;

    struct GameplayHUDContext
    {
        const GameState* GameState = nullptr;
        const PlayerStateComponent* PlayerState = nullptr;
        const InteractionPromptData* InteractionPrompt = nullptr;
        const ObjectiveSystem* Objectives = nullptr;
        IECSWorld* ECS = nullptr;
        GameplayEventBus* EventBus = nullptr;
    };

} // namespace eng::runtime
