#pragma once

#include <string>
#include <cstdint>

#ifndef OMNIX_ENTITY_DEFINED
#define OMNIX_ENTITY_DEFINED
using Entity = std::uint32_t;
constexpr Entity INVALID_ENTITY = 0;
#endif

namespace eng::runtime {

    enum class GameplayEventType
    {
        None,
        TriggerEnter,
        TriggerExit,
        Interaction,
        ObjectiveStarted,
        ObjectiveCompleted,
        CheckpointReached,
        LevelCompleted,
        PlayerDied,
        ZoneEnter,
        ZoneExit
    };

    struct GameplayEvent
    {
        GameplayEventType Type = GameplayEventType::None;

        Entity Source = INVALID_ENTITY;
        Entity Target = INVALID_ENTITY;

        std::string ObjectiveID;
        std::string CheckpointID;

        uint64_t ZoneUUIDHigh = 0;
        uint64_t ZoneUUIDLow = 0;

        float Timestamp = 0.0f;
        uint64_t SequenceID = 0;
    };

    inline std::string ToString(GameplayEventType type)
    {
        switch (type)
        {
            case GameplayEventType::None: return "None";
            case GameplayEventType::TriggerEnter: return "TriggerEnter";
            case GameplayEventType::TriggerExit: return "TriggerExit";
            case GameplayEventType::Interaction: return "Interaction";
            case GameplayEventType::ObjectiveStarted: return "ObjectiveStarted";
            case GameplayEventType::ObjectiveCompleted: return "ObjectiveCompleted";
            case GameplayEventType::CheckpointReached: return "CheckpointReached";
            case GameplayEventType::LevelCompleted: return "LevelCompleted";
            case GameplayEventType::PlayerDied: return "PlayerDied";
            case GameplayEventType::ZoneEnter: return "ZoneEnter";
            case GameplayEventType::ZoneExit: return "ZoneExit";
            default: return "Unknown";
        }
    }

} // namespace eng::runtime
