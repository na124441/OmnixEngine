#include "Runtime/AssetRegistryTests.h"
#include "Runtime/AssetRegistry.h"
#include "Runtime/AssetHandle.h"
#include "Runtime/AssetType.h"
#include "Core/Logger.h"
#include <iostream>
#include <vector>
#include <filesystem>

namespace eng::runtime {

    bool RunAssetRegistryTests() noexcept {
        LOG_INFO("================================================================================");
        LOG_INFO("                    RUNNING ASSET IDENTITY & REGISTRY TESTS                     ");
        LOG_INFO("================================================================================");

        AssetRegistry registry;

        // -----------------------------------------------------------------------------
        // Test 1 — Deterministic Handle Test
        // -----------------------------------------------------------------------------
        LOG_INFO("[AssetTest] Running Test 1: Deterministic Handle Test...");
        {
            std::string path = "Assets/Textures/wall.png";
            AssetType type = AssetType::Texture;

            AssetHandle handleA = registry.RegisterAsset(path, type);
            // Clear memory registry to verify it creates the same handle again
            registry.Clear();
            AssetHandle handleB = registry.RegisterAsset(path, type);

            if (handleA != handleB) {
                LOG_ERROR("[AssetTest] Test 1 FAILED: Non-deterministic handles generated!");
                return false;
            }
            if (!handleA.IsValid()) {
                LOG_ERROR("[AssetTest] Test 1 FAILED: Generated handle is invalid!");
                return false;
            }
            LOG_INFO("[AssetTest] Test 1 Passed: wall.png consistently mapped to %llu", handleA.value);
        }

        // -----------------------------------------------------------------------------
        // Test 2 — Collision Test
        // -----------------------------------------------------------------------------
        LOG_INFO("[AssetTest] Running Test 2: Collision Test...");
        {
            registry.Clear();
            std::vector<std::string> paths = {
                "wall.png",
                "floor.png",
                "cube.gltf",
                "player.gltf"
            };
            std::vector<AssetType> types = {
                AssetType::Texture,
                AssetType::Texture,
                AssetType::Mesh,
                AssetType::Mesh
            };

            std::vector<AssetHandle> handles;
            for (size_t i = 0; i < paths.size(); ++i) {
                handles.push_back(registry.RegisterAsset(paths[i], types[i]));
            }

            // Check for duplicate handles
            for (size_t i = 0; i < handles.size(); ++i) {
                for (size_t j = i + 1; j < handles.size(); ++j) {
                    if (handles[i] == handles[j]) {
                        LOG_ERROR("[AssetTest] Test 2 FAILED: Collision detected between %s and %s (Handle: %llu)!",
                                  paths[i].c_str(), paths[j].c_str(), handles[i].value);
                        return false;
                    }
                }
            }
            LOG_INFO("[AssetTest] Test 2 Passed: All %zu handles are unique.", handles.size());
        }

        // -----------------------------------------------------------------------------
        // Test 3 — Metadata Lookup Test
        // -----------------------------------------------------------------------------
        LOG_INFO("[AssetTest] Running Test 3: Metadata Lookup Test...");
        {
            registry.Clear();
            AssetHandle wallTex = registry.RegisterAsset("Assets/Textures/wall.png", AssetType::Texture);
            
            const AssetMetadata* meta = registry.GetMetadata(wallTex);
            if (!meta) {
                LOG_ERROR("[AssetTest] Test 3 FAILED: Metadata lookup returned nullptr for valid handle!");
                return false;
            }

            if (meta->handle != wallTex || meta->type != AssetType::Texture || meta->sourcePath != "Assets/Textures/wall.png") {
                LOG_ERROR("[AssetTest] Test 3 FAILED: Metadata content mismatch!");
                return false;
            }
            LOG_INFO("[AssetTest] Test 3 Passed: Metadata lookup retrieved correct details.");
        }

        // -----------------------------------------------------------------------------
        // Test 4 — Save/Load Test & Test 5 — Dependency Preservation Test
        // -----------------------------------------------------------------------------
        LOG_INFO("[AssetTest] Running Test 4 & 5: Save/Load & Dependency Preservation...");
        {
            registry.Clear();
            
            // Register Assets
            AssetHandle texA = registry.RegisterAsset("Assets/Textures/wall_albedo.png", AssetType::Texture);
            AssetHandle texB = registry.RegisterAsset("Assets/Textures/wall_normal.png", AssetType::Texture);
            AssetHandle shader = registry.RegisterAsset("Assets/Shaders/pbr.omnixshader", AssetType::Shader);
            AssetHandle material = registry.RegisterAsset("Assets/Materials/wall.omnixmat", AssetType::Material);

            // Establish dependencies: Material -> Texture A, Texture B, Shader
            const AssetMetadata* origMetaConst = registry.GetMetadata(material);
            if (!origMetaConst) {
                LOG_ERROR("[AssetTest] Failed to retrieve material metadata for setup!");
                return false;
            }
            
            // Cast const away for test configuration
            AssetMetadata* origMeta = const_cast<AssetMetadata*>(origMetaConst);
            origMeta->dependencies.push_back(texA);
            origMeta->dependencies.push_back(texB);
            origMeta->dependencies.push_back(shader);
            origMeta->isDirty = false;
            origMeta->isImported = true;
            origMeta->importedPath = "Cache/Materials/wall.omnixmat.bin";
            origMeta->importTimestamp = 123456789ULL;

            // Save Registry to disk
            std::string dbFile = "AssetRegistry.json";
            if (!registry.SaveRegistry(dbFile)) {
                LOG_ERROR("[AssetTest] Test 4 FAILED: Could not save registry to %s", dbFile.c_str());
                return false;
            }

            // Clear registry in memory
            registry.Clear();
            if (registry.Contains(material)) {
                LOG_ERROR("[AssetTest] Test 4 FAILED: Clear operation did not flush registry!");
                return false;
            }

            // Load Registry back from disk
            if (!registry.LoadRegistry(dbFile)) {
                LOG_ERROR("[AssetTest] Test 4 FAILED: Could not load registry from %s", dbFile.c_str());
                std::filesystem::remove(dbFile);
                return false;
            }

            // Verify equivalence
            const AssetMetadata* loadedMeta = registry.GetMetadata(material);
            if (!loadedMeta) {
                LOG_ERROR("[AssetTest] Test 4 FAILED: Material handle could not be resolved after loading!");
                std::filesystem::remove(dbFile);
                return false;
            }

            if (loadedMeta->importedPath != "Cache/Materials/wall.omnixmat.bin" ||
                loadedMeta->importTimestamp != 123456789ULL ||
                loadedMeta->isImported != true ||
                loadedMeta->isDirty != false) {
                LOG_ERROR("[AssetTest] Test 4 FAILED: Loaded metadata fields do not match original!");
                std::filesystem::remove(dbFile);
                return false;
            }

            // Verify dependency preservation (Test 5)
            if (loadedMeta->dependencies.size() != 3) {
                LOG_ERROR("[AssetTest] Test 5 FAILED: Dependency count mismatch after load (Expected 3, got %zu)!",
                          loadedMeta->dependencies.size());
                std::filesystem::remove(dbFile);
                return false;
            }

            if (loadedMeta->dependencies[0] != texA ||
                loadedMeta->dependencies[1] != texB ||
                loadedMeta->dependencies[2] != shader) {
                LOG_ERROR("[AssetTest] Test 5 FAILED: Dependency handles sequence was not preserved!");
                std::filesystem::remove(dbFile);
                return false;
            }

            // Cleanup db file
            std::filesystem::remove(dbFile);
            LOG_INFO("[AssetTest] Test 4 & 5 Passed: Persistence cycle and dependencies preserved successfully.");
        }

        LOG_INFO("================================================================================");
        LOG_INFO("                  ALL ASSET IDENTITY & REGISTRY TESTS PASSED                     ");
        LOG_INFO("================================================================================");
        return true;
    }

} // namespace eng::runtime
