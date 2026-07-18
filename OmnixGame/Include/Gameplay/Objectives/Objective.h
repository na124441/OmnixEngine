#pragma once

#include <string>

namespace eng::runtime {

    enum class ObjectiveState
    {
        Inactive,
        Active,
        Completed,
        Failed
    };

    inline std::string ObjectiveStateToString(ObjectiveState state)
    {
        switch (state)
        {
            case ObjectiveState::Inactive: return "Inactive";
            case ObjectiveState::Active: return "Active";
            case ObjectiveState::Completed: return "Completed";
            case ObjectiveState::Failed: return "Failed";
            default: return "Unknown";
        }
    }

    struct Objective
    {
        std::string ID;
        std::string Title;
        std::string Description;
        ObjectiveState State = ObjectiveState::Inactive;
        bool Repeatable = false;
    };

} // namespace eng::runtime
