#pragma once
#include <glm/glm.hpp>
#include <vulkan/vulkan.h>

#include "RenderingEngine/Core/types/Vertex.h"

namespace eng::renderer {

    struct PushConstants {
        glm::mat4 model;
    };

    struct ViewportOverlaySettings
    {
        bool showGrid = true;
        bool showSelectionOutline = true;
        bool showColliders = false;
        bool showBounds = false;
        bool showLightGizmos = false;
        bool showPhysicsDebug = false;
        bool showGizmos = true;
        bool showLightVolumes = false;
        bool showLabels = true;
    };

} // namespace eng::renderer
