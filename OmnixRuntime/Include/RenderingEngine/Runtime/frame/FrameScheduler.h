#pragma once
#include <cstdint>
#include "Core/types/Result.h"
#include "Runtime/world/Transform.h"
#include "RHI/RHIDevice.h"

#include "runtime/frame/FrameContext.h"

namespace eng::runtime {

    class FrameScheduler {
    public:
        explicit FrameScheduler(eng::rhi::Device* device) {}
        ~FrameScheduler() = default;

        eng::core::Result BeginFrame(FrameContext& ctx, double time) { return eng::core::Result(); }
        eng::core::Result EndFrame(FrameContext& ctx) { return eng::core::Result(); }
        void SetTargetFPS(uint32_t fps) {}
    };

} // namespace eng::runtime
