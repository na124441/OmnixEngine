#pragma once
#include "Core/Scheduler/IScheduler.h"

namespace eng::runtime {

    class IScheduler : public eng::core::IScheduler {
    public:
        using eng::core::IScheduler::IScheduler;
    };

} // namespace eng::runtime
