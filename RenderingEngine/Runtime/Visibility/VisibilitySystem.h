#pragma once
#include "Runtime/world/World.h"
#include "Runtime/render_scene/RenderView.h"
#include "RenderingEngine/Core/memory/LinearAllocator.h"
#include "Core/types/Result.h"

namespace eng::runtime {

    struct VisibleSet {
        // Placeholder
    };

    class VisibilitySystem {
    public:
        VisibilitySystem() = default;
        ~VisibilitySystem() = default;

        template<typename CameraType>
        eng::core::Result CullAndSort(
            const RenderScene& scene,
            const CameraType& camera,
            eng::core::LinearAllocator<>& allocator,
            VisibleSet& outVisibleSet) 
        {
            return eng::core::Result();
        }
    };

} // namespace eng::runtime
