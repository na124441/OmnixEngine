#pragma once
#include <glm/glm.hpp>
#include "Core/types/Handle.h"
#include "Runtime/world/Entity.h"

namespace eng::runtime {

    struct RenderObject {
        Entity          entity;
        eng::core::Handle<uint32_t> mesh;
        eng::core::Handle<uint32_t> material;
        glm::mat4       worldMatrix;
        float           distanceSq;
        uint32_t        lodLevel;
    };

} // namespace eng::runtime
