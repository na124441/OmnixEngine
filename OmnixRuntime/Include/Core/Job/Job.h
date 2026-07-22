#pragma once

#include <functional>
#include <cstdint>

namespace eng::core {

    /**
     * @brief One unit of work that the JobSystem can schedule.
     */
    struct Job {
        using Callback = std::function<void()>;

        Callback  task;                     // The work to execute
        uint32_t  priority{ 0 };              // Optional priority (lower = higher priority)
    };

} // namespace eng::core
