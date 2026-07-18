#pragma once

#include "Gameplay/GameState.h"
#include "Gameplay/PlayerStateComponent.h"
#include "Gameplay/Interaction/InteractionPromptData.h"
#include "Gameplay/Objectives/ObjectiveSystem.h"

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
