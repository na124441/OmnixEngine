#pragma once
#include "runtime/render_scene/RenderObject.h"
#include "runtime/visibility/VisibleSet.h"
#include "core/memory/LinearAllocator.h"
#include "rhi/RHI.h"

namespace eng::runtime {

    /**
     * @class OcclusionCulling
     *
     * *Optional* GPU‑based occlusion test.  It works by:
     *   1. Recording a low‑resolution depth‑only pass for the whole view.
     *   2. Issuing a set of **occlusion queries** for each candidate bounding box.
     *   3. After the GPU finishes, reading back the query results and discarding
     *      objects that were not visible.
     *
     * The class does **not** own any RHI resources; it receives a `RHI::CommandList*`
     * from the pass that executes the occlusion queries.
     *
     * When the hardware/driver does not support `RHIQuery` (or the `Result` from
     * the query creation failed), the class simply returns success without doing anything.
     */
    class OcclusionCulling {
    public:
        OcclusionCulling() = default;
        ~OcclusionCulling() = default;

        /**
         * @brief Perform occlusion queries for the given objects.
         *
         * @param cmd          Command list where the queries will be recorded.
         * @param view         Current view (needed for projection matrix).
         * @param visibleIn    Input set after frustum culling.
         * @param allocator    Frame‑local allocator for temporary storage.
         * @param outVisible   Output set (objects that survived occlusion test).
         *
         * @return Result::Success (or Failure if a query could not be created).
         */
        Result Run(RHI::CommandList* cmd,
            const RenderView& view,
            const VisibleSet& visibleIn,
            LinearAllocator& allocator,
            VisibleSet& outVisible);
    };

} // namespace eng::runtime
