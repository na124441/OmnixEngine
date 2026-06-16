#pragma once
#include <glm/glm.hpp>
#include "Rendering/GPUScene/GPUVisibilityTypes.h"

namespace eng::renderer {

enum FrustumPlaneIndex
{
    Left = 0,
    Right = 1,
    Bottom = 2,
    Top = 3,
    Near = 4,
    Far = 5
};

GPUFrustum ExtractFrustumPlanes(const glm::mat4& viewProjection);
bool SphereInFrustumCPU(const GPUFrustum& frustum, const glm::vec3& center, float radius);

} // namespace eng::renderer
