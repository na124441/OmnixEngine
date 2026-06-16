#include "Core/pch.h"
#include "Frustum.h"

namespace eng::renderer {

static glm::vec4 NormalizePlane(const glm::vec4& p)
{
    float len = glm::length(glm::vec3(p));
    return p / len;
}

GPUFrustum ExtractFrustumPlanes(const glm::mat4& viewProjection)
{
    GPUFrustum frustum{};
    const glm::mat4& m = viewProjection;
    frustum.planes[Left] = NormalizePlane(glm::vec4(
        m[0][3] + m[0][0],
        m[1][3] + m[1][0],
        m[2][3] + m[2][0],
        m[3][3] + m[3][0]
    ));
    frustum.planes[Right] = NormalizePlane(glm::vec4(
        m[0][3] - m[0][0],
        m[1][3] - m[1][0],
        m[2][3] - m[2][0],
        m[3][3] - m[3][0]
    ));
    frustum.planes[Bottom] = NormalizePlane(glm::vec4(
        m[0][3] + m[0][1],
        m[1][3] + m[1][1],
        m[2][3] + m[2][1],
        m[3][3] + m[3][1]
    ));
    frustum.planes[Top] = NormalizePlane(glm::vec4(
        m[0][3] - m[0][1],
        m[1][3] - m[1][1],
        m[2][3] - m[2][1],
        m[3][3] - m[3][1]
    ));
    frustum.planes[Near] = NormalizePlane(glm::vec4(
        m[0][3] + m[0][2],
        m[1][3] + m[1][2],
        m[2][3] + m[2][2],
        m[3][3] + m[3][2]
    ));
    frustum.planes[Far] = NormalizePlane(glm::vec4(
        m[0][3] - m[0][2],
        m[1][3] - m[1][2],
        m[2][3] - m[2][2],
        m[3][3] - m[3][2]
    ));
    return frustum;
}

bool SphereInFrustumCPU(
    const GPUFrustum& frustum,
    const glm::vec3& center,
    float radius)
{
    for (int i = 0; i < 6; ++i)
    {
        glm::vec4 plane = frustum.planes[i];
        float d = glm::dot(glm::vec3(plane), center) + plane.w;
        if (d < -radius)
        {
            return false;
        }
    }
    return true;
}

} // namespace eng::renderer
