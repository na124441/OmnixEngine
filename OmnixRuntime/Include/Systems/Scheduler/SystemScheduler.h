#pragma once
#include "Core/Scheduler/SystemScheduler.h"

namespace eng::runtime {

    class SystemScheduler : public eng::core::SystemScheduler {
    public:
        using eng::core::SystemScheduler::SystemScheduler;
    };

} // namespace eng::runtime
