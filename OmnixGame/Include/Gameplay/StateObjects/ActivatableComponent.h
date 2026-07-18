#pragma once

#include <string>

namespace eng::runtime {

    struct ActivatableComponent
    {
        std::string ActivationID;
        std::string TargetActivationID;

        bool RequiresUnlocked = false;
        bool OneShot = true;
        bool HasActivated = false;
    };

} // namespace eng::runtime
