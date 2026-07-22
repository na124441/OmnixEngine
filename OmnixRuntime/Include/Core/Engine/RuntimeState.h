#pragma once

#include <cstdint>

namespace eng::core {

    enum class RuntimeState : uint8_t {
        Uninitialized = 0,
        Initializing,
        Running,
        Suspended,
        ShuttingDown
    };

    enum class RuntimeMode : uint8_t {
        Game = 0,
        Editor
    };

    enum class EditorSimulationState : uint8_t {
        Edit = 0,
        Play,
        Pause,
        Step
    };

} // namespace eng::core
