#include "Runtime/TextureImporter.h"
#include "Runtime/TextureCache.h"
#include "Runtime/BinaryReader.h"
#include "Runtime/BinaryWriter.h"
#include "Runtime/AssetRegistry.h"
#include "Runtime/AssetType.h"
#include "Core/Logger.h"
#include "stb_image.h"
#include <filesystem>
#include <algorithm>
#include <cctype>
#include <chrono>

namespace eng::runtime {

    static uint64_t GetFileLastWriteTime(const std::string& filepath) noexcept {
        try {
            if (!std::filesystem::exists(filepath)) {
                return 0;
            }
            auto ftime = std::filesystem::last_write_time(filepath);
            return std::chrono::duration_cast<std::chrono::seconds>(ftime.time_since_epoch()).count();
        } catch (...) {
            return 0;
        }
    }

    static bool IsSRGBFromFilename(const std::string& filename) noexcept {
        std::string lower = filename;
        std::transform(lower.begin(), lower.end(), lower.begin(), [](unsigned char c) {
            return std::tolower(c);
        });

        if (lower.find("_albedo") != std::string::npos ||
            lower.find("_basecolor") != std::string::npos ||
            lower.find("_diffuse") != std::string::npos) {
            return true;
        }
        return false;
    }

    static bool DetectAlphaChannel(const std::vector<uint8_t>& pixels) noexcept {
        for (size_t i = 3; i < pixels.size(); i += 4) {
            if (pixels[i] < 255) {
                return true;
            }
        }
        return false;
    }

    static bool IsPowerOfTwo(uint32_t n) noexcept {
        return n && !(n & (n - 1));
    }

    bool TextureImporter::ImportTexture(const std::string& sourcePath, const std::string& cachePath, TextureMetadata& outMetadata, bool forceReimport) {
        TextureCache cache;
        if (!forceReimport && cache.IsCachedAndUpToDate(sourcePath, cachePath)) {
            // Load sidecar metadata
            if (LoadTextureMetadata(outMetadata, cachePath + ".meta")) {
                LOG_INFO("[TextureImporter] Using cached texture: %s", cachePath.c_str());
                return true;
            }
        }

        LOG_INFO("[TextureImporter] Importing texture: %s -> %s", sourcePath.c_str(), cachePath.c_str());

        RawTextureData rawData;
        if (!DecodeSourceImage(sourcePath, rawData)) {
            LOG_ERROR("[TextureImporter] Failed to decode source image: %s", sourcePath.c_str());
            return false;
        }

        if (!ValidateTexture(rawData)) {
            LOG_ERROR("[TextureImporter] Validation failed for: %s", sourcePath.c_str());
            return false;
        }

        // Detect Metadata properties
        outMetadata.handle = GenerateAssetUUID(sourcePath, AssetType::Texture);
        outMetadata.width = rawData.width;
        outMetadata.height = rawData.height;
        outMetadata.channels = rawData.channels;
        outMetadata.sourceFormat = rawData.sourceFormat;
        outMetadata.runtimeFormat = rawData.runtimeFormat;
        outMetadata.mipCount = 1; // v0.2 mip count placeholder
        outMetadata.hasAlpha = DetectAlphaChannel(rawData.pixels);
        outMetadata.isSRGB = IsSRGBFromFilename(sourcePath);
        outMetadata.generateMips = false;
        outMetadata.isCompressed = false;
        outMetadata.sourceFileTimestamp = GetFileLastWriteTime(sourcePath);
        outMetadata.cachePath = cachePath;

        // Save binary cached texture
        std::filesystem::create_directories(std::filesystem::path(cachePath).parent_path());
        if (!SaveAsOmnixTexture(rawData, outMetadata, cachePath)) {
            LOG_ERROR("[TextureImporter] Failed to save Omnix Texture to %s", cachePath.c_str());
            return false;
        }

        // Update import timestamp from output cache file write time
        outMetadata.importTimestamp = GetFileLastWriteTime(cachePath);

        // Save metadata sidecar
        if (!SaveTextureMetadata(outMetadata, cachePath + ".meta")) {
            LOG_ERROR("[TextureImporter] Failed to save sidecar metadata for: %s", cachePath.c_str());
            return false;
        }

        return true;
    }

    bool TextureImporter::DecodeSourceImage(const std::string& sourcePath, RawTextureData& outData) {
        if (!std::filesystem::exists(sourcePath)) {
            LOG_ERROR("[TextureImporter] Source image file does not exist: %s", sourcePath.c_str());
            return false;
        }

        // Detect Format by extension
        std::string ext = std::filesystem::path(sourcePath).extension().string();
        std::transform(ext.begin(), ext.end(), ext.begin(), [](unsigned char c) {
            return std::tolower(c);
        });

        if (ext == ".png") {
            outData.sourceFormat = TextureSourceFormat::PNG;
        } else if (ext == ".jpg" || ext == ".jpeg") {
            outData.sourceFormat = TextureSourceFormat::JPG;
        } else {
            outData.sourceFormat = TextureSourceFormat::Unknown;
        }

        int width = 0;
        int height = 0;
        int channels = 0;

        // Force RGBA8 (4 channels)
        unsigned char* pixels = stbi_load(sourcePath.c_str(), &width, &height, &channels, 4);
        if (!pixels) {
            LOG_ERROR("[TextureImporter] stb_image decoding failed for %s. Reason: %s", sourcePath.c_str(), stbi_failure_reason());
            return false;
        }

        outData.width = static_cast<uint32_t>(width);
        outData.height = static_cast<uint32_t>(height);
        outData.channels = 4;
        outData.runtimeFormat = TextureRuntimeFormat::RGBA8;

        outData.pixels.assign(pixels, pixels + (width * height * 4));
        stbi_image_free(pixels);

        return true;
    }

    bool TextureImporter::ValidateTexture(const RawTextureData& texture) {
        constexpr uint32_t MIN_TEXTURE_SIZE = 1;
        constexpr uint32_t MAX_TEXTURE_SIZE = 16384;

        if (texture.width < MIN_TEXTURE_SIZE || texture.height < MIN_TEXTURE_SIZE) {
            LOG_ERROR("[TextureImporter] ERROR: Invalid texture dimensions: %dx%d (Min size is %d)",
                      texture.width, texture.height, MIN_TEXTURE_SIZE);
            return false;
        }

        if (texture.width > MAX_TEXTURE_SIZE || texture.height > MAX_TEXTURE_SIZE) {
            LOG_ERROR("[TextureImporter] ERROR: Invalid texture dimensions: %dx%d (Max size is %d)",
                      texture.width, texture.height, MAX_TEXTURE_SIZE);
            return false;
        }

        if (texture.pixels.empty()) {
            LOG_ERROR("[TextureImporter] ERROR: Pixel buffer is empty.");
            return false;
        }

        // Warn on non-power-of-two
        if (!IsPowerOfTwo(texture.width) || !IsPowerOfTwo(texture.height)) {
            LOG_WARN("[TextureImporter] Warning: texture is not power-of-two. Dimensions: %dx%d",
                      texture.width, texture.height);
        }

        return true;
    }

    bool TextureImporter::SaveAsOmnixTexture(const RawTextureData& rawData, const TextureMetadata& metadata, const std::string& cachePath) {
        BinaryWriter writer;
        writer.BeginFile(MAGIC_TEX, OMNIX_TEXTURE_VERSION_MAJOR, OMNIX_TEXTURE_VERSION_MINOR);

        // Write header fields
        writer.WriteU32(metadata.width);
        writer.WriteU32(metadata.height);
        writer.WriteU32(metadata.channels);
        writer.WriteU32(metadata.mipCount);
        writer.WriteU32(static_cast<uint32_t>(metadata.runtimeFormat));
        writer.WriteU32(metadata.isSRGB ? 1 : 0);
        writer.WriteU32(static_cast<uint32_t>(TextureCompressionType::None));

        // MipDataBlock offset calculation
        // Header: FileHeader (32) + 7 * uint32_t fields (28) + 2 * uint64_t offset/size (16) = 76 bytes
        // MipDataBlock array: mipCount * sizeof(MipDataBlock) = 1 * 20 = 20 bytes
        // Total = 96 bytes offset for pixel data
        uint64_t pixelOffset = 96;
        uint64_t pixelSize = rawData.pixels.size();
        writer.WriteU64(pixelOffset);
        writer.WriteU64(pixelSize);

        // Write MipDataBlocks
        MipDataBlock mip;
        mip.mipIndex = 0;
        mip.offset = pixelOffset;
        mip.size = pixelSize;
        writer.WriteBytes(reinterpret_cast<const uint8_t*>(&mip), sizeof(MipDataBlock));

        // Write Pixel data
        writer.WriteBytes(rawData.pixels.data(), rawData.pixels.size());

        return writer.SaveToFile(cachePath);
    }

    bool TextureImporter::LoadOmnixTexture(const std::string& cachePath, std::vector<uint8_t>& outPixelData, OmnixTextureHeader& outHeader) {
        BinaryReader reader;
        if (!reader.LoadFromFile(cachePath)) {
            return false;
        }

        if (!reader.ValidateHeaderAndChecksum(MAGIC_TEX, OMNIX_TEXTURE_VERSION_MAJOR, OMNIX_TEXTURE_VERSION_MINOR)) {
            return false;
        }

        try {
            outHeader.width = reader.ReadU32();
            outHeader.height = reader.ReadU32();
            outHeader.channels = reader.ReadU32();
            outHeader.mipCount = reader.ReadU32();
            outHeader.runtimeFormat = reader.ReadU32();
            outHeader.isSRGB = reader.ReadU32();
            outHeader.isCompressed = reader.ReadU32();
            outHeader.pixelDataOffset = reader.ReadU64();
            outHeader.pixelDataSize = reader.ReadU64();

            // Read MipDataBlock
            MipDataBlock mip;
            reader.ReadBytes(reinterpret_cast<uint8_t*>(&mip), sizeof(MipDataBlock));

            // Seek to pixel data offset
            reader.Seek(outHeader.pixelDataOffset);

            outPixelData.resize(outHeader.pixelDataSize);
            reader.ReadBytes(outPixelData.data(), outHeader.pixelDataSize);

        } catch (const std::exception&) {
            return false;
        }

        return true;
    }

    bool TextureImporter::LoadOmnixTextureFromMemory(const uint8_t* data, size_t size, std::vector<uint8_t>& outPixelData, OmnixTextureHeader& outHeader) {
        BinaryReader reader;
        if (!reader.LoadFromMemory(data, size)) {
            return false;
        }

        if (!reader.ValidateHeaderAndChecksum(MAGIC_TEX, OMNIX_TEXTURE_VERSION_MAJOR, OMNIX_TEXTURE_VERSION_MINOR)) {
            return false;
        }

        try {
            outHeader.width = reader.ReadU32();
            outHeader.height = reader.ReadU32();
            outHeader.channels = reader.ReadU32();
            outHeader.mipCount = reader.ReadU32();
            outHeader.runtimeFormat = reader.ReadU32();
            outHeader.isSRGB = reader.ReadU32();
            outHeader.isCompressed = reader.ReadU32();
            outHeader.pixelDataOffset = reader.ReadU64();
            outHeader.pixelDataSize = reader.ReadU64();

            // Read MipDataBlock
            MipDataBlock mip;
            reader.ReadBytes(reinterpret_cast<uint8_t*>(&mip), sizeof(MipDataBlock));

            // Seek to pixel data offset
            reader.Seek(outHeader.pixelDataOffset);

            outPixelData.resize(outHeader.pixelDataSize);
            reader.ReadBytes(outPixelData.data(), outHeader.pixelDataSize);

        } catch (const std::exception&) {
            return false;
        }

        return true;
    }

} // namespace eng::runtime
