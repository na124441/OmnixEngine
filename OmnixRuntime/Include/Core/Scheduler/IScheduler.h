#pragma once

#include <functional>

namespace eng::core {

    class IScheduler {
    public:
        virtual ~IScheduler() = default;
        virtual void Initialize() = 0;
        virtual void Shutdown() = 0;

        // Schedules a task for execution.
        virtual void Execute(std::function<void()>&& task) = 0;
    };

} // namespace eng::core
