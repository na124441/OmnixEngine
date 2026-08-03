#pragma once
#include "RHI/RHI.h"

namespace eng::rhi {

    /**
     * @class Device
     * @brief Abstract interface for the GPU device.
     */
    class Device {
    public:
        virtual ~Device() = default;

        virtual void WaitIdle() = 0;
    };

} // namespace eng::rhi
