#pragma once

#include "Scene/Vector3.h"
#include <string>

namespace eng::runtime {

    enum class DoorOpenMode
    {
        Instant,
        Smooth
    };

    inline std::string DoorOpenModeToString(DoorOpenMode mode)
    {
        switch (mode)
        {
            case DoorOpenMode::Instant: return "Instant";
            case DoorOpenMode::Smooth:  return "Smooth";
        }
        return "Unknown";
    }

    struct DoorComponent
    {
        Vector3 ClosedPosition;
        Vector3 OpenOffset = Vector3(0.0f, 3.0f, 0.0f);
        float OpenSpeed = 2.0f;
        DoorOpenMode OpenMode = DoorOpenMode::Instant;
        bool IsOpen = false;
        bool IsOpening = false; // Runtime state
    };

} // namespace eng::runtime
