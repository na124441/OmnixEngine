#pragma once
#include "Core/Threading/ThreadUtils.h"

namespace eng {

    using eng::core::GetHardwareThreadCount;
    using eng::core::RecommendedWorkerCount;
    using eng::core::ScopedThread;

    namespace Runtime {
        using eng::core::GetHardwareThreadCount;
        using eng::core::RecommendedWorkerCount;
    }

} // namespace eng

namespace eng::runtime {

    namespace Runtime {
        using eng::core::GetHardwareThreadCount;
        using eng::core::RecommendedWorkerCount;
    }

} // namespace eng::runtime
