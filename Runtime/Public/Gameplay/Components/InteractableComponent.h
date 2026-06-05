#pragma once

#include <string>

namespace eng::runtime {

    enum class InteractionType
    {
        None,
        Use,
        Pickup,
        Talk,
        Inspect,
        Open,
        Activate
    };

    inline std::string InteractionTypeToString(InteractionType type)
    {
        switch (type)
        {
            case InteractionType::None: return "None";
            case InteractionType::Use: return "Use";
            case InteractionType::Pickup: return "Pickup";
            case InteractionType::Talk: return "Talk";
            case InteractionType::Inspect: return "Inspect";
            case InteractionType::Open: return "Open";
            case InteractionType::Activate: return "Activate";
            default: return "Unknown";
        }
    }

    struct InteractableComponent
    {
        std::string PromptText = "Interact";
        bool Enabled = true;
        float InteractionRadius = 2.0f;
        InteractionType Type = InteractionType::Use;
    };

} // namespace eng::runtime
