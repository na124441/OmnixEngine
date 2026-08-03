#pragma once
#include <glm/glm.hpp>
#include "Core/types/Handle.h"
#include "RHI/RHI.h"

namespace eng::runtime {

    struct MaterialInstance {
        eng::core::Handle<uint32_t> materialId;
        glm::vec4       albedoColor;
        eng::rhi::TextureHandle albedoMap;
        eng::rhi::TextureHandle normalMap;
        eng::rhi::TextureHandle metalRoughnessMap;
    };

} // namespace eng::runtime
