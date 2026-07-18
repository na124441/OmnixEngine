#pragma once

#include "Gameplay/GameSessionState.h"
#include <string>
#include <vector>
#include <unordered_map>

namespace eng::runtime {

    struct GameState
    {
        std::string ActiveSceneName;
        GameSessionState SessionState = GameSessionState::None;

        std::string ActiveObjectiveID;
        std::vector<std::string> CompletedObjectives;

        float ElapsedGameplayTime = 0.0f;
        std::string CurrentCheckpointID;

        std::unordered_map<std::string, bool> GameplayFlags;

        void Reset() {
            ActiveSceneName.clear();
            SessionState = GameSessionState::None;
            ActiveObjectiveID.clear();
            CompletedObjectives.clear();
            ElapsedGameplayTime = 0.0f;
            CurrentCheckpointID.clear();
            GameplayFlags.clear();
        }
    };

} // namespace eng::runtime
