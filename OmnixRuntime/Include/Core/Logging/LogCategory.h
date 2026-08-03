#pragma once

namespace eng::logging {

    enum class LogCategory {
        Runtime,
        Renderer,
        ECS,
        Physics,
        Assets,
        Scheduler,
        Memory,
        Networking
    };

    inline const char* LogCategoryToString(LogCategory category) {
        switch (category) {
            case LogCategory::Runtime:    return "Runtime";
            case LogCategory::Renderer:   return "Renderer";
            case LogCategory::ECS:        return "ECS";
            case LogCategory::Physics:    return "Physics";
            case LogCategory::Assets:     return "Assets";
            case LogCategory::Scheduler:  return "Scheduler";
            case LogCategory::Memory:     return "Memory";
            case LogCategory::Networking: return "Networking";
            default:                      return "Unknown";
        }
    }

} // namespace eng::logging
