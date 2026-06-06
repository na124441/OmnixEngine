#pragma once

#include <cstdint>

namespace Omnix
{
    struct WorldEntryPoint
    {
        char entryZonePath[256] = "";

        float spawnPositionX = 0.0f;
        float spawnPositionY = 0.0f;
        float spawnPositionZ = 0.0f;

        float spawnRotationPitch = 0.0f;
        float spawnRotationYaw = 0.0f;
        float spawnRotationRoll = 0.0f;

        char spawnTag[64] = "";

        uint32_t reserved[16] = {0};
    };
}
