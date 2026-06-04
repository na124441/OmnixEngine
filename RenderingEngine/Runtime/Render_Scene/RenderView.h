#pragma once
#include <glm/glm.hpp>
#include "Core/containers/Array.h"

namespace eng::runtime {

    struct RenderView {
        glm::mat4       viewMatrix;
        glm::mat4       projMatrix;
        glm::vec4       viewport;
        eng::core::Array<struct RenderObject*> visibleObjects;
    };

} // namespace eng::runtime
