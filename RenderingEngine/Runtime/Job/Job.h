#pragma once
#include <functional>
#include <cstdint>
#include "core/types/Result.h"

namespace eng::runtime {

    /**
     * @brief One unit of work that the JobSystem can schedule.
     *
     * The payload is a `std::function<void()>` to keep things simple.
     * In a performance‑critical build the payload can be replaced by a
     * templated functor or a raw function pointer + void* user data.
     */
    struct Job {
        using Callback = std::function<void()>;

        Callback  task;                     // The work to execute
        uint32_t  priority{ 0 };              // Optional priority (lower = higher priority)
        // For future extensions: job dependencies, parent/child counters, etc.
    };

} // namespace eng::runtime
