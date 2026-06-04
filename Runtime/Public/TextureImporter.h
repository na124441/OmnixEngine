#pragma once
#include "Runtime/Public/TextureFormat.h"
#include "Runtime/Public/TextureMetadata.h"
#include "Runtime/Public/OmnixTextureFormat.h"
#include <string>
#include <vector>

namespace eng::runtime {

    struct RawTextureData
    {
        uint32_t width = 0;
        uint32_t height = 0;
        uint32_t channels = 4;

        TextureSourceFormat sourceFormat = TextureSourceFormat::Unknown;
        TextureRuntimeFormat runtimeFormat = TextureRuntimeFormat::RGBA8;

        std::vector<uint8_t> pixels;
    };

    class TextureImporter
    {
    public:
        TextureImporter() = default;
        ~TextureImporter() = default;

        /**
         * @brief Core import function. Decodes, validates, generates metadata, and writes the cache file.
         * Skips decode if cached file exists and is up to date.
         * @return true if successful, false otherwise.
         */
        bool ImportTexture(const std::string& sourcePath, const std::string& cachePath, TextureMetadata& outMetadata, bool forceReimport = false);

        /**
         * @brief Decodes a source image file (PNG/JPG) using stb_image and normalizes it to RGBA8.
         */
        bool DecodeSourceImage(const std::string& sourcePath, RawTextureData& outData);

        /**
         * @brief Validates decoded texture metrics (dimensions, sizes).
         */
        bool ValidateTexture(const RawTextureData& texture);

        /**
         * @brief Encodes raw normalized pixel data into an Omnix texture file (.omnixtex).
         */
        bool SaveAsOmnixTexture(const RawTextureData& rawData, const TextureMetadata& metadata, const std::string& cachePath);

        /**
         * @brief Deserializes an .omnixtex file from disk.
         */
        bool LoadOmnixTexture(const std::string& cachePath, std::vector<uint8_t>& outPixelData, OmnixTextureHeader& outHeader);

        /**
         * @brief Deserializes an .omnixtex file from memory buffer.
         */
        bool LoadOmnixTextureFromMemory(const uint8_t* data, size_t size, std::vector<uint8_t>& outPixelData, OmnixTextureHeader& outHeader);
    };

} // namespace eng::runtime
