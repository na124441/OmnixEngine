#pragma once

#include "ECS/ECSComponents.h"
#include "Runtime/Public/Gameplay/StateObjects/SimpleStateComponent.h"
#include <string>
#include <vector>
#include <unordered_map>

namespace eng::runtime {

    struct CheckpointSnapshot
    {
        std::string CheckpointID;
        std::string CheckpointName;

        TransformComponent PlayerTransform;

        std::string ActiveObjectiveID;
        std::vector<std::string> CompletedObjectives;

        // Map from stable Activation ID of state object to its runtime state
        std::unordered_map<std::string, SimpleObjectState> SimpleObjectStates;

        float ElapsedGameplayTime = 0.0f;
        bool Valid = false;
    };

} // namespace eng::runtime
