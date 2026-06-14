#pragma once

#include <glm/glm.hpp>
#include <cstdint>

namespace Omnix::Radiance
{
    enum class ToneMappingMode : uint32_t
    {
        None = 0,
        Reinhard = 1,
        ACES = 2,
        Filmic = 3
    };

    struct SkySettings
    {
        glm::vec3 skyTopColor = glm::vec3(0.20f, 0.45f, 0.85f);
        float skyIntensity = 1.0f;

        glm::vec3 horizonColor = glm::vec3(0.65f, 0.80f, 0.95f);
        float horizonBlend = 1.5f;

        glm::vec3 groundColor = glm::vec3(0.20f, 0.22f, 0.25f);
        float groundIntensity = 0.35f;
    };

    struct SunSettings
    {
        glm::vec3 direction = glm::normalize(glm::vec3(-0.4f, -1.0f, -0.25f));
        float intensity = 3.0f;

        glm::vec3 color = glm::vec3(1.0f, 0.94f, 0.82f);
        float angularSize = 0.015f;
    };

    struct ShadowSettings
    {
        uint32_t resolution = 2048;

        float strength = 1.0f;
        float constantBias = 0.003f;
        float slopeBias = 0.015f;
        float normalBias = 0.0f;

        uint32_t pcfKernelSize = 3;
        float softness = 1.0f;
    };

    struct ExposureSettings
    {
        bool autoExposure = false;

        float manualExposure = 1.0f;
        float minExposure = 0.25f;
        float maxExposure = 4.0f;
        float adaptationSpeed = 2.0f;

        ToneMappingMode toneMappingMode = ToneMappingMode::ACES;
    };

    struct RadianceSettings
    {
        SkySettings sky;
        SunSettings sun;
        ShadowSettings shadows;
        ExposureSettings exposure;

        bool cinematicPreview = false;
        bool showGrid = true;
        float gridOpacity = 0.35f;
        float gridFadeDistance = 120.0f;
    };
}
