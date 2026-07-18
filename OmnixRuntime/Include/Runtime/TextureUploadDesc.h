#pragma once
#include "Runtime/TextureFormat.h"
#include <cstdint>

struct TextureUploadDesc
{
    uint32_t width = 0;
    uint32_t height = 0;
    uint32_t mipCount = 1;

    TextureRuntimeFormat format = TextureRuntimeFormat::RGBA8;

    const void* pixelData = nullptr;
    uint64_t pixelDataSize = 0;

    bool isSRGB = false;
};
