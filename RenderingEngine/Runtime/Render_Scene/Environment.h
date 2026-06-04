#pragma once
#include <glm/glm.hpp>
#include "RHI/RHI.h"

namespace eng::runtime {

    struct Environment {
        eng::rhi::TextureHandle skybox;
        eng::rhi::TextureHandle irradianceMap;
        eng::rhi::TextureHandle prefilterMap;
        eng::rhi::TextureHandle brdfLUT;
    };

} // namespace eng::runtime
