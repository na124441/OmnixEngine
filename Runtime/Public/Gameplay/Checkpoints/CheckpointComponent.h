#pragma once

#include <string>

namespace eng::runtime {

    struct CheckpointComponent
    {
        std::string CheckpointID;
        std::string CheckpointName;

        bool ActivateOnTriggerEnter = true;
        bool OneShot = false;
        bool HasActivated = false;
    };

} // namespace eng::runtime
