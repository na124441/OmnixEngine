#pragma once

#include <cstdint>

namespace Omnix
{
    struct WorldSettingsBlock
    {
        float gravityX = 0.0f;
        float gravityY = -9.81f;
        float gravityZ = 0.0f;

        float worldTimeScale = 1.0f;

        uint32_t enableStreaming = 1;
        uint32_t enablePhysics = 1;
        uint32_t enableNavigation = 0;
        uint32_t enableAudio = 1;

        uint32_t reserved[16] = {0};
    };
}
