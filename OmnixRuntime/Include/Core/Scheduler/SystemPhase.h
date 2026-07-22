#pragma once

#include <cstdint>

namespace eng::core {

    enum class SystemPhase : uint8_t {
        PreUpdate = 0,
        Update = 1,
        PostUpdate = 2,
        PreRender = 3,
        Render = 4,
        PostRender = 5
    };

} // namespace eng::core
