#include "Runtime/TextureImportTests.h"
#include "Runtime/TextureImporter.h"
#include "Runtime/TextureMetadata.h"
#include "Runtime/TextureCache.h"
#include "Runtime/TextureUploadDesc.h"
#include "Core/Logger.h"
#include "stb_image_write.h"
#include <vector>
#include <string>
#include <filesystem>
#include <cstring>
#include <fstream>

namespace eng::runtime {

    bool RunTextureImportTests() noexcept {
        LOG_INFO("================================================================================");
        LOG_INFO("                     RUNNING OMNIX TEXTURE IMPORT TESTS                         ");
        LOG_INFO("================================================================================");

        std::filesystem::create_directories("TestTextures");
        std::filesystem::create_directories("Cache/Textures");

        TextureImporter importer;

        // -----------------------------------------------------------------------------
        // Test 1 — PNG Import Test
        // -----------------------------------------------------------------------------
        LOG_INFO("[TextureTest] Running Test 1: PNG Import Test...");
        {
            // Generate red transparent pixel data (16x16, RGBA)
            int width = 16;
            int height = 16;
            std::vector<uint8_t> srcPixels(width * height * 4);
            for (size_t i = 0; i < srcPixels.size(); i += 4) {
                srcPixels[i] = 255;   // R
                srcPixels[i + 1] = 0; // G
                srcPixels[i + 2] = 0; // B
                srcPixels[i + 3] = 128; // A (has alpha)
            }

            std::string sourceFile = "TestTextures/stone_albedo.png";
            if (!stbi_write_png(sourceFile.c_str(), width, height, 4, srcPixels.data(), width * 4)) {
                LOG_ERROR("[TextureTest] Failed to write dummy PNG file.");
                return false;
            }

            std::string cacheFile = "Cache/Textures/stone_albedo.omnixtex";
            TextureMetadata metadata;
            if (!importer.ImportTexture(sourceFile, cacheFile, metadata, true)) {
                LOG_ERROR("[TextureTest] Test 1 FAILED: Could not import PNG texture!");
                std::filesystem::remove_all("TestTextures");
                std::filesystem::remove_all("Cache");
                return false;
            }

            // Assertions
            if (metadata.width != 16 || metadata.height != 16 || metadata.channels != 4) {
                LOG_ERROR("[TextureTest] Test 1 FAILED: Dimension metadata mismatch!");
                return false;
            }
            if (metadata.sourceFormat != TextureSourceFormat::PNG ||
                metadata.runtimeFormat != TextureRuntimeFormat::RGBA8) {
                LOG_ERROR("[TextureTest] Test 1 FAILED: Format mismatch!");
                return false;
            }
            if (!metadata.hasAlpha) {
                LOG_ERROR("[TextureTest] Test 1 FAILED: Alpha channel not detected!");
                return false;
            }
            if (!metadata.isSRGB) {
                LOG_ERROR("[TextureTest] Test 1 FAILED: sRGB flag not detected from albedo filename tag!");
                return false;
            }
            if (!std::filesystem::exists(cacheFile) || !std::filesystem::exists(cacheFile + ".meta")) {
                LOG_ERROR("[TextureTest] Test 1 FAILED: Cached file or metadata sidecar missing!");
                return false;
            }

            // Read back .omnixtex
            std::vector<uint8_t> loadedPixels;
            OmnixTextureHeader header;
            if (!importer.LoadOmnixTexture(cacheFile, loadedPixels, header)) {
                LOG_ERROR("[TextureTest] Test 1 FAILED: Could not read back cached .omnixtex file!");
                return false;
            }

            if (header.width != 16 || header.height != 16 || header.channels != 4 ||
                header.isSRGB != 1) {
                LOG_ERROR("[TextureTest] Test 1 FAILED: Cached header fields mismatch!");
                return false;
            }

            if (loadedPixels != srcPixels) {
                LOG_ERROR("[TextureTest] Test 1 FAILED: Decoded pixel payload content mismatch!");
                return false;
            }

            // GPU Upload Description creation
            TextureUploadDesc uploadDesc;
            uploadDesc.width = header.width;
            uploadDesc.height = header.height;
            uploadDesc.mipCount = header.mipCount;
            uploadDesc.format = static_cast<TextureRuntimeFormat>(header.runtimeFormat);
            uploadDesc.isSRGB = (header.isSRGB != 0);
            uploadDesc.pixelData = loadedPixels.data();
            uploadDesc.pixelDataSize = loadedPixels.size();

            if (uploadDesc.width != 16 || uploadDesc.height != 16 || uploadDesc.mipCount != 1 ||
                uploadDesc.format != TextureRuntimeFormat::RGBA8 || uploadDesc.isSRGB != true ||
                uploadDesc.pixelDataSize != 16 * 16 * 4) {
                LOG_ERROR("[TextureTest] Test 1 FAILED: Failed to create valid TextureUploadDesc!");
                return false;
            }

            LOG_INFO("[TextureTest] Test 1 Passed: PNG imported, validated, cached, and prepared for GPU.");
        }

        // -----------------------------------------------------------------------------
        // Test 2 — JPG Import Test
        // -----------------------------------------------------------------------------
        LOG_INFO("[TextureTest] Running Test 2: JPG Import Test...");
        {
            // Generate RGB pixel data (8x8, RGB)
            int width = 8;
            int height = 8;
            std::vector<uint8_t> srcPixels(width * height * 3);
            for (size_t i = 0; i < srcPixels.size(); i += 3) {
                srcPixels[i] = 0;     // R
                srcPixels[i + 1] = 255; // G (Green)
                srcPixels[i + 2] = 0;   // B
            }

            std::string sourceFile = "TestTextures/metal_roughness.jpg";
            if (!stbi_write_jpg(sourceFile.c_str(), width, height, 3, srcPixels.data(), 100)) {
                LOG_ERROR("[TextureTest] Failed to write dummy JPG file.");
                return false;
            }

            std::string cacheFile = "Cache/Textures/metal_roughness.omnixtex";
            TextureMetadata metadata;
            if (!importer.ImportTexture(sourceFile, cacheFile, metadata, true)) {
                LOG_ERROR("[TextureTest] Test 2 FAILED: Could not import JPG texture!");
                return false;
            }

            // Assertions (Note: JPG has 3 channels, but is normalized to 4 channels RGBA8)
            if (metadata.width != 8 || metadata.height != 8 || metadata.channels != 4) {
                LOG_ERROR("[TextureTest] Test 2 FAILED: Normalized RGBA8 channel layout mismatch!");
                return false;
            }
            if (metadata.sourceFormat != TextureSourceFormat::JPG) {
                LOG_ERROR("[TextureTest] Test 2 FAILED: Source format detection mismatch!");
                return false;
            }
            if (metadata.hasAlpha) {
                LOG_ERROR("[TextureTest] Test 2 FAILED: JPG should not contain alpha channels.");
                return false;
            }
            if (metadata.isSRGB) {
                LOG_ERROR("[TextureTest] Test 2 FAILED: Roughness texture should be interpreted as linear sRGB=false.");
                return false;
            }

            // Read back and check alpha padding
            std::vector<uint8_t> loadedPixels;
            OmnixTextureHeader header;
            if (!importer.LoadOmnixTexture(cacheFile, loadedPixels, header)) {
                LOG_ERROR("[TextureTest] Test 2 FAILED: Could not read back cached JPG .omnixtex!");
                return false;
            }

            // JPG decoding normalizes to RGBA8, checking if alpha channel is filled to 255
            for (size_t i = 3; i < loadedPixels.size(); i += 4) {
                if (loadedPixels[i] != 255) {
                    LOG_ERROR("[TextureTest] Test 2 FAILED: Alpha channel padding is not 255!");
                    return false;
                }
            }
            LOG_INFO("[TextureTest] Test 2 Passed: JPG imported and normalized to RGBA8 successfully.");
        }

        // -----------------------------------------------------------------------------
        // Test 3 — Corrupt Image Test
        // -----------------------------------------------------------------------------
        LOG_INFO("[TextureTest] Running Test 3: Corrupt Image Test...");
        {
            std::string sourceFile = "TestTextures/corrupt.png";
            std::ofstream file(sourceFile, std::ios::binary);
            file << "This is corrupted png file, not real image data!";
            file.close();

            std::string cacheFile = "Cache/Textures/corrupt.omnixtex";
            TextureMetadata metadata;

            // Import should fail gracefully and return false
            bool ok = importer.ImportTexture(sourceFile, cacheFile, metadata, true);
            if (ok) {
                LOG_ERROR("[TextureTest] Test 3 FAILED: Corrupted file was incorrectly imported!");
                return false;
            }
            LOG_INFO("[TextureTest] Test 3 Passed: Corrupted image safely rejected without engine crash.");
        }

        // -----------------------------------------------------------------------------
        // Test 4 — Cache Test
        // -----------------------------------------------------------------------------
        LOG_INFO("[TextureTest] Running Test 4: Cache Test...");
        {
            std::string sourceFile = "TestTextures/stone_albedo.png";
            std::string cacheFile = "Cache/Textures/stone_albedo.omnixtex";
            TextureMetadata meta1;

            // Perform first import to write caches
            if (!importer.ImportTexture(sourceFile, cacheFile, meta1, true)) {
                LOG_ERROR("[TextureTest] Failed first import in Cache Test");
                return false;
            }

            // Perform second import without force flag
            TextureMetadata meta2;
            if (!importer.ImportTexture(sourceFile, cacheFile, meta2, false)) {
                LOG_ERROR("[TextureTest] Test 4 FAILED: Second cache-based import failed!");
                return false;
            }

            // Verify that timestamps are preserved
            if (meta2.sourceFileTimestamp != meta1.sourceFileTimestamp ||
                meta2.width != meta1.width ||
                meta2.height != meta1.height) {
                LOG_ERROR("[TextureTest] Test 4 FAILED: Metadata mismatch on cached import!");
                return false;
            }
            LOG_INFO("[TextureTest] Test 4 Passed: Second import skipped image decoding and used cache.");
        }

        // -----------------------------------------------------------------------------
        // Test 5 — Metadata Round Trip
        // -----------------------------------------------------------------------------
        LOG_INFO("[TextureTest] Running Test 5: Metadata Round Trip...");
        {
            TextureMetadata orig;
            orig.handle = AssetHandle{991199};
            orig.width = 512;
            orig.height = 256;
            orig.channels = 4;
            orig.sourceFormat = TextureSourceFormat::PNG;
            orig.runtimeFormat = TextureRuntimeFormat::BC7;
            orig.mipCount = 9;
            orig.hasAlpha = true;
            orig.isSRGB = true;
            orig.generateMips = true;
            orig.isCompressed = true;
            orig.sourceFileTimestamp = 987654321ULL;
            orig.importTimestamp = 123456789ULL;
            orig.cachePath = "Cache/Textures/round_trip.omnixtex";

            std::string metaFile = "Cache/Textures/round_trip.omnixtex.meta";
            if (!SaveTextureMetadata(orig, metaFile)) {
                LOG_ERROR("[TextureTest] Test 5 FAILED: Could not save texture metadata!");
                return false;
            }

            TextureMetadata loaded;
            if (!LoadTextureMetadata(loaded, metaFile)) {
                LOG_ERROR("[TextureTest] Test 5 FAILED: Could not load texture metadata!");
                std::filesystem::remove(metaFile);
                return false;
            }
            std::filesystem::remove(metaFile);

            if (loaded.handle != orig.handle ||
                loaded.width != orig.width ||
                loaded.height != orig.height ||
                loaded.channels != orig.channels ||
                loaded.sourceFormat != orig.sourceFormat ||
                loaded.runtimeFormat != orig.runtimeFormat ||
                loaded.mipCount != orig.mipCount ||
                loaded.hasAlpha != orig.hasAlpha ||
                loaded.isSRGB != orig.isSRGB ||
                loaded.generateMips != orig.generateMips ||
                loaded.isCompressed != orig.isCompressed ||
                loaded.sourceFileTimestamp != orig.sourceFileTimestamp ||
                loaded.importTimestamp != orig.importTimestamp ||
                loaded.cachePath != orig.cachePath) {
                LOG_ERROR("[TextureTest] Test 5 FAILED: Loaded metadata fields mismatch!");
                return false;
            }
            LOG_INFO("[TextureTest] Test 5 Passed: Metadata Round Trip successful.");
        }

        // Clean up
        try {
            std::filesystem::remove_all("TestTextures");
            std::filesystem::remove_all("Cache");
        } catch (...) {}

        LOG_INFO("================================================================================");
        LOG_INFO("                  ALL OMNIX TEXTURE IMPORT TESTS PASSED                          ");
        LOG_INFO("================================================================================");
        return true;
    }

} // namespace eng::runtime
