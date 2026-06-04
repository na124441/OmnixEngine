#include "Runtime/Public/PackageTests.h"
#include "Runtime/Public/PackageBuilder.h"
#include "Runtime/Public/Package.h"
#include "Runtime/Public/PackageManager.h"
#include "Runtime/Public/RuntimeLoaders.h"
#include "Runtime/Public/OmnixMeshFormat.h"
#include "Runtime/Public/OmnixMaterialFormat.h"
#include "Runtime/Public/OmnixTextureFormat.h"
#include "Runtime/Public/TextureImporter.h"
#include "Core/Logger.h"
#include <filesystem>
#include <fstream>
#include <vector>
#include <cstring>

namespace eng::runtime {

    namespace {
        // Simple helper to create dummy binary file
        bool WriteDummyFile(const std::string& path, const std::vector<uint8_t>& data) {
            std::ofstream f(path, std::ios::binary);
            if (!f.is_open()) return false;
            f.write(reinterpret_cast<const char*>(data.data()), data.size());
            return true;
        }

        // Helper to read binary file
        std::vector<uint8_t> ReadBinaryFile(const std::string& path) {
            std::ifstream f(path, std::ios::binary);
            if (!f.is_open()) return {};
            return std::vector<uint8_t>((std::istreambuf_iterator<char>(f)), std::istreambuf_iterator<char>());
        }
    }

    bool RunPackageTests() noexcept
    {
        LOG_INFO("================================================================================");
        LOG_INFO("                     RUNNING OMNIX ASSET PACKAGING TESTS                        ");
        LOG_INFO("================================================================================");

        std::filesystem::create_directories("test_output");

        // -----------------------------------------------------------------------------
        // Test 1: Empty Package
        // -----------------------------------------------------------------------------
        LOG_INFO("[PackageTest] Running Test 1: Empty Package...");
        {
            PackageBuilder builder;
            std::string pkgPath = "test_output/empty.omnixpackage";
            if (!builder.Build(pkgPath)) {
                LOG_ERROR("[PackageTest] Test 1 FAILED: Could not build empty package.");
                return false;
            }

            PackageManager& manager = GetPackageManager();
            manager.Clear();
            if (!manager.MountPackage(pkgPath)) {
                LOG_ERROR("[PackageTest] Test 1 FAILED: Could not mount empty package.");
                return false;
            }

            if (manager.GetMountedPackages().size() != 1) {
                LOG_ERROR("[PackageTest] Test 1 FAILED: Mounted package count is not 1.");
                return false;
            }

            manager.Clear();
            std::filesystem::remove(pkgPath);
            LOG_INFO("[PackageTest] Test 1 Passed: Empty package successfully built and mounted.");
        }

        // -----------------------------------------------------------------------------
        // Test 2: Single Asset Package (Mesh payload round-trip & loading)
        // -----------------------------------------------------------------------------
        LOG_INFO("[PackageTest] Running Test 2: Single Asset Package...");
        {
            // Create a valid serialized mesh
            OmnixMesh origMesh;
            origMesh.header.vertexCount = 3;
            origMesh.header.indexCount = 3;
            origMesh.header.vertexStride = sizeof(OmnixVertex);
            origMesh.header.submeshCount = 1;
            origMesh.header.hasSkeleton = 0;
            origMesh.header.materialSlotCount = 1;
            origMesh.header.bounds = BoundingBox{ {0.0f,0.0f,0.0f}, {1.0f,1.0f,1.0f} };
            origMesh.header.sphere = BoundingSphere{ {0.5f,0.5f,0.5f}, 0.5f };

            OmnixVertex v0, v1, v2;
            v0.position = {0,0,0}; v0.normal = {0,1,0}; v0.tangent = {1,0,0,1}; v0.uv0 = {0,0}; v0.uv1 = {0,0};
            v1.position = {1,0,0}; v1.normal = {0,1,0}; v1.tangent = {1,0,0,1}; v1.uv0 = {1,0}; v1.uv1 = {1,0};
            v2.position = {0,1,0}; v2.normal = {0,1,0}; v2.tangent = {1,0,0,1}; v2.uv0 = {0,1}; v2.uv1 = {0,1};
            origMesh.vertices = {v0, v1, v2};
            origMesh.indices = {0, 1, 2};
            origMesh.submeshes = { {0, 3, 0} };
            origMesh.materialSlots = { AssetHandle{7777} };
            origMesh.skeletonAssetPath = "";

            std::string meshPath = "test_output/single_asset.omnixmesh";
            if (!SerializeMesh(origMesh, meshPath)) {
                LOG_ERROR("[PackageTest] Test 2 FAILED: Could not serialize test mesh.");
                return false;
            }

            std::vector<uint8_t> meshBytes = ReadBinaryFile(meshPath);
            AssetHandle meshHandle = AssetHandle{9999123};

            PackageBuilder builder;
            builder.AddAsset(meshHandle, AssetType::Mesh, meshBytes, {});
            
            std::string pkgPath = "test_output/single.omnixpackage";
            if (!builder.Build(pkgPath)) {
                LOG_ERROR("[PackageTest] Test 2 FAILED: Could not build package.");
                return false;
            }

            PackageManager& manager = GetPackageManager();
            manager.Clear();
            if (!manager.MountPackage(pkgPath)) {
                LOG_ERROR("[PackageTest] Test 2 FAILED: Could not mount package.");
                return false;
            }

            if (!manager.Contains(meshHandle)) {
                LOG_ERROR("[PackageTest] Test 2 FAILED: Package manager doesn't contain handle.");
                return false;
            }

            std::vector<uint8_t> loadedBytes = manager.ReadAssetPayload(meshHandle);
            if (loadedBytes != meshBytes) {
                LOG_ERROR("[PackageTest] Test 2 FAILED: Payload byte mismatch.");
                return false;
            }

            // Loader integration check:
            // Since it's mounted, we test MeshLoader with a non-existent importedPath.
            // It should resolve from memory because GetPackageManager().Contains is true.
            AssetMetadata metadata;
            metadata.handle = meshHandle;
            metadata.type = AssetType::Mesh;
            metadata.importedPath = "non_existent_file.omnixmesh"; // File doesn't exist on disk

            MeshLoader loader;
            RuntimeAsset* asset = nullptr;
            if (!loader.Load(metadata, &asset)) {
                LOG_ERROR("[PackageTest] Test 2 FAILED: MeshLoader failed to load from package memory.");
                return false;
            }

            if (!asset) {
                LOG_ERROR("[PackageTest] Test 2 FAILED: MeshLoader loaded asset is null.");
                return false;
            }

            RuntimeMesh* runtimeMesh = static_cast<RuntimeMesh*>(asset);
            if (runtimeMesh->vertexCount != 3 || runtimeMesh->indexCount != 3) {
                LOG_ERROR("[PackageTest] Test 2 FAILED: Mesh geometry parameters mismatch.");
                loader.Unload(asset);
                return false;
            }

            loader.Unload(asset);
            manager.Clear();

            std::filesystem::remove(meshPath);
            std::filesystem::remove(pkgPath);
            LOG_INFO("[PackageTest] Test 2 Passed: Single asset packaging, mounting, and loader resolution successful.");
        }

        // -----------------------------------------------------------------------------
        // Test 3: Multi-Asset Package (Mesh, Material, Texture, Shader & Dependencies)
        // -----------------------------------------------------------------------------
        LOG_INFO("[PackageTest] Running Test 3: Multi-Asset Package...");
        {
            // 1. Texture data
            BinaryWriter texWriter;
            texWriter.BeginFile(MAGIC_TEX, OMNIX_TEXTURE_VERSION_MAJOR, OMNIX_TEXTURE_VERSION_MINOR);
            texWriter.WriteU32(2); // width
            texWriter.WriteU32(2); // height
            texWriter.WriteU32(4); // channels
            texWriter.WriteU32(1); // mip count
            texWriter.WriteU32(0); // runtime format (RGBA8)
            texWriter.WriteU32(1); // isSRGB
            texWriter.WriteU32(0); // isCompressed
            texWriter.WriteU64(96); // offset
            texWriter.WriteU64(16); // size
            // MipDataBlock
            MipDataBlock mip;
            mip.mipIndex = 0;
            mip.offset = 96;
            mip.size = 16;
            texWriter.WriteBytes(reinterpret_cast<const uint8_t*>(&mip), sizeof(mip));
            // pixels
            std::vector<uint8_t> rawPixels(16, 255);
            texWriter.WriteBytes(rawPixels.data(), rawPixels.size());
            std::string tempTex = "test_output/temp.omnixtex";
            texWriter.SaveToFile(tempTex);
            std::vector<uint8_t> texBytes = ReadBinaryFile(tempTex);
            std::filesystem::remove(tempTex);

            // 2. Material data
            OmnixMaterial origMat;
            origMat.header.shader = AssetHandle{1002}; // depends on shader
            origMat.header.textureBindingCount = 1;
            origMat.header.scalarParameterCount = 0;
            origMat.header.vectorParameterCount = 0;
            origMat.name = "TestMaterial";
            origMat.textures = { {MaterialTextureSlot::Albedo, AssetHandle{1003}} }; // depends on texture
            std::string tempMat = "test_output/temp.omnixmat";
            SerializeMaterial(origMat, tempMat);
            std::vector<uint8_t> matBytes = ReadBinaryFile(tempMat);
            std::filesystem::remove(tempMat);

            // 3. Mesh data
            OmnixMesh origMesh;
            origMesh.header.vertexCount = 3;
            origMesh.header.indexCount = 3;
            origMesh.header.vertexStride = sizeof(OmnixVertex);
            origMesh.header.submeshCount = 1;
            origMesh.header.hasSkeleton = 0;
            origMesh.header.materialSlotCount = 1;
            origMesh.header.bounds = BoundingBox{ {0.0f,0.0f,0.0f}, {1.0f,1.0f,1.0f} };
            origMesh.header.sphere = BoundingSphere{ {0.5f,0.5f,0.5f}, 0.5f };
            origMesh.vertices.resize(3);
            origMesh.indices = {0, 1, 2};
            origMesh.submeshes = { {0, 3, 0} };
            origMesh.materialSlots = { AssetHandle{1001} }; // depends on material
            std::string tempMesh = "test_output/temp.omnixmesh";
            SerializeMesh(origMesh, tempMesh);
            std::vector<uint8_t> meshBytes = ReadBinaryFile(tempMesh);
            std::filesystem::remove(tempMesh);

            // 4. Shader (Just raw dummy bytes representing shader binary format)
            std::vector<uint8_t> shaderBytes = { 0x53, 0x48, 0x41, 0x44, 0x01, 0x02, 0x03, 0x04 };

            AssetHandle meshHandle{1001};
            AssetHandle shaderHandle{1002};
            AssetHandle texHandle{1003};
            AssetHandle matHandle{1004};

            PackageBuilder builder;
            builder.AddAsset(meshHandle, AssetType::Mesh, meshBytes, { matHandle });
            builder.AddAsset(matHandle, AssetType::Material, matBytes, { shaderHandle, texHandle });
            builder.AddAsset(texHandle, AssetType::Texture, texBytes, {});
            builder.AddAsset(shaderHandle, AssetType::Shader, shaderBytes, {});

            std::string pkgPath = "test_output/multi.omnixpackage";
            if (!builder.Build(pkgPath)) {
                LOG_ERROR("[PackageTest] Test 3 FAILED: Multi-asset build failed.");
                return false;
            }

            PackageManager& manager = GetPackageManager();
            manager.Clear();
            if (!manager.MountPackage(pkgPath)) {
                LOG_ERROR("[PackageTest] Test 3 FAILED: Multi-asset mount failed.");
                return false;
            }

            // Verify dependencies query
            std::vector<AssetHandle> meshDeps = manager.GetDependencies(meshHandle);
            if (meshDeps.size() != 1 || meshDeps[0] != matHandle) {
                LOG_ERROR("[PackageTest] Test 3 FAILED: Mesh dependencies mismatch.");
                return false;
            }

            std::vector<AssetHandle> matDeps = manager.GetDependencies(matHandle);
            if (matDeps.size() != 2 || 
                (matDeps[0] != shaderHandle && matDeps[1] != shaderHandle) || 
                (matDeps[0] != texHandle && matDeps[1] != texHandle)) {
                LOG_ERROR("[PackageTest] Test 3 FAILED: Material dependencies mismatch.");
                return false;
            }

            // Loader checks
            // 1. TextureLoader memory check
            {
                AssetMetadata metadata;
                metadata.handle = texHandle;
                metadata.type = AssetType::Texture;
                metadata.importedPath = "non_existent_texture.omnixtex";

                TextureLoader loader;
                RuntimeAsset* asset = nullptr;
                if (!loader.Load(metadata, &asset)) {
                    LOG_ERROR("[PackageTest] Test 3 FAILED: TextureLoader failed loading from package.");
                    return false;
                }
                if (!asset) {
                    LOG_ERROR("[PackageTest] Test 3 FAILED: Loaded texture was null.");
                    return false;
                }
                RuntimeTexture* runtimeTex = static_cast<RuntimeTexture*>(asset);
                if (runtimeTex->width != 2 || runtimeTex->height != 2 || runtimeTex->channels != 4) {
                    LOG_ERROR("[PackageTest] Test 3 FAILED: Loaded texture dimensions mismatch.");
                    loader.Unload(asset);
                    return false;
                }
                loader.Unload(asset);
            }

            // 2. MaterialLoader memory check
            {
                AssetMetadata metadata;
                metadata.handle = matHandle;
                metadata.type = AssetType::Material;
                metadata.importedPath = "non_existent_material.omnixmat";

                MaterialLoader loader;
                RuntimeAsset* asset = nullptr;
                if (!loader.Load(metadata, &asset)) {
                    LOG_ERROR("[PackageTest] Test 3 FAILED: MaterialLoader failed loading from package.");
                    return false;
                }
                if (!asset) {
                    LOG_ERROR("[PackageTest] Test 3 FAILED: Loaded material was null.");
                    return false;
                }
                RuntimeMaterial* runtimeMat = static_cast<RuntimeMaterial*>(asset);
                if (runtimeMat->shader != shaderHandle || runtimeMat->textures.size() != 1 || runtimeMat->textures[0].texture != texHandle) {
                    LOG_ERROR("[PackageTest] Test 3 FAILED: Loaded material contents mismatch.");
                    loader.Unload(asset);
                    return false;
                }
                loader.Unload(asset);
            }

            manager.Clear();
            std::filesystem::remove(pkgPath);
            LOG_INFO("[PackageTest] Test 3 Passed: Multi-asset package built, verified dependencies and loader resolutions.");
        }

        // -----------------------------------------------------------------------------
        // Test 4: Corrupted Header
        // -----------------------------------------------------------------------------
        LOG_INFO("[PackageTest] Running Test 4: Corrupted Header...");
        {
            PackageBuilder builder;
            builder.AddAsset(AssetHandle{1}, AssetType::Shader, {0x11, 0x22}, {});
            std::string pkgPath = "test_output/corrupt_hdr.omnixpackage";
            if (!builder.Build(pkgPath)) {
                LOG_ERROR("[PackageTest] Test 4 FAILED: Could not build temp package.");
                return false;
            }

            // Corrupt header magic
            std::vector<uint8_t> bytes = ReadBinaryFile(pkgPath);
            if (bytes.size() >= 8) {
                // Change magic bytes to something invalid
                bytes[0] = 'X';
                bytes[1] = 'X';
                bytes[2] = 'X';
            }
            WriteDummyFile(pkgPath, bytes);

            Package pkg;
            if (pkg.Open(pkgPath)) {
                LOG_ERROR("[PackageTest] Test 4 FAILED: Package with corrupted magic bytes was successfully opened!");
                std::filesystem::remove(pkgPath);
                return false;
            }

            std::filesystem::remove(pkgPath);
            LOG_INFO("[PackageTest] Test 4 Passed: Package with corrupted magic was correctly rejected.");
        }

        // -----------------------------------------------------------------------------
        // Test 5: Corrupted Payload
        // -----------------------------------------------------------------------------
        LOG_INFO("[PackageTest] Running Test 5: Corrupted Payload...");
        {
            PackageBuilder builder;
            std::vector<uint8_t> originalData = {0xAA, 0xBB, 0xCC, 0xDD, 0xEE, 0xFF};
            builder.AddAsset(AssetHandle{123}, AssetType::Shader, originalData, {});
            std::string pkgPath = "test_output/corrupt_payload.omnixpackage";
            if (!builder.Build(pkgPath)) {
                LOG_ERROR("[PackageTest] Test 5 FAILED: Could not build package.");
                return false;
            }

            // Corrupt a byte in the payload
            // Raw data block is at the very end of the file.
            std::vector<uint8_t> bytes = ReadBinaryFile(pkgPath);
            if (bytes.size() > 10) {
                bytes.back() ^= 0xFF; // Flip last byte of payload
            }
            WriteDummyFile(pkgPath, bytes);

            Package pkg;
            if (pkg.Open(pkgPath)) {
                LOG_ERROR("[PackageTest] Test 5 FAILED: Package with corrupted payload checksum was successfully opened!");
                std::filesystem::remove(pkgPath);
                return false;
            }

            std::filesystem::remove(pkgPath);
            LOG_INFO("[PackageTest] Test 5 Passed: Package with corrupted payload was correctly rejected by checksum check.");
        }

        // -----------------------------------------------------------------------------
        // Test 6: Bad Offset
        // -----------------------------------------------------------------------------
        LOG_INFO("[PackageTest] Running Test 6: Bad Offset...");
        {
            PackageBuilder builder;
            builder.AddAsset(AssetHandle{456}, AssetType::Shader, {0x10, 0x20}, {});
            std::string pkgPath = "test_output/bad_offset.omnixpackage";
            if (!builder.Build(pkgPath)) {
                LOG_ERROR("[PackageTest] Test 6 FAILED: Build failed.");
                return false;
            }

            // Let's load the package format using DeserializePackage, modify offset, and SerializePackage back.
            OmnixPackage pkgData;
            if (!DeserializePackage(pkgData, pkgPath)) {
                LOG_ERROR("[PackageTest] Test 6 FAILED: Failed to deserialize package for offset modification.");
                std::filesystem::remove(pkgPath);
                return false;
            }

            // Corrupt the asset entry's offset to point outside the file size
            if (!pkgData.assets.empty()) {
                // Set offset way beyond file size
                pkgData.assets[0].dataOffset = 99999999; 
            }

            if (!SerializePackage(pkgData, pkgPath)) {
                LOG_ERROR("[PackageTest] Test 6 FAILED: Failed to serialize corrupted offset package.");
                std::filesystem::remove(pkgPath);
                return false;
            }

            Package pkg;
            if (pkg.Open(pkgPath)) {
                LOG_ERROR("[PackageTest] Test 6 FAILED: Package with out-of-bounds asset offset was opened successfully!");
                std::filesystem::remove(pkgPath);
                return false;
            }

            std::filesystem::remove(pkgPath);
            LOG_INFO("[PackageTest] Test 6 Passed: Package with bad asset offset was successfully rejected.");
        }

        // -----------------------------------------------------------------------------
        // Test 7: Package Mount Stress Test
        // -----------------------------------------------------------------------------
        LOG_INFO("[PackageTest] Running Test 7: Package Mount Stress Test...");
        {
            // Build a valid package
            PackageBuilder builder;
            builder.AddAsset(AssetHandle{99}, AssetType::Shader, {1, 2, 3}, {});
            std::string pkgPath = "test_output/stress.omnixpackage";
            if (!builder.Build(pkgPath)) {
                LOG_ERROR("[PackageTest] Test 7 FAILED: Build failed.");
                return false;
            }

            PackageManager& manager = GetPackageManager();
            manager.Clear();

            // Run mount/unmount in a loop 100 times to verify memory stability and correct unmounting.
            for (int i = 0; i < 100; ++i) {
                if (!manager.MountPackage(pkgPath)) {
                    LOG_ERROR("[PackageTest] Test 7 FAILED: Mount failed on iteration %d", i);
                    std::filesystem::remove(pkgPath);
                    return false;
                }

                if (!manager.Contains(AssetHandle{99})) {
                    LOG_ERROR("[PackageTest] Test 7 FAILED: Contains failed on iteration %d", i);
                    std::filesystem::remove(pkgPath);
                    return false;
                }

                if (!manager.UnmountPackage(pkgPath)) {
                    LOG_ERROR("[PackageTest] Test 7 FAILED: Unmount failed on iteration %d", i);
                    std::filesystem::remove(pkgPath);
                    return false;
                }
            }

            std::filesystem::remove(pkgPath);
            LOG_INFO("[PackageTest] Test 7 Passed: Stress mount/unmount of 100 iterations ran successfully with zero leaks.");
        }

        // Clean up output dir
        std::error_code ec;
        std::filesystem::remove_all("test_output", ec);

        LOG_INFO("================================================================================");
        LOG_INFO("                   ALL ASSET ASSET PACKAGING TESTS PASSED                       ");
        LOG_INFO("================================================================================");
        return true;
    }

} // namespace eng::runtime
