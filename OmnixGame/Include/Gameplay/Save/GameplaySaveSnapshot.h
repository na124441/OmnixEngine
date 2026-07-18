#pragma once

#include "ECS/ECSComponents.h"
#include "Gameplay/StateObjects/SimpleStateComponent.h"
#include <string>
#include <vector>
#include <unordered_map>

namespace eng::runtime {

    struct GameplaySaveSnapshot
    {
        uint32_t Version = 1;

        std::string SceneName;

        TransformComponent PlayerTransform;
        float PlayerHealth = 100.0f;
        bool PlayerAlive = true;

        std::string ActiveObjectiveID;
        std::vector<std::string> CompletedObjectives;

        std::string CheckpointID;

        std::unordered_map<std::string, bool> InteractableActivationStates;
        std::unordered_map<std::string, SimpleObjectState> SimpleObjectStates;

        uint64_t Checksum = 0;
        bool Valid = false;
    };

} // namespace eng::runtime
