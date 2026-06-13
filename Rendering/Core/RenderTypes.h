#pragma once
#include <glm/glm.hpp>
#include <vulkan/vulkan.h>

#include "RenderingEngine/Core/types/Vertex.h"

namespace eng::renderer {

    struct PushConstants {
        glm::mat4 model;
    };

} // namespace eng::renderer
