#pragma once
#include "runtime/render_scene/RenderObject.h"
#include <vector>

namespace eng::runtime {

    /**
     * @brief Result of all visibility determination steps.
     *
     * The three vectors are **sorted** (by material for opaque, by depth for
     * transparent) by the `VisibilitySystem` after culling.
     */
    struct VisibleSet {
        std::vector<RenderObject*> opaque;
        std::vector<RenderObject*> transparent;
        std::vector<RenderObject*> shadowCasters; // subset of opaque that casts shadows

        void Clear() {
            opaque.clear();
            transparent.clear();
            shadowCasters.clear();
        }
    };

    class VisibilitySystem {
    public:
        VisibilitySystem() = default;
        ~VisibilitySystem() = default;

        /**
         * @brief Main entry point – runs frustum culling, LOD, and optional occlusion
         *        culling.
         *
         * The function writes the final `VisibleSet` into `outSet`.  All temporary
         * buffers are allocated from the per‑frame `LinearAllocator`.
         */
        Result CullAndSort(const RenderScene& scene,
            const RenderView& view,
            LinearAllocator& allocator,
            VisibleSet& outSet);
    private:
        FrustumCulling  m_Frustum;
        LODSystem       m_LOD;
        OcclusionCulling m_Occlusion;   // optional – can be disabled at runtime
    };

} // namespace eng::runtime
