#pragma once
#include "FrameGraph.h"
#include "core/log/Log.h"

namespace eng::runtime {

    /**
     * @class GraphExecutor
     *
     * Walks the sorted pass list, obtains a fresh `RHI::CommandList` from the
     * `RHI::Device`, invokes each pass’s lambda, and finally submits the list.
     *
     * It also inserts **explicit per‑pass barriers** that the compiler
     * (via `InsertImplicitBarriers`) has pre‑computed and stored inside each
     * `PassNode` (or inside a separate barrier list attached to the graph).
     *
     * After submission, the executor returns a `FenceHandle` to the caller
     * (`EngineLoop`) so that the next frame can wait for the GPU.
     */
    class GraphExecutor {
    public:
        explicit GraphExecutor(RHI::Device* rhi);
        ~GraphExecutor();

        Result Execute(const FrameContext& ctx,
            const std::vector<PassNode*>& orderedPasses,
            const std::vector<ResourceNode*>& orderedResources);
    };

} // namespace eng::runtime
