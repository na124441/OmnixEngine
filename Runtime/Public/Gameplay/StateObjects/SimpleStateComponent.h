#pragma once

#include <string>

namespace eng::runtime {

    enum class SimpleObjectState
    {
        Inactive,
        Active,
        Completed,
        Locked,
        Unlocked
    };

    inline std::string SimpleObjectStateToString(SimpleObjectState state)
    {
        switch (state)
        {
            case SimpleObjectState::Inactive:  return "Inactive";
            case SimpleObjectState::Active:    return "Active";
            case SimpleObjectState::Completed: return "Completed";
            case SimpleObjectState::Locked:    return "Locked";
            case SimpleObjectState::Unlocked:  return "Unlocked";
        }
        return "Unknown";
    }

    struct SimpleStateComponent
    {
        SimpleObjectState InitialState = SimpleObjectState::Inactive;
        SimpleObjectState CurrentState = SimpleObjectState::Inactive;
        bool ResetOnPlay = true;

        void Reset()
        {
            CurrentState = InitialState;
        }
    };

} // namespace eng::runtime
