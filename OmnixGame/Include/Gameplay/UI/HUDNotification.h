#pragma once

#include <string>

namespace eng::runtime {

    struct HUDNotification
    {
        std::string Text;
        float Duration = 2.0f;
        float TimeRemaining = 0.0f;
    };

} // namespace eng::runtime
