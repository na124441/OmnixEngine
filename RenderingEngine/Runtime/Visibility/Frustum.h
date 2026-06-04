#pragma once
#include "runtime/render_scene/RenderObject.h"
#include "runtime/render_scene/RenderView.h"
#include "runtime/visibility/VisibleSet.h"
#include "core/memory/LinearAllocator.h"
#include <glm/glm.hpp>

namespace eng::runtime {

    /**
     * @class FrustumCulling
     *
     * Performs classic plane‑based frustum culling.
     *
     * Input:
     *   - A list of `RenderObject`s (dense array).
     *   - The current `RenderView` (provides view & projection matrices).
     *
     * Output:
     *   - Populates a `VisibleSet` (three separate vectors for opaque,
     *     transparent, and shadow‑casting objects).  The vectors store **pointers**
     *     to the original `RenderObject`s, not copies.
     *
     * All temporary vectors are allocated from a per‑frame `LinearAllocator`
     * (passed in via `FrameContext::resources`).
     */
    class FrustumCulling {
    public:
        FrustumCulling() = default;
        ~FrustumCulling() = default;

        /**
         * @brief Cull against the view frustum.
         *
         * @param view          The camera view (contains view/proj matrix).
         * @param objects       Array of objects to test.
         * @param allocator     Frame‑local LinearAllocator (for the vectors inside VisibleSet).
         * @param outVisible    Output structure.
         *
         * @return Result::Success on success.
         */
        Result Cull(const RenderView& view,
            const Array<RenderObject>& objects,
            LinearAllocator& allocator,
            VisibleSet& outVisible);
    private:
        // Helper to extract six frustum planes from view+proj matrix.
        void ExtractPlanes(const glm::mat4& viewProj,
            glm::vec4 planes[6]) const;

        bool TestAABB(const glm::vec4 planes[6],
            const glm::vec3& min,
            const glm::vec3& max) const;
    };

} // namespace eng::runtime
