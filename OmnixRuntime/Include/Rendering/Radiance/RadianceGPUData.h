#pragma once

#include <glm/glm.hpp>
#include <cstdint>
#include "Rendering/Core/RenderTargetManager.h"

namespace Omnix::Radiance
{
    struct RadianceSurfaceTargets
    {
        eng::renderer::RenderTargetHandle depth;
        eng::renderer::RenderTargetHandle baseColor;
        eng::renderer::RenderTargetHandle normalRoughness;
        eng::renderer::RenderTargetHandle materialProperties;
        eng::renderer::RenderTargetHandle emissiveShadingModel;
        eng::renderer::RenderTargetHandle objectID;
        eng::renderer::RenderTargetHandle velocity;
    };

    inline bool ValidateRadianceSurface(const RadianceSurfaceTargets& targets, eng::renderer::RenderTargetManager& manager) {
        return manager.IsValid(targets.depth) &&
               manager.IsValid(targets.baseColor) &&
               manager.IsValid(targets.normalRoughness) &&
               manager.IsValid(targets.materialProperties) &&
               manager.IsValid(targets.emissiveShadingModel) &&
               manager.IsValid(targets.objectID) &&
               manager.IsValid(targets.velocity);
    }
    struct alignas(16) RadianceFrameUBO
    {
        glm::mat4 view;
        glm::mat4 projection;
        glm::mat4 inverseView;
        glm::mat4 inverseProjection;
        glm::mat4 inverseViewProjection;

        glm::vec4 cameraPosition; 
        // xyz = camera position, w = time

        glm::vec4 viewportSize;
        // x = width, y = height, z = 1/width, w = 1/height

        glm::vec4 skyTopColorIntensity;
        // rgb = top color, a = intensity

        glm::vec4 skyHorizonColorBlend;
        // rgb = horizon color, a = horizon blend

        glm::vec4 skyGroundColorIntensity;
        // rgb = ground color, a = ground intensity

        glm::vec4 sunDirectionIntensity;
        // xyz = sun direction, w = intensity

        glm::vec4 sunColorAngularSize;
        // rgb = sun color, a = angular size

        glm::vec4 exposureSettings;
        // x = manual exposure
        // y = min exposure
        // z = max exposure
        // w = auto exposure enabled, 0 or 1

        glm::uvec4 renderFlags;
        // x = tone mapping mode
        // y = cinematic preview
        // z/w reserved
    };
}
