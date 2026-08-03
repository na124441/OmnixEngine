#pragma once
#include "runtime/render_scene/RenderObject.h"
#include "runtime/visibility/VisibleSet.h"
#include "core/memory/LinearAllocator.h"
#include <glm/glm.hpp>

namespace eng::runtime {

    /**
     * @class LODSystem
     *
     * Adjusts the LOD level of each visible object according to its distance
     * from the active camera.  The system works on a `VisibleSet` after
     * frustum culling.
     *
     * Policy (example):
     *   - distance² <  10 m² → LOD 0 (highest)
     *   - distance² < 100 m² → LOD 1
     *   - distance² < 500 m² → LOD 2
     *   - otherwise        → LOD 3 (lowest)
     *
     * The distances and LOD thresholds are configurable via a simple struct.
     */
    class LODSystem {
    public:
        struct Settings {
            float lodDistances[4] = { 10.0f, 100.0f, 500.0f, 2000.0f };
        };

        explicit LODSystem(const Settings& s = Settings()) : m_Settings(s) {}
        ~LODSystem() = default;

        /**
         * @brief Update LOD level for each object in the visible set.
         *
         * This function **modifies the objects in‑place**, therefore the
         * `VisibleSet` must contain mutable pointers.
         */
        void Process(VisibleSet& visible, const glm::vec3& cameraPos) const;

    private:
        Settings m_Settings;
    };

} // namespace eng::runtime
