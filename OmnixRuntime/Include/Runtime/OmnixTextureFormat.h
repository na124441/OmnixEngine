#pragma once
#include "Runtime/FileHeader.h"
#include <cstdint>

#pragma pack(push, 1)

struct MipDataBlock
{
    uint32_t mipIndex = 0;
    uint64_t offset = 0;
    uint64_t size = 0;
};

struct OmnixTextureHeader
{
    FileHeader file;

    uint32_t width = 0;
    uint32_t height = 0;
    uint32_t channels = 0;

    uint32_t mipCount = 0;
    uint32_t runtimeFormat = 0;
    uint32_t isSRGB = 0;
    uint32_t isCompressed = 0;

    uint64_t pixelDataOffset = 0;
    uint64_t pixelDataSize = 0;
};

#pragma pack(pop)

constexpr char MAGIC_TEX[8] = {'O', 'M', 'X', 'T', 'E', 'X', '\0', '\0'};
constexpr uint32_t OMNIX_TEXTURE_VERSION_MAJOR = 1;
constexpr uint32_t OMNIX_TEXTURE_VERSION_MINOR = 0;
