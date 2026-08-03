#pragma once

#include <string>

namespace eng::runtime {

    enum class ObjectiveCompletionMode
    {
        None,
        Interaction,
        TriggerEnter
    };

    inline std::string ObjectiveCompletionModeToString(ObjectiveCompletionMode mode)
    {
        switch (mode)
        {
            case ObjectiveCompletionMode::None: return "None";
            case ObjectiveCompletionMode::Interaction: return "Interaction";
            case ObjectiveCompletionMode::TriggerEnter: return "TriggerEnter";
            default: return "Unknown";
        }
    }

    struct ObjectiveComponent
    {
        std::string ObjectiveID;
        std::string Title;
        std::string Description;
        ObjectiveCompletionMode CompletionMode = ObjectiveCompletionMode::Interaction;
        bool StartsActive = true;
        bool Repeatable = false;
        bool Completed = false;
    };

} // namespace eng::runtime
