#pragma once

#include <string>

namespace eng::runtime {

    struct AudioClip
    {
        std::string Path;
        std::string Name;
        bool Loaded = false;
        float Duration = 0.0f;
    };

} // namespace eng::runtime
