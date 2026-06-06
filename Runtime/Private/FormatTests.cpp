#include "Runtime/Public/FormatTests.h"
#include "Runtime/Public/FileHeader.h"
#include "Runtime/Public/Checksum.h"
#include "Runtime/Public/BinaryReader.h"
#include "Runtime/Public/BinaryWriter.h"
#include "Runtime/Public/OmnixMeshFormat.h"
#include "Runtime/Public/OmnixMaterialFormat.h"
#include "Runtime/Public/OmnixSceneFormat.h"
#include "Runtime/Public/OmnixAnimFormat.h"
#include "Runtime/Public/OmnixPackageFormat.h"
#include "Runtime/Public/World/WorldDescriptor.h"
#include "Runtime/Public/World/WorldFileReader.h"
#include "Runtime/Public/World/WorldFileWriter.h"
#include "Runtime/Public/World/OmnixWorldHeader.h"
#include "Runtime/Public/World/WorldFileError.h"
#include "Runtime/Public/World/WorldZone.h"
#include "Runtime/Public/World/WorldZoneReader.h"
#include "Runtime/Public/World/WorldZoneWriter.h"
#include "Runtime/Public/World/WorldManager.h"
#include "Runtime/Public/AssetRegistry.h"
#include "RenderingEngine/Public/IAssetManager.h"
#include "Renderer/scene/Texture.h"
#include "Renderer/scene/Mesh.h"
#include "Renderer/scene/Material.h"
#include "Runtime/Public/World/OmnixZoneHeader.h"
#include "Serializer/Serialization/SerializationCommon.h"
#include "Core/Logger.h"
#include "Scene/Scene.h"
#include "Scene/SceneObject.h"
#include "Scene/SceneSerializer.h"
#include "Scene/SceneLoader.h"
#include "Scene/SceneManager.h"
#include "Scene/SceneValidator.h"
#include "ECS/Coordinator.h"
#include "ECS/ECSComponents.h"
#include "ECS/PhysicsSystem.h"
#include "ECS/PlayerSystem.h"
#include "Physics/Public/PhysicsValidation.h"
#include "Physics/Public/PhysicsWorld.h"
#include "Runtime/Public/Gameplay/GameplayEvent.h"
#include "Runtime/Public/Gameplay/GameplayEventBus.h"
#include "ECS/PlayerControllerSystem.h"
#include "Core/World.h"
#include <iostream>
#include <vector>
#include <filesystem>
#include <cstring>
#include <algorithm>
#include <iterator>

namespace eng::runtime {

    bool RunFormatTests() noexcept {
        LOG_INFO("================================================================================");
        LOG_INFO("                     RUNNING OMNIX RUNTIME FORMAT TESTS                         ");
        LOG_INFO("================================================================================");

        // -----------------------------------------------------------------------------
        // Test 1 — Magic Header Test
        // -----------------------------------------------------------------------------
        LOG_INFO("[FormatTest] Running Test 1: Magic Header Test...");
        {
            OmnixMesh mesh;
            mesh.header.vertexCount = 4;
            mesh.header.indexCount = 6;
            mesh.header.vertexStride = sizeof(OmnixVertex);
            mesh.header.submeshCount = 1;
            mesh.header.hasSkeleton = 0;
            mesh.header.materialSlotCount = 1;
            mesh.header.bounds = BoundingBox{ {0,0,0}, {1,1,1} };
            mesh.header.sphere = BoundingSphere{ {0.5f,0.5f,0.5f}, 0.866f };

            mesh.vertices.resize(4);
            mesh.indices = {0, 1, 2, 2, 3, 0};
            mesh.submeshes = { {0, 6, 0} };
            mesh.materialSlots = { AssetHandle{12345} };
            mesh.skeletonAssetPath = "";

            std::string filename = "test_magic.omnixmesh";
            if (!SerializeMesh(mesh, filename)) {
                LOG_ERROR("[FormatTest] Test 1 FAILED: Could not serialize mesh.");
                return false;
            }

            // Read raw FileHeader
            std::ifstream file(filename, std::ios::binary);
            if (!file.is_open()) {
                LOG_ERROR("[FormatTest] Test 1 FAILED: Could not reopen serialized mesh file.");
                std::filesystem::remove(filename);
                return false;
            }

            FileHeader header;
            file.read(reinterpret_cast<char*>(&header), sizeof(FileHeader));
            file.close();
            std::filesystem::remove(filename);

            if (std::memcmp(header.magic, MAGIC_MESH, 8) != 0) {
                LOG_ERROR("[FormatTest] Test 1 FAILED: Magic header mismatch!");
                return false;
            }
            LOG_INFO("[FormatTest] Test 1 Passed: Magic header correctly matched OMXMESH.");
        }

        // -----------------------------------------------------------------------------
        // Test 2 — Version Rejection Test
        // -----------------------------------------------------------------------------
        LOG_INFO("[FormatTest] Running Test 2: Version Rejection Test...");
        {
            std::string filename = "test_version_rejection.omnixmesh";
            BinaryWriter writer;
            writer.BeginFile(MAGIC_MESH, 9999, OMNIX_MESH_VERSION_MINOR); // Invalid Major Version 9999
            
            // Dummy header fields
            writer.WriteU32(0);
            writer.WriteU32(0);
            writer.WriteU32(0);
            writer.WriteU32(0);
            writer.WriteU32(0);
            writer.WriteU32(0);
            
            BoundingBox box;
            BoundingSphere sphere;
            writer.WriteBytes(reinterpret_cast<const uint8_t*>(&box), sizeof(box));
            writer.WriteBytes(reinterpret_cast<const uint8_t*>(&sphere), sizeof(sphere));
            writer.WriteString(""); // Skeleton path

            writer.SaveToFile(filename);

            OmnixMesh loadedMesh;
            bool ok = DeserializeMesh(loadedMesh, filename);
            std::filesystem::remove(filename);

            if (ok) {
                LOG_ERROR("[FormatTest] Test 2 FAILED: File with unsupported major version was accepted!");
                return false;
            }
            LOG_INFO("[FormatTest] Test 2 Passed: Version rejection test passed successfully.");
        }

        // -----------------------------------------------------------------------------
        // Test 3 — Checksum Corruption Test
        // -----------------------------------------------------------------------------
        LOG_INFO("[FormatTest] Running Test 3: Checksum Corruption Test...");
        {
            OmnixMesh mesh;
            mesh.header.vertexCount = 10;
            mesh.vertices.resize(10);

            std::string filename = "test_corruption.omnixmesh";
            if (!SerializeMesh(mesh, filename)) {
                LOG_ERROR("[FormatTest] Failed to write mesh for corruption test");
                return false;
            }

            // Read file bytes
            std::ifstream inFile(filename, std::ios::binary);
            std::vector<uint8_t> bytes((std::istreambuf_iterator<char>(inFile)), std::istreambuf_iterator<char>());
            inFile.close();

            if (bytes.size() > 40) {
                // Corrupt one byte of the payload (at index 35)
                bytes[35] ^= 0xFF; 
            } else {
                LOG_ERROR("[FormatTest] Checksum test mesh file too small!");
                std::filesystem::remove(filename);
                return false;
            }

            // Write corrupted bytes back
            std::ofstream outFile(filename, std::ios::binary);
            outFile.write(reinterpret_cast<const char*>(bytes.data()), bytes.size());
            outFile.close();

            // Try deserializing
            OmnixMesh loadedMesh;
            bool ok = DeserializeMesh(loadedMesh, filename);
            std::filesystem::remove(filename);

            if (ok) {
                LOG_ERROR("[FormatTest] Test 3 FAILED: Corrupted file was accepted without flagging checksum error!");
                return false;
            }
            LOG_INFO("[FormatTest] Test 3 Passed: Checksum validation successfully detected corruption.");
        }

        // -----------------------------------------------------------------------------
        // Test 4 — Round Trip Test
        // -----------------------------------------------------------------------------
        LOG_INFO("[FormatTest] Running Test 4: Round Trip Test (All 5 Formats)...");
        {
            // --- 1. MESH ROUND TRIP ---
            {
                OmnixMesh orig;
                orig.header.vertexCount = 3;
                orig.header.indexCount = 3;
                orig.header.vertexStride = sizeof(OmnixVertex);
                orig.header.submeshCount = 1;
                orig.header.hasSkeleton = 0;
                orig.header.materialSlotCount = 1;
                orig.header.bounds = BoundingBox{ {0.0f,0.0f,0.0f}, {1.0f,2.0f,3.0f} };
                orig.header.sphere = BoundingSphere{ {0.5f,1.0f,1.5f}, 2.5f };

                OmnixVertex v0, v1, v2;
                v0.position = {0,0,0}; v0.normal = {0,1,0}; v0.tangent = {1,0,0,1}; v0.uv0 = {0,0}; v0.uv1 = {0,0};
                v1.position = {1,0,0}; v1.normal = {0,1,0}; v1.tangent = {1,0,0,1}; v1.uv0 = {1,0}; v1.uv1 = {1,0};
                v2.position = {0,1,0}; v2.normal = {0,1,0}; v2.tangent = {1,0,0,1}; v2.uv0 = {0,1}; v2.uv1 = {0,1};
                orig.vertices = {v0, v1, v2};
                orig.indices = {0, 1, 2};
                orig.submeshes = { {0, 3, 0} };
                orig.materialSlots = { AssetHandle{7777} };
                orig.skeletonAssetPath = "test_skeleton_path";

                std::string file = "rt_mesh.omnixmesh";
                if (!SerializeMesh(orig, file)) {
                    LOG_ERROR("[FormatTest] Mesh serialization failed!");
                    return false;
                }

                OmnixMesh loaded;
                if (!DeserializeMesh(loaded, file)) {
                    LOG_ERROR("[FormatTest] Mesh deserialization failed!");
                    std::filesystem::remove(file);
                    return false;
                }
                std::filesystem::remove(file);

                if (loaded.header.vertexCount != orig.header.vertexCount ||
                    loaded.header.indexCount != orig.header.indexCount ||
                    loaded.vertices != orig.vertices ||
                    loaded.indices != orig.indices ||
                    loaded.submeshes != orig.submeshes ||
                    loaded.materialSlots != orig.materialSlots ||
                    loaded.skeletonAssetPath != orig.skeletonAssetPath) {
                    LOG_ERROR("[FormatTest] Mesh Round Trip data mismatch!");
                    return false;
                }
                LOG_INFO("[FormatTest]   -> Mesh Round Trip OK.");
            }

            // --- 2. MATERIAL ROUND TRIP ---
            {
                OmnixMaterial orig;
                orig.header.shader = AssetHandle{99999};
                orig.header.textureBindingCount = 2;
                orig.header.scalarParameterCount = 1;
                orig.header.vectorParameterCount = 1;
                orig.header.blendMode = 2;
                orig.header.cullMode = 1;
                orig.header.depthTest = 1;
                orig.name = "GoldMaterial";

                orig.textures = {
                    {MaterialTextureSlot::Albedo, AssetHandle{1001}},
                    {MaterialTextureSlot::Normal, AssetHandle{1002}}
                };

                MaterialScalarParameter sp;
                std::strncpy(sp.name, "roughness", 64);
                sp.value = 0.45f;
                orig.scalars = { sp };

                MaterialVectorParameter vp;
                std::strncpy(vp.name, "baseColor", 64);
                vp.value[0] = 1.0f; vp.value[1] = 0.8f; vp.value[2] = 0.2f; vp.value[3] = 1.0f;
                orig.vectors = { vp };

                std::string file = "rt_mat.omnixmat";
                if (!SerializeMaterial(orig, file)) {
                    LOG_ERROR("[FormatTest] Material serialization failed!");
                    return false;
                }

                OmnixMaterial loaded;
                if (!DeserializeMaterial(loaded, file)) {
                    LOG_ERROR("[FormatTest] Material deserialization failed!");
                    std::filesystem::remove(file);
                    return false;
                }
                std::filesystem::remove(file);

                if (loaded.header.shader != orig.header.shader ||
                    loaded.name != orig.name ||
                    loaded.textures != orig.textures ||
                    loaded.scalars != orig.scalars ||
                    loaded.vectors != orig.vectors ||
                    loaded.header.blendMode != orig.header.blendMode ||
                    loaded.header.cullMode != orig.header.cullMode ||
                    loaded.header.depthTest != orig.header.depthTest) {
                    LOG_ERROR("[FormatTest] Material Round Trip data mismatch!");
                    return false;
                }
                LOG_INFO("[FormatTest]   -> Material Round Trip OK.");
            }

            // --- 3. SCENE ROUND TRIP ---
            {
                OmnixScene orig;
                orig.header.entityCount = 2;
                orig.header.componentTypeCount = 2;
                orig.header.assetReferenceCount = 1;
                orig.header.hierarchyNodeCount = 1;
                orig.header.componentBlockCount = 2;
                orig.sceneName = "ForestLevel";

                orig.componentTypes = { {1, "TransformComponent"}, {2, "LightComponent"} };
                orig.assetReferences = { {AssetHandle{555}, "Assets/Textures/bark.png"} };
                orig.entities = { {0, 1, 3, 0}, {1, 1, 1, 0} };
                orig.hierarchy = { {1, 0} };
                orig.components = {
                    {0, 1, {1,2,3,4}},
                    {1, 2, {255,128}}
                };

                std::string file = "rt_scene.omnixscene";
                if (!SerializeScene(orig, file)) {
                    LOG_ERROR("[FormatTest] Scene serialization failed!");
                    return false;
                }

                OmnixScene loaded;
                if (!DeserializeScene(loaded, file)) {
                    LOG_ERROR("[FormatTest] Scene deserialization failed!");
                    std::filesystem::remove(file);
                    return false;
                }
                std::filesystem::remove(file);

                if (loaded.sceneName != orig.sceneName ||
                    loaded.componentTypes != orig.componentTypes ||
                    loaded.assetReferences != orig.assetReferences ||
                    loaded.entities != orig.entities ||
                    loaded.hierarchy != orig.hierarchy ||
                    loaded.components != orig.components) {
                    LOG_ERROR("[FormatTest] Scene Round Trip data mismatch!");
                    return false;
                }
                LOG_INFO("[FormatTest]   -> Scene Round Trip OK.");
            }

            // --- 4. ANIMATION ROUND TRIP ---
            {
                OmnixAnim orig;
                orig.header.durationSeconds = 2.5f;
                orig.header.ticksPerSecond = 30.0f;
                orig.header.boneTrackCount = 1;
                orig.header.compressionType = 0;
                orig.header.hasRootMotion = 1;
                orig.name = "WalkCycle";

                BoneTrack track;
                track.header.boneIndex = 0;
                track.boneName = "RootBone";
                track.positionKeys = { {0.0f, {0,0,0}}, {1.0f, {0,0,1}} };
                track.rotationKeys = { {0.0f, {0,0,0,1}}, {1.0f, {0.707f,0,0,0.707f}} };
                track.scaleKeys = { {0.0f, {1,1,1}} };

                orig.boneTracks = { track };

                std::string file = "rt_anim.omnixanim";
                if (!SerializeAnim(orig, file)) {
                    LOG_ERROR("[FormatTest] Animation serialization failed!");
                    return false;
                }

                OmnixAnim loaded;
                if (!DeserializeAnim(loaded, file)) {
                    LOG_ERROR("[FormatTest] Animation deserialization failed!");
                    std::filesystem::remove(file);
                    return false;
                }
                std::filesystem::remove(file);

                if (loaded.name != orig.name ||
                    loaded.header.durationSeconds != orig.header.durationSeconds ||
                    loaded.header.ticksPerSecond != orig.header.ticksPerSecond ||
                    loaded.boneTracks != orig.boneTracks) {
                    LOG_ERROR("[FormatTest] Animation Round Trip data mismatch!");
                    return false;
                }
                LOG_INFO("[FormatTest]   -> Animation Round Trip OK.");
            }

            // --- 5. PACKAGE ROUND TRIP ---
            {
                OmnixPackage orig;
                orig.header.assetCount = 2;
                orig.header.dependencyCount = 1;
                orig.header.chunkCount = 1;
                orig.header.assetTableOffset = sizeof(OmnixPackageHeader);
                orig.header.dependencyTableOffset = orig.header.assetTableOffset + 2 * sizeof(PackageAssetEntry);
                orig.header.chunkTableOffset = orig.header.dependencyTableOffset + sizeof(PackageDependencyEntry);
                orig.header.dataBlockOffset = orig.header.chunkTableOffset + sizeof(PackageChunkEntry);

                orig.assets = {
                    {AssetHandle{101}, 1, 0, 16, 0, 1111},
                    {AssetHandle{102}, 2, 16, 32, 0, 2222}
                };
                orig.dependencies = { {AssetHandle{102}, AssetHandle{101}} };
                orig.chunks = { {0, 0, 48, 0} };
                orig.rawDataBlock = {0,1,2,3,4,5,6,7,8,9,10,11,12,13,14,15,99,99,99,99};

                std::string file = "rt_pkg.omnixpackage";
                if (!SerializePackage(orig, file)) {
                    LOG_ERROR("[FormatTest] Package serialization failed!");
                    return false;
                }

                OmnixPackage loaded;
                if (!DeserializePackage(loaded, file)) {
                    LOG_ERROR("[FormatTest] Package deserialization failed!");
                    std::filesystem::remove(file);
                    return false;
                }
                std::filesystem::remove(file);

                if (loaded.assets != orig.assets ||
                    loaded.dependencies != orig.dependencies ||
                    loaded.chunks != orig.chunks ||
                    loaded.rawDataBlock != orig.rawDataBlock) {
                    LOG_ERROR("[FormatTest] Package Round Trip data mismatch!");
                    return false;
                }
                LOG_INFO("[FormatTest]   -> Package Round Trip OK.");
            }

            LOG_INFO("[FormatTest] Test 4 Passed: All 5 formats successfully passed Round Trip tests.");
        }

        // -----------------------------------------------------------------------------
        // Test 5 — Determinism Test
        // -----------------------------------------------------------------------------
        LOG_INFO("[FormatTest] Running Test 5: Determinism Test...");
        {
            OmnixMesh orig;
            orig.header.vertexCount = 2;
            orig.header.indexCount = 2;
            orig.header.vertexStride = sizeof(OmnixVertex);
            orig.header.submeshCount = 1;
            orig.header.hasSkeleton = 0;
            orig.header.materialSlotCount = 1;
            orig.header.bounds = BoundingBox{ {0,0,0}, {1,1,1} };
            orig.header.sphere = BoundingSphere{ {0.5f,0.5f,0.5f}, 0.5f };

            OmnixVertex v0, v1;
            v0.position = {0,0,0}; v0.normal = {0,1,0}; v0.tangent = {0,0,0,1}; v0.uv0 = {0,0}; v0.uv1 = {0,0};
            v1.position = {1,1,1}; v1.normal = {1,0,0}; v1.tangent = {0,0,0,1}; v1.uv0 = {1,1}; v1.uv1 = {1,1};
            orig.vertices = {v0, v1};
            orig.indices = {0, 1};
            orig.submeshes = { {0, 2, 0} };
            orig.materialSlots = { AssetHandle{100} };
            orig.skeletonAssetPath = "det_skelet";

            std::string filename = "test_determinism.omnixmesh";
            if (!SerializeMesh(orig, filename)) {
                LOG_ERROR("[FormatTest] Failed to write mesh for determinism test");
                return false;
            }

            // Run 100 parses and check equivalence
            for (int i = 0; i < 100; ++i) {
                OmnixMesh loaded;
                if (!DeserializeMesh(loaded, filename)) {
                    LOG_ERROR("[FormatTest] Determinism loop failed to read file on iteration %d", i);
                    std::filesystem::remove(filename);
                    return false;
                }
                
                if (loaded.header.vertexCount != orig.header.vertexCount ||
                    loaded.vertices != orig.vertices ||
                    loaded.indices != orig.indices ||
                    loaded.submeshes != orig.submeshes ||
                    loaded.materialSlots != orig.materialSlots ||
                    loaded.skeletonAssetPath != orig.skeletonAssetPath) {
                    LOG_ERROR("[FormatTest] Determinism loop output mismatch on iteration %d", i);
                    std::filesystem::remove(filename);
                    return false;
                }
            }

            std::filesystem::remove(filename);
            LOG_INFO("[FormatTest] Test 5 Passed: Parser is fully deterministic over 100 runs.");
        }

        // -----------------------------------------------------------------------------
        // Test 6 — JSON Scene Round Trip Test (.omnixscene)
        // -----------------------------------------------------------------------------
        LOG_INFO("[FormatTest] Running Test 6: JSON Scene Round Trip Test...");
        {
            Scene* scene = new Scene("RoundTripScene");

            auto rootObj = std::make_shared<SceneObject>("RootEntity");
            rootObj->transform.SetPosition(Vector3(1.0f, 2.0f, 3.0f));
            rootObj->transform.SetScale(Vector3(1.0f, 1.0f, 1.0f));

            auto childObj = std::make_shared<SceneObject>("ChildEntity");
            childObj->transform.SetPosition(Vector3(4.0f, 5.0f, 6.0f));
            childObj->transform.SetScale(Vector3(2.0f, 2.0f, 2.0f));
            childObj->SetRenderableMesh(AssetHandle(777));
            childObj->SetMaterial(AssetHandle(888));

            rootObj->AddChild(childObj.get());
            childObj->SetParent(rootObj.get());

            scene->AddSceneObject(rootObj);
            scene->AddSceneObject(childObj);

            std::string tempPath = "rt_test.omnixscene";
            if (!SceneSerializer::SaveScene(scene, tempPath)) {
                LOG_ERROR("[FormatTest] Test 6 FAILED: Failed to serialize scene to JSON.");
                delete scene;
                return false;
            }

            Scene* loadedScene = SceneLoader::LoadFromFile(tempPath);
            std::filesystem::remove(tempPath);

            if (!loadedScene) {
                LOG_ERROR("[FormatTest] Test 6 FAILED: Failed to load scene from JSON.");
                delete scene;
                return false;
            }

            if (loadedScene->GetName() != "RoundTripScene") {
                LOG_ERROR("[FormatTest] Test 6 FAILED: Loaded scene name mismatch!");
                delete scene;
                delete loadedScene;
                return false;
            }

            if (loadedScene->GetAllSceneObjects().size() != 2) {
                LOG_ERROR("[FormatTest] Test 6 FAILED: Loaded scene object count mismatch!");
                delete scene;
                delete loadedScene;
                return false;
            }

            auto loadedRoot = loadedScene->FindObjectByName("RootEntity");
            auto loadedChild = loadedScene->FindObjectByName("ChildEntity");

            if (!loadedRoot || !loadedChild) {
                LOG_ERROR("[FormatTest] Test 6 FAILED: Could not find loaded entities by name!");
                delete scene;
                delete loadedScene;
                return false;
            }

            if (loadedRoot->transform.GetPosition().x != 1.0f ||
                loadedChild->transform.GetPosition().y != 5.0f) {
                LOG_ERROR("[FormatTest] Test 6 FAILED: Loaded entity transforms mismatched!");
                delete scene;
                delete loadedScene;
                return false;
            }

            if (!loadedChild->GetParent() || loadedChild->GetParent()->GetName() != "RootEntity") {
                LOG_ERROR("[FormatTest] Test 6 FAILED: Parent-child hierarchy link was not restored!");
                delete scene;
                delete loadedScene;
                return false;
            }

            if (!loadedChild->m_HasRenderableMesh || loadedChild->m_MeshAssetHandle.value != 777 ||
                !loadedChild->m_HasMaterial || loadedChild->m_MaterialAssetHandle.value != 888) {
                LOG_ERROR("[FormatTest] Test 6 FAILED: Components (mesh/material handles) mismatched after load!");
                delete scene;
                delete loadedScene;
                return false;
            }

            delete scene;
            delete loadedScene;
            LOG_INFO("[FormatTest] Test 6 Passed: JSON Scene Round Trip successfully restored entity hierarchy, transforms, names, and assets.");
        }

        // -----------------------------------------------------------------------------
        // Test 7 — Edit / Play Mode Snapshot & Restore Simulation Test
        // -----------------------------------------------------------------------------
        LOG_INFO("[FormatTest] Running Test 7: Edit / Play Mode Snapshot & Restore Simulation Test...");
        {
            Coordinator coordinator;
            coordinator.Init();
            coordinator.RegisterComponent<TransformComponent>();
            coordinator.RegisterComponent<RigidBodyComponent>();
            coordinator.RegisterComponent<MeshRendererComponent>();
            coordinator.RegisterComponent<CameraComponent>();
            coordinator.RegisterComponent<ColliderComponent>();
            coordinator.RegisterComponent<PlayerControllerComponent>();
            coordinator.RegisterComponent<TagComponent>();
            coordinator.RegisterComponent<LayerComponent>();
            coordinator.RegisterComponent<HealthComponent>();
            coordinator.RegisterComponent<RenderableMeshComponent>();
            coordinator.RegisterComponent<MaterialComponent>();
            coordinator.RegisterComponent<NameComponent>();

            auto physicsSys = coordinator.RegisterSystem<PhysicsSystem>();
            ::Signature sig;
            sig.set(coordinator.GetComponentType<TransformComponent>());
            sig.set(coordinator.GetComponentType<RigidBodyComponent>());
            coordinator.SetSystemSignature<PhysicsSystem>(sig);

            // Initialize SceneManager with the test coordinator
            SceneManager sceneMgr(&coordinator);

            // Create initial Edit Scene
            sceneMgr.CreateNewScene("EditScene");
            
            // Create a physical entity in ECS
            Entity testEntity = coordinator.CreateEntity();
            coordinator.AddComponent<NameComponent>(testEntity, NameComponent("OriginalName"));
            
            TransformComponent tc;
            tc.position = Vector3(1.0f, 10.0f, 1.0f);
            coordinator.AddComponent<TransformComponent>(testEntity, tc);
            
            RigidBodyComponent rb;
            rb.useGravity = true;
            coordinator.AddComponent<RigidBodyComponent>(testEntity, rb);

            // Sync coordinator changes to active scene
            sceneMgr.SyncECSToScene();

            // Save Snapshot (Simulating entering Play Mode)
            std::string tempPath = "Assets/Scenes/play_session_temp_test.omnixscene";
            if (!sceneMgr.SaveActiveScene(tempPath)) {
                LOG_ERROR("[FormatTest] Test 7 FAILED: Could not save play session backup snapshot.");
                std::filesystem::remove(tempPath);
                return false;
            }

            // --- Simulating Play Mode updates (mutating values) ---
            // 1. Run physics update (simulate gravity acceleration)
            physicsSys->Update(1.0f, coordinator); // 1.0 second step

            // 2. Mutate name
            coordinator.GetComponent<NameComponent>(testEntity).name = "MutatedName";

            // Verify position dropped and name is changed
            float updatedY = coordinator.GetComponent<TransformComponent>(testEntity).position.y;
            std::string updatedName = coordinator.GetComponent<NameComponent>(testEntity).name;
            if (updatedY >= 10.0f || updatedName != "MutatedName") {
                LOG_ERROR("[FormatTest] Test 7 FAILED: Play mode simulation changes did not apply correctly.");
                std::filesystem::remove(tempPath);
                return false;
            }

            // --- Simulating stopping Play Mode (loading snapshot back) ---
            sceneMgr.LoadScene(tempPath);

            // Force SceneManager to process loading and switch scene synchronously
            sceneMgr.Update(0.1f); // processes loading
            sceneMgr.Update(0.1f); // switches scene

            // Check restored entity count
            if (coordinator.GetActiveEntities().size() != 1) {
                LOG_ERROR("[FormatTest] Test 7 FAILED: Restored entity count mismatch! Restored: %zu", coordinator.GetActiveEntities().size());
                std::filesystem::remove(tempPath);
                return false;
            }

            // Retrieve restored entity (address may change due to recreation, so we query it)
            Entity restoredEntity = *coordinator.GetActiveEntities().begin();
            std::string restoredName = coordinator.GetComponent<NameComponent>(restoredEntity).name;
            float restoredY = coordinator.GetComponent<TransformComponent>(restoredEntity).position.y;

            if (restoredName != "OriginalName") {
                LOG_ERROR("[FormatTest] Test 7 FAILED: Restored entity name mismatch! Restored: %s", restoredName.c_str());
                std::filesystem::remove(tempPath);
                return false;
            }

            if (restoredY != 10.0f) {
                LOG_ERROR("[FormatTest] Test 7 FAILED: Restored entity position mismatch! Restored Y: %.2f", restoredY);
                std::filesystem::remove(tempPath);
                return false;
            }

            std::filesystem::remove(tempPath);
            LOG_INFO("[FormatTest] Test 7 Passed: Edit/Play Mode snapshotting, simulation mutation, and clean restoration verified.");
        }

        // -----------------------------------------------------------------------------
        // Test 8 — In-Memory Runtime Scene Cloning & Independent Simulation
        // -----------------------------------------------------------------------------
        LOG_INFO("[FormatTest] Running Test 8: In-Memory Runtime Scene Cloning & Simulation...");
        {
            Coordinator srcCoordinator;
            srcCoordinator.Init();
            srcCoordinator.RegisterComponent<TransformComponent>();
            srcCoordinator.RegisterComponent<RigidBodyComponent>();
            srcCoordinator.RegisterComponent<MeshRendererComponent>();
            srcCoordinator.RegisterComponent<CameraComponent>();
            srcCoordinator.RegisterComponent<ColliderComponent>();
            srcCoordinator.RegisterComponent<PlayerControllerComponent>();
            srcCoordinator.RegisterComponent<TagComponent>();
            srcCoordinator.RegisterComponent<LayerComponent>();
            srcCoordinator.RegisterComponent<HealthComponent>();
            srcCoordinator.RegisterComponent<RenderableMeshComponent>();
            srcCoordinator.RegisterComponent<MaterialComponent>();
            srcCoordinator.RegisterComponent<NameComponent>();

            auto physicsSys = srcCoordinator.RegisterSystem<PhysicsSystem>();
            ::Signature sig;
            sig.set(srcCoordinator.GetComponentType<TransformComponent>());
            sig.set(srcCoordinator.GetComponentType<RigidBodyComponent>());
            srcCoordinator.SetSystemSignature<PhysicsSystem>(sig);

            // Create original edit scene
            Scene* srcScene = new Scene("EditScene");
            
            // Create a physical entity in scene and ECS
            auto srcObj = std::make_shared<SceneObject>("PhysicalEntity");
            srcObj->transform.SetPosition(Vector3(1.0f, 20.0f, 1.0f));
            srcScene->AddSceneObject(srcObj);
            srcObj->InitializeWithECS(srcCoordinator);

            // Set up RigidBody
            Entity oldEntity = srcObj->GetECSEntity();
            RigidBodyComponent rb;
            rb.useGravity = true;
            srcCoordinator.AddComponent<RigidBodyComponent>(oldEntity, rb);

            // Deep-copy ECS and Scene in-memory
            Coordinator destCoordinator;
            destCoordinator.Init();
            destCoordinator.RegisterComponent<TransformComponent>();
            destCoordinator.RegisterComponent<RigidBodyComponent>();
            destCoordinator.RegisterComponent<MeshRendererComponent>();
            destCoordinator.RegisterComponent<CameraComponent>();
            destCoordinator.RegisterComponent<ColliderComponent>();
            destCoordinator.RegisterComponent<PlayerControllerComponent>();
            destCoordinator.RegisterComponent<TagComponent>();
            destCoordinator.RegisterComponent<LayerComponent>();
            destCoordinator.RegisterComponent<HealthComponent>();
            destCoordinator.RegisterComponent<RenderableMeshComponent>();
            destCoordinator.RegisterComponent<MaterialComponent>();
            destCoordinator.RegisterComponent<NameComponent>();

            auto destPhysicsSys = destCoordinator.RegisterSystem<PhysicsSystem>();
            destCoordinator.SetSystemSignature<PhysicsSystem>(sig);

            std::unordered_map<Entity, Entity> entityMap;
            Scene* destScene = srcScene->Clone(srcCoordinator, destCoordinator, entityMap);

            // Verify clone structure
            if (!srcScene->CompareScene(*destScene, entityMap, destCoordinator)) {
                LOG_ERROR("[FormatTest] Test 8 FAILED: Cloned scene structure mismatched!");
                delete srcScene;
                delete destScene;
                return false;
            }

            // Simulate the cloned scene (runs physics update on cloned coordinator)
            destPhysicsSys->Update(1.0f, destCoordinator); // 1.0 second simulation step

            // Verify position dropped in destination, but remained untouched in source!
            Entity newEntity = entityMap[oldEntity];
            float srcY = srcCoordinator.GetComponent<TransformComponent>(oldEntity).position.y;
            float destY = destCoordinator.GetComponent<TransformComponent>(newEntity).position.y;

            if (srcY != 20.0f) {
                LOG_ERROR("[FormatTest] Test 8 FAILED: Original scene coordinates were mutated by cloned simulation!");
                delete srcScene;
                delete destScene;
                return false;
            }

            if (destY >= 20.0f) {
                LOG_ERROR("[FormatTest] Test 8 FAILED: Cloned simulation did not update transform independently. Dest Y: %.2f", destY);
                delete srcScene;
                delete destScene;
                return false;
            }

            // Verify destruction leaving no leaks
            delete srcScene;
            delete destScene;
            LOG_INFO("[FormatTest] Test 8 Passed: In-memory scene cloning, structural verification, independent simulation, and clean cleanup verified.");
        }

        // -----------------------------------------------------------------------------
        // Test 9 — Static Collider Authoring & Serialization
        // -----------------------------------------------------------------------------
        LOG_INFO("[FormatTest] Running Test 9: Static Collider Authoring & Serialization...");
        {
            Coordinator srcCoordinator;
            srcCoordinator.Init();
            srcCoordinator.RegisterComponent<TransformComponent>();
            srcCoordinator.RegisterComponent<RigidBodyComponent>();
            srcCoordinator.RegisterComponent<MeshRendererComponent>();
            srcCoordinator.RegisterComponent<CameraComponent>();
            srcCoordinator.RegisterComponent<LightComponent>();
            srcCoordinator.RegisterComponent<ColliderComponent>();
            srcCoordinator.RegisterComponent<PlayerControllerComponent>();
            srcCoordinator.RegisterComponent<TagComponent>();
            srcCoordinator.RegisterComponent<LayerComponent>();
            srcCoordinator.RegisterComponent<HealthComponent>();
            srcCoordinator.RegisterComponent<RenderableMeshComponent>();
            srcCoordinator.RegisterComponent<MaterialComponent>();
            srcCoordinator.RegisterComponent<NameComponent>();
            srcCoordinator.RegisterComponent<StaticBodyComponent>();
            srcCoordinator.RegisterComponent<BoxColliderComponent>();
            srcCoordinator.RegisterComponent<SphereColliderComponent>();
            srcCoordinator.RegisterComponent<CapsuleColliderComponent>();

            // 1. Create a scene with entities having colliders
            Scene* srcScene = new Scene("PhysicsTestScene");
            
            auto floorObj = std::make_shared<SceneObject>("Floor");
            floorObj->transform.SetPosition(Vector3(0.0f, -1.0f, 0.0f));
            
            StaticBodyComponent sbc;
            sbc.enabled = true;
            sbc.collisionLayer = 2;
            sbc.collisionMask = 0x000000FF;
            floorObj->SetStaticBody(sbc);

            BoxColliderComponent bcc;
            bcc.size = Vector3(10.0f, 0.5f, 10.0f);
            bcc.offset = Vector3(0.0f, 0.25f, 0.0f);
            bcc.isTrigger = false;
            bcc.debugDraw = true;
            floorObj->SetBoxCollider(bcc);

            srcScene->AddSceneObject(floorObj);
            floorObj->InitializeWithECS(srcCoordinator);

            auto sphereObj = std::make_shared<SceneObject>("SphereSensor");
            sphereObj->transform.SetPosition(Vector3(2.0f, 1.0f, 2.0f));
            
            SphereColliderComponent scc;
            scc.radius = 1.5f;
            scc.offset = Vector3(0.0f, 0.0f, 0.0f);
            scc.isTrigger = true;
            scc.debugDraw = false;
            sphereObj->SetSphereCollider(scc);

            srcScene->AddSceneObject(sphereObj);
            sphereObj->InitializeWithECS(srcCoordinator);

            auto capsuleObj = std::make_shared<SceneObject>("CapsuleWall");
            capsuleObj->transform.SetPosition(Vector3(-2.0f, 2.0f, -2.0f));
            
            CapsuleColliderComponent ccc;
            ccc.radius = 0.8f;
            ccc.height = 3.0f;
            ccc.offset = Vector3(0.1f, 0.2f, 0.3f);
            ccc.isTrigger = false;
            ccc.debugDraw = true;
            capsuleObj->SetCapsuleCollider(ccc);

            srcScene->AddSceneObject(capsuleObj);
            capsuleObj->InitializeWithECS(srcCoordinator);

            // 2. Validate Clamping & Validation rules
            // Box size clamp (MIN_COLLIDER_SIZE is 0.001f)
            BoxColliderComponent invalidBcc;
            invalidBcc.size = Vector3(0.0001f, -2.0f, 5.0f);
            eng::physics::ValidateBoxCollider(invalidBcc);
            if (invalidBcc.size.x < 0.001f || invalidBcc.size.y < 0.001f) {
                LOG_ERROR("[FormatTest] Test 9 FAILED: Box collider dimensions not clamped correctly!");
                delete srcScene;
                return false;
            }

            // Sphere radius clamp (MIN_COLLIDER_RADIUS is 0.001f)
            SphereColliderComponent invalidScc;
            invalidScc.radius = -0.5f;
            eng::physics::ValidateSphereCollider(invalidScc);
            if (invalidScc.radius < 0.001f) {
                LOG_ERROR("[FormatTest] Test 9 FAILED: Sphere collider radius not clamped correctly!");
                delete srcScene;
                return false;
            }

            // Capsule dimensions clamp (height >= 2.0f * radius, radius >= 0.001f)
            CapsuleColliderComponent invalidCcc;
            invalidCcc.radius = 1.0f;
            invalidCcc.height = 1.5f; // invalid (must be >= 2.0f * 1.0f = 2.0f)
            eng::physics::ValidateCapsuleCollider(invalidCcc);
            if (invalidCcc.height < 2.0f) {
                LOG_ERROR("[FormatTest] Test 9 FAILED: Capsule height not adjusted to at least 2x radius!");
                delete srcScene;
                return false;
            }

            // 3. Clone verification
            Coordinator destCoordinator;
            destCoordinator.Init();
            destCoordinator.RegisterComponent<TransformComponent>();
            destCoordinator.RegisterComponent<RigidBodyComponent>();
            destCoordinator.RegisterComponent<MeshRendererComponent>();
            destCoordinator.RegisterComponent<CameraComponent>();
            destCoordinator.RegisterComponent<LightComponent>();
            destCoordinator.RegisterComponent<ColliderComponent>();
            destCoordinator.RegisterComponent<PlayerControllerComponent>();
            destCoordinator.RegisterComponent<TagComponent>();
            destCoordinator.RegisterComponent<LayerComponent>();
            destCoordinator.RegisterComponent<HealthComponent>();
            destCoordinator.RegisterComponent<RenderableMeshComponent>();
            destCoordinator.RegisterComponent<MaterialComponent>();
            destCoordinator.RegisterComponent<NameComponent>();
            destCoordinator.RegisterComponent<StaticBodyComponent>();
            destCoordinator.RegisterComponent<BoxColliderComponent>();
            destCoordinator.RegisterComponent<SphereColliderComponent>();
            destCoordinator.RegisterComponent<CapsuleColliderComponent>();

            std::unordered_map<Entity, Entity> entityMap;
            Scene* destScene = srcScene->Clone(srcCoordinator, destCoordinator, entityMap);

            if (!srcScene->CompareScene(*destScene, entityMap, destCoordinator)) {
                LOG_ERROR("[FormatTest] Test 9 FAILED: Cloned scene structure or collider properties mismatched!");
                delete srcScene;
                delete destScene;
                return false;
            }

            // 4. Save and Load verification
            std::string tempPath = "Physics_Static_Test.omnixscene";
            if (!SceneSerializer::SaveScene(srcScene, tempPath)) {
                LOG_ERROR("[FormatTest] Test 9 FAILED: Could not serialize scene with colliders to JSON.");
                delete srcScene;
                delete destScene;
                std::filesystem::remove(tempPath);
                return false;
            }

            Scene* loadedScene = SceneLoader::LoadFromFile(tempPath);
            std::filesystem::remove(tempPath);

            if (!loadedScene) {
                LOG_ERROR("[FormatTest] Test 9 FAILED: Could not load scene containing colliders from JSON.");
                delete srcScene;
                delete destScene;
                return false;
            }

            // Verify loaded entities have correct properties
            auto loadedFloor = loadedScene->FindObjectByName("Floor");
            auto loadedSphere = loadedScene->FindObjectByName("SphereSensor");
            auto loadedCapsule = loadedScene->FindObjectByName("CapsuleWall");

            if (!loadedFloor || !loadedSphere || !loadedCapsule) {
                LOG_ERROR("[FormatTest] Test 9 FAILED: Missing loaded objects by name.");
                delete srcScene;
                delete destScene;
                delete loadedScene;
                return false;
            }

            if (!loadedFloor->m_HasStaticBody || !loadedFloor->m_HasBoxCollider ||
                loadedFloor->m_StaticBody.collisionLayer != 2 ||
                loadedFloor->m_BoxCollider.size.x != 10.0f ||
                loadedFloor->m_BoxCollider.offset.y != 0.25f) {
                LOG_ERROR("[FormatTest] Test 9 FAILED: Restored floor StaticBody/BoxCollider properties mismatched!");
                delete srcScene;
                delete destScene;
                delete loadedScene;
                return false;
            }

            if (!loadedSphere->m_HasSphereCollider ||
                loadedSphere->m_SphereCollider.radius != 1.5f ||
                loadedSphere->m_SphereCollider.isTrigger != true ||
                loadedSphere->m_SphereCollider.debugDraw != false) {
                LOG_ERROR("[FormatTest] Test 9 FAILED: Restored sphere SphereCollider properties mismatched!");
                delete srcScene;
                delete destScene;
                delete loadedScene;
                return false;
            }

            if (!loadedCapsule->m_HasCapsuleCollider ||
                loadedCapsule->m_CapsuleCollider.radius != 0.8f ||
                loadedCapsule->m_CapsuleCollider.height != 3.0f ||
                loadedCapsule->m_CapsuleCollider.offset.z != 0.3f) {
                LOG_ERROR("[FormatTest] Test 9 FAILED: Restored capsule CapsuleCollider properties mismatched!");
                delete srcScene;
                delete destScene;
                delete loadedScene;
                return false;
            }

            delete srcScene;
            delete destScene;
            delete loadedScene;
            LOG_INFO("[FormatTest] Test 9 Passed: Physics static collider authoring, validation clamping, cloning and serialization verified successfully.");
        }

        // -----------------------------------------------------------------------------
        // Test 10 — PhysX Static Actor Registration & Queries
        // -----------------------------------------------------------------------------
        LOG_INFO("[FormatTest] Running Test 10: PhysX Static Actor Registration & Queries...");
        {
            eng::physics::PhysicsWorld physicsWorld;
            if (!physicsWorld.Initialize()) {
                LOG_ERROR("[FormatTest] Test 10 FAILED: Could not initialize PhysicsWorld!");
                return false;
            }

            Coordinator coordinator;
            coordinator.Init();
            coordinator.RegisterComponent<TransformComponent>();
            coordinator.RegisterComponent<StaticBodyComponent>();
            coordinator.RegisterComponent<BoxColliderComponent>();
            coordinator.RegisterComponent<SphereColliderComponent>();
            coordinator.RegisterComponent<CapsuleColliderComponent>();

            // Entity 1: Floor
            Entity floorEntity = coordinator.CreateEntity();
            TransformComponent floorTrans;
            floorTrans.position = { 0.0f, 0.0f, 0.0f };
            floorTrans.scale = { 1.0f, 1.0f, 1.0f };
            coordinator.AddComponent<TransformComponent>(floorEntity, floorTrans);
            coordinator.AddComponent<StaticBodyComponent>(floorEntity, StaticBodyComponent{});
            BoxColliderComponent floorBox;
            floorBox.size = { 10.0f, 1.0f, 10.0f };
            floorBox.offset = { 0.0f, -0.5f, 0.0f };
            coordinator.AddComponent<BoxColliderComponent>(floorEntity, floorBox);

            // Entity 2: Sphere sensor
            Entity sphereEntity = coordinator.CreateEntity();
            TransformComponent sphereTrans;
            sphereTrans.position = { 0.0f, 5.0f, 0.0f };
            sphereTrans.scale = { 1.0f, 1.0f, 1.0f };
            coordinator.AddComponent<TransformComponent>(sphereEntity, sphereTrans);
            coordinator.AddComponent<StaticBodyComponent>(sphereEntity, StaticBodyComponent{});
            SphereColliderComponent sphereShape;
            sphereShape.radius = 1.0f;
            sphereShape.offset = { 0.0f, 0.0f, 0.0f };
            coordinator.AddComponent<SphereColliderComponent>(sphereEntity, sphereShape);

            // Entity 3: Capsule wall
            Entity capsuleEntity = coordinator.CreateEntity();
            TransformComponent capsuleTrans;
            capsuleTrans.position = { 5.0f, 0.0f, 0.0f };
            capsuleTrans.scale = { 1.0f, 1.0f, 1.0f };
            coordinator.AddComponent<TransformComponent>(capsuleEntity, capsuleTrans);
            coordinator.AddComponent<StaticBodyComponent>(capsuleEntity, StaticBodyComponent{});
            CapsuleColliderComponent capsuleShape;
            capsuleShape.radius = 0.5f;
            capsuleShape.height = 2.0f;
            capsuleShape.offset = { 0.0f, 0.0f, 0.0f };
            coordinator.AddComponent<CapsuleColliderComponent>(capsuleEntity, capsuleShape);

            // Register colliders
            physicsWorld.RegisterStaticColliders(coordinator);

            // Verify they are registered as 3 distinct static actors
            if (physicsWorld.GetStaticActorCount() != 3) {
                LOG_ERROR("[FormatTest] Test 10 FAILED: Expected 3 static actors, got %d", (int)physicsWorld.GetStaticActorCount());
                physicsWorld.Shutdown();
                return false;
            }

            // Perform a downward raycast and verify it hits the floor (bypass Sphere sensor by starting at x=2.0f)
            eng::physics::RaycastHit hit;
            Vector3 origin = { 2.0f, 10.0f, 0.0f };
            Vector3 direction = { 0.0f, -1.0f, 0.0f };
            bool rayOk = physicsWorld.Raycast(origin, direction, 20.0f, hit);
            if (!rayOk || !hit.hit || hit.entity != floorEntity) {
                LOG_ERROR("[FormatTest] Test 10 FAILED: Downward raycast failed to hit Floor correctly (hit = %d, entity = %u (expected %u))",
                          (int)hit.hit, (unsigned int)hit.entity, (unsigned int)floorEntity);
                physicsWorld.Shutdown();
                return false;
            }

            // Perform an overlap sphere test near the Sphere sensor
            std::vector<Entity> overlaps;
            bool overlapOk = physicsWorld.OverlapSphere({ 0.0f, 5.0f, 0.0f }, 1.5f, overlaps);
            if (!overlapOk || overlaps.empty() || std::find(overlaps.begin(), overlaps.end(), sphereEntity) == overlaps.end()) {
                LOG_ERROR("[FormatTest] Test 10 FAILED: Overlap sphere failed to detect Sphere sensor!");
                physicsWorld.Shutdown();
                return false;
            }

            // Reload the scene 5 times in a row and verify that the static actor count remains exactly 3
            for (int i = 0; i < 5; ++i) {
                physicsWorld.RegisterStaticColliders(coordinator);
                if (physicsWorld.GetStaticActorCount() != 3) {
                    LOG_ERROR("[FormatTest] Test 10 FAILED: Duplication safety check failed on iteration %d (actor count = %d)",
                              i, (int)physicsWorld.GetStaticActorCount());
                    physicsWorld.Shutdown();
                    return false;
                }
            }

            // Step the simulation
            physicsWorld.FixedUpdate(0.1f);

            physicsWorld.Shutdown();
            LOG_INFO("[FormatTest] Test 10 Passed: PhysX Static Actor registration, queries, and reload lifecycle safety verified successfully.");
        }

        // -----------------------------------------------------------------------------
        // Test 11 — Scene Validator & Load Gate
        // -----------------------------------------------------------------------------
        LOG_INFO("[FormatTest] Running Test 11: Scene Validator & Load Gate...");
        {
            SceneValidator validator;
            
            // 1. Verify invalid scenes are caught and block loading (have errors)
            SceneValidationReport reportDup = validator.ValidateSceneFile("Assets/Scenes/duplicate_names.omnixscene", nullptr);
            if (!reportDup.HasErrors()) {
                LOG_ERROR("[FormatTest] Test 11 FAILED: Expected duplicate_names.omnixscene to have errors!");
                return false;
            }
            LOG_INFO("[FormatTest] duplicate_names.omnixscene validation blocked correctly.");

            SceneValidationReport reportCycle = validator.ValidateSceneFile("Assets/Scenes/hierarchy_cycle.omnixscene", nullptr);
            if (!reportCycle.HasErrors()) {
                LOG_ERROR("[FormatTest] Test 11 FAILED: Expected hierarchy_cycle.omnixscene to have errors!");
                return false;
            }
            LOG_INFO("[FormatTest] hierarchy_cycle.omnixscene validation blocked correctly.");

            SceneValidationReport reportTransform = validator.ValidateSceneFile("Assets/Scenes/invalid_transform.omnixscene", nullptr);
            if (!reportTransform.HasErrors()) {
                LOG_ERROR("[FormatTest] Test 11 FAILED: Expected invalid_transform.omnixscene to have errors!");
                return false;
            }
            LOG_INFO("[FormatTest] invalid_transform.omnixscene validation blocked correctly.");

            // 2. Verify a valid scene passes validation
            SceneValidationReport reportValid = validator.ValidateSceneFile("Assets/Scenes/Lighting_Renderer_Test.omnixscene", nullptr);
            if (reportValid.HasErrors()) {
                LOG_ERROR("[FormatTest] Test 11 FAILED: Expected Lighting_Renderer_Test.omnixscene to pass validation, but got errors:\n%s", reportValid.ToString().c_str());
                return false;
            }
            SceneValidationReport reportRoom = validator.ValidateSceneFile("Assets/Scenes/test_room.omnixscene", nullptr);
            if (reportRoom.HasErrors()) {
                LOG_ERROR("[FormatTest] Test 11 FAILED: Expected test_room.omnixscene to pass validation, but got errors:\n%s", reportRoom.ToString().c_str());
                return false;
            }
            LOG_INFO("[FormatTest] test_room.omnixscene validation passed correctly.");

            LOG_INFO("[FormatTest] Test 11 Passed: SceneValidator blocks invalid scenes and allows valid scenes to proceed.");
        }

        // -----------------------------------------------------------------------------
        // Test 12 — OmnixWorld Format Tests
        // -----------------------------------------------------------------------------
        LOG_INFO("[FormatTest] Running Test 12: OmnixWorld Format Tests...");
        {
            using namespace Omnix;

            WorldDescriptor orig;
            orig.worldUUIDHigh = 0x1111222233334444ULL;
            orig.worldUUIDLow = 0x5555666677778888ULL;
            orig.worldName = "CampaignWorld";

            orig.settings.gravityX = 0.0f;
            orig.settings.gravityY = -9.81f;
            orig.settings.gravityZ = 0.0f;
            orig.settings.worldTimeScale = 1.0f;
            orig.settings.enableStreaming = 1;
            orig.settings.enablePhysics = 1;
            orig.settings.enableNavigation = 0;
            orig.settings.enableAudio = 1;

            std::strncpy(orig.entryPoint.entryZonePath, "Assets/Zones/StartZone.omnixzone", sizeof(orig.entryPoint.entryZonePath) - 1);
            orig.entryPoint.entryZonePath[sizeof(orig.entryPoint.entryZonePath) - 1] = '\0';
            orig.entryPoint.spawnPositionX = 1.0f;
            orig.entryPoint.spawnPositionY = 2.0f;
            orig.entryPoint.spawnPositionZ = 3.0f;
            orig.entryPoint.spawnRotationPitch = 0.0f;
            orig.entryPoint.spawnRotationYaw = 90.0f;
            orig.entryPoint.spawnRotationRoll = 0.0f;
            std::strncpy(orig.entryPoint.spawnTag, "SpawnPoint_Main", sizeof(orig.entryPoint.spawnTag) - 1);
            orig.entryPoint.spawnTag[sizeof(orig.entryPoint.spawnTag) - 1] = '\0';

            WorldZoneEntry zone1;
            zone1.zoneUUIDHigh = 100;
            zone1.zoneUUIDLow = 200;
            std::strncpy(zone1.zoneName, "Forest_01", sizeof(zone1.zoneName) - 1);
            zone1.zoneName[sizeof(zone1.zoneName) - 1] = '\0';
            std::strncpy(zone1.zonePath, "Assets/Zones/Forest_01.omnixzone", sizeof(zone1.zonePath) - 1);
            zone1.zonePath[sizeof(zone1.zonePath) - 1] = '\0';
            zone1.flags = 1;
            orig.zones.push_back(zone1);

            WorldZoneEntry zone2;
            zone2.zoneUUIDHigh = 300;
            zone2.zoneUUIDLow = 400;
            std::strncpy(zone2.zoneName, "Village_01", sizeof(zone2.zoneName) - 1);
            zone2.zoneName[sizeof(zone2.zoneName) - 1] = '\0';
            std::strncpy(zone2.zonePath, "Assets/Zones/Village_01.omnixzone", sizeof(zone2.zonePath) - 1);
            zone2.zonePath[sizeof(zone2.zonePath) - 1] = '\0';
            zone2.flags = 2;
            orig.zones.push_back(zone2);

            WorldDependencyEntry dep1;
            dep1.assetUUIDHigh = 999;
            dep1.assetUUIDLow = 888;
            std::strncpy(dep1.assetPath, "Assets/Models/Tree.obj", sizeof(dep1.assetPath) - 1);
            dep1.assetPath[sizeof(dep1.assetPath) - 1] = '\0';
            dep1.assetType = 4; // Mesh
            orig.dependencies.push_back(dep1);

            std::filesystem::path tempWorldPath = "test_world.omnixworld";
            
            // --- 1. Round trip test ---
            WorldFileResult writeRes = WorldFileWriter::WriteToFile(tempWorldPath, orig);
            if (!writeRes.Success()) {
                LOG_ERROR("[FormatTest] Test 12 FAILED: WorldFileWriter returned error code %d", static_cast<int>(writeRes.error));
                std::filesystem::remove(tempWorldPath);
                return false;
            }

            WorldDescriptor loaded;
            WorldFileResult readRes = WorldFileReader::ReadFromFile(tempWorldPath, loaded);
            if (!readRes.Success()) {
                LOG_ERROR("[FormatTest] Test 12 FAILED: WorldFileReader returned error code %d", static_cast<int>(readRes.error));
                std::filesystem::remove(tempWorldPath);
                return false;
            }

            if (loaded.worldUUIDHigh != orig.worldUUIDHigh ||
                loaded.worldUUIDLow != orig.worldUUIDLow ||
                loaded.worldName != orig.worldName ||
                loaded.settings.gravityY != orig.settings.gravityY ||
                loaded.settings.enableStreaming != orig.settings.enableStreaming ||
                std::strcmp(loaded.entryPoint.entryZonePath, orig.entryPoint.entryZonePath) != 0 ||
                loaded.entryPoint.spawnRotationYaw != orig.entryPoint.spawnRotationYaw ||
                loaded.zones.size() != orig.zones.size() ||
                loaded.dependencies.size() != orig.dependencies.size() ||
                loaded.zones[0].zoneUUIDHigh != orig.zones[0].zoneUUIDHigh ||
                std::strcmp(loaded.zones[1].zoneName, orig.zones[1].zoneName) != 0 ||
                std::strcmp(loaded.dependencies[0].assetPath, orig.dependencies[0].assetPath) != 0) {
                LOG_ERROR("[FormatTest] Test 12 FAILED: Parsed WorldDescriptor mismatched original!");
                std::filesystem::remove(tempWorldPath);
                return false;
            }
            LOG_INFO("[FormatTest]   -> Round trip OK.");

            // --- 2. Unsupported future version test ---
            {
                std::ifstream inFile(tempWorldPath, std::ios::binary);
                std::vector<uint8_t> bytes((std::istreambuf_iterator<char>(inFile)), std::istreambuf_iterator<char>());
                inFile.close();

                if (bytes.size() >= sizeof(OmnixWorldHeader)) {
                    OmnixWorldHeader* rawHeader = reinterpret_cast<OmnixWorldHeader*>(bytes.data());
                    rawHeader->version = OMNIX_WORLD_VERSION + 1;
                    
                    std::memset(bytes.data() + rawHeader->checksumOffset, 0, sizeof(uint32_t));
                    uint32_t newCRC = SerializationCommon::CalculateCRC32(bytes.data(), bytes.size());
                    std::memcpy(bytes.data() + rawHeader->checksumOffset, &newCRC, sizeof(uint32_t));

                    std::ofstream outFile(tempWorldPath, std::ios::binary);
                    outFile.write(reinterpret_cast<const char*>(bytes.data()), bytes.size());
                    outFile.close();
                }

                WorldDescriptor badVersionWorld;
                WorldFileResult badVerRes = WorldFileReader::ReadFromFile(tempWorldPath, badVersionWorld);
                if (badVerRes.Success() || badVerRes.error != WorldFileError::UnsupportedVersion) {
                    LOG_ERROR("[FormatTest] Test 12 FAILED: Expected UnsupportedVersion error, got: %d", static_cast<int>(badVerRes.error));
                    std::filesystem::remove(tempWorldPath);
                    return false;
                }
                LOG_INFO("[FormatTest]   -> Unsupported version rejection OK.");
            }

            // Restore valid file
            WorldFileWriter::WriteToFile(tempWorldPath, orig);

            // --- 3. Checksum mismatch test ---
            {
                std::ifstream inFile(tempWorldPath, std::ios::binary);
                std::vector<uint8_t> bytes((std::istreambuf_iterator<char>(inFile)), std::istreambuf_iterator<char>());
                inFile.close();

                if (bytes.size() > 100) {
                    bytes[100] ^= 0x55;
                }

                std::ofstream outFile(tempWorldPath, std::ios::binary);
                outFile.write(reinterpret_cast<const char*>(bytes.data()), bytes.size());
                outFile.close();

                WorldDescriptor corruptedWorld;
                WorldFileResult corruptRes = WorldFileReader::ReadFromFile(tempWorldPath, corruptedWorld);
                if (corruptRes.Success() || corruptRes.error != WorldFileError::ChecksumMismatch) {
                    LOG_ERROR("[FormatTest] Test 12 FAILED: Expected ChecksumMismatch error, got: %d", static_cast<int>(corruptRes.error));
                    std::filesystem::remove(tempWorldPath);
                    return false;
                }
                LOG_INFO("[FormatTest]   -> Checksum mismatch rejection OK.");
            }

            // --- 4. Empty and too small file checks ---
            {
                std::string emptyFile = "test_empty.omnixworld";
                std::ofstream outFile(emptyFile, std::ios::binary);
                outFile.close();

                WorldDescriptor dummy;
                WorldFileResult emptyRes = WorldFileReader::ReadFromFile(emptyFile, dummy);
                std::filesystem::remove(emptyFile);
                if (emptyRes.Success() || emptyRes.error != WorldFileError::FileTooSmall) {
                    LOG_ERROR("[FormatTest] Test 12 FAILED: Expected FileTooSmall error for empty file, got: %d", static_cast<int>(emptyRes.error));
                    std::filesystem::remove(tempWorldPath);
                    return false;
                }
                LOG_INFO("[FormatTest]   -> Empty/too small file rejection OK.");
            }

            // --- 5. Determinism test ---
            {
                WorldFileWriter::WriteToFile(tempWorldPath, orig);

                std::ifstream inFile(tempWorldPath, std::ios::binary);
                std::vector<uint8_t> bytesA((std::istreambuf_iterator<char>(inFile)), std::istreambuf_iterator<char>());
                inFile.close();

                WorldFileWriter::WriteToFile(tempWorldPath, orig);

                std::ifstream inFile2(tempWorldPath, std::ios::binary);
                std::vector<uint8_t> bytesB((std::istreambuf_iterator<char>(inFile2)), std::istreambuf_iterator<char>());
                inFile2.close();

                if (bytesA != bytesB) {
                    LOG_ERROR("[FormatTest] Test 12 FAILED: Binary output is not deterministic!");
                    std::filesystem::remove(tempWorldPath);
                    return false;
                }

                WorldDescriptor descA;
                WorldDescriptor descB;
                WorldFileReader::ReadFromFile(tempWorldPath, descA);
                WorldFileReader::ReadFromFile(tempWorldPath, descB);

                if (descA.worldName != descB.worldName ||
                    descA.zones.size() != descB.zones.size() ||
                    descA.dependencies.size() != descB.dependencies.size() ||
                    std::strcmp(descA.entryPoint.spawnTag, descB.entryPoint.spawnTag) != 0) {
                    LOG_ERROR("[FormatTest] Test 12 FAILED: Parsed descriptor is not deterministic!");
                    std::filesystem::remove(tempWorldPath);
                    return false;
                }
                LOG_INFO("[FormatTest]   -> Deterministic serialization & parsing OK.");
            }

            std::filesystem::remove(tempWorldPath);
            LOG_INFO("[FormatTest] Test 12 Passed: OmnixWorld Format validation verified successfully.");
        }

        // -----------------------------------------------------------------------------
        // Test 13 — WorldZone Format Tests & Validation
        // -----------------------------------------------------------------------------
        LOG_INFO("[FormatTest] Running Test 13: WorldZone Format & Validation Tests...");
        {
            using namespace Omnix;

            // --- 1. Bounds validation checks ---
            WorldZone zoneBoundsTest;
            zoneBoundsTest.bounds.min = {0.0f, 0.0f, 0.0f};
            zoneBoundsTest.bounds.max = {10.0f, 10.0f, 10.0f};
            if (!ValidateZoneBounds(zoneBoundsTest)) {
                LOG_ERROR("[FormatTest] Test 13 FAILED: Valid bounds failed validation!");
                return false;
            }

            // Min > Max
            zoneBoundsTest.bounds.min = {15.0f, 0.0f, 0.0f};
            zoneBoundsTest.bounds.max = {10.0f, 10.0f, 10.0f};
            if (ValidateZoneBounds(zoneBoundsTest)) {
                LOG_ERROR("[FormatTest] Test 13 FAILED: Invalid bounds (min.x > max.x) passed validation!");
                return false;
            }

            // NaN
            zoneBoundsTest.bounds.min = {0.0f, NAN, 0.0f};
            zoneBoundsTest.bounds.max = {10.0f, 10.0f, 10.0f};
            if (ValidateZoneBounds(zoneBoundsTest)) {
                LOG_ERROR("[FormatTest] Test 13 FAILED: Invalid bounds (NaN) passed validation!");
                return false;
            }
            LOG_INFO("[FormatTest]   -> Bounds validation OK.");

            // --- 2. ID uniqueness checks ---
            std::vector<WorldZone> zoneList;
            WorldZone z1; z1.zoneUUIDHigh = 1; z1.zoneUUIDLow = 1;
            WorldZone z2; z2.zoneUUIDHigh = 1; z2.zoneUUIDLow = 2;
            WorldZone z3; z3.zoneUUIDHigh = 1; z3.zoneUUIDLow = 1; // Duplicate of z1
            
            zoneList.push_back(z1);
            zoneList.push_back(z2);
            if (!ValidateZoneIDsUnique(zoneList)) {
                LOG_ERROR("[FormatTest] Test 13 FAILED: Unique IDs failed uniqueness check!");
                return false;
            }

            zoneList.push_back(z3);
            if (ValidateZoneIDsUnique(zoneList)) {
                LOG_ERROR("[FormatTest] Test 13 FAILED: Duplicate IDs passed uniqueness check!");
                return false;
            }
            LOG_INFO("[FormatTest]   -> ID uniqueness validation OK.");

            // --- 3. Round trip serialization check ---
            WorldZone origZone;
            origZone.zoneUUIDHigh = 0xABCDEULL;
            origZone.zoneUUIDLow = 0x12345ULL;
            origZone.zoneName = "ForestSector";
            origZone.sceneAssetPath = "Assets/Scenes/Forest.omnixscene";
            origZone.bounds.min = {-100.0f, -50.0f, -100.0f};
            origZone.bounds.max = {100.0f, 50.0f, 100.0f};
            origZone.loadingPriority = 5;
            origZone.activationRadius = 250.0f;
            origZone.state = ZoneState::Active;

            ZoneAssetDependency d1;
            d1.assetUUIDHigh = 11;
            d1.assetUUIDLow = 22;
            d1.assetPath = "Assets/Textures/Bark.png";
            d1.assetType = 2; // Texture
            origZone.assetDependencies.push_back(d1);

            ZoneNeighbor n1;
            n1.zoneUUIDHigh = 0xFEEDBEEF;
            n1.zoneUUIDLow = 0xDECAFBAD;
            origZone.neighbors.push_back(n1);

            origZone.gameplayTags.push_back("CombatZone");
            origZone.gameplayTags.push_back("Outdoor");

            // Validate the zone itself
            if (!ValidateWorldZone(origZone)) {
                LOG_ERROR("[FormatTest] Test 13 FAILED: Valid WorldZone failed validation check!");
                return false;
            }

            std::filesystem::path tempZonePath = "test_zone.omnixzone";
            WorldFileResult writeRes = WorldZoneWriter::WriteToFile(tempZonePath, origZone);
            if (!writeRes.Success()) {
                LOG_ERROR("[FormatTest] Test 13 FAILED: WorldZoneWriter returned error code %d", static_cast<int>(writeRes.error));
                std::filesystem::remove(tempZonePath);
                return false;
            }

            WorldZone loadedZone;
            WorldFileResult readRes = WorldZoneReader::ReadFromFile(tempZonePath, loadedZone);
            if (!readRes.Success()) {
                LOG_ERROR("[FormatTest] Test 13 FAILED: WorldZoneReader returned error code %d", static_cast<int>(readRes.error));
                std::filesystem::remove(tempZonePath);
                return false;
            }

            if (loadedZone.zoneUUIDHigh != origZone.zoneUUIDHigh ||
                loadedZone.zoneUUIDLow != origZone.zoneUUIDLow ||
                loadedZone.zoneName != origZone.zoneName ||
                loadedZone.sceneAssetPath != origZone.sceneAssetPath ||
                loadedZone.bounds.min.x != origZone.bounds.min.x ||
                loadedZone.bounds.max.z != origZone.bounds.max.z ||
                loadedZone.loadingPriority != origZone.loadingPriority ||
                loadedZone.activationRadius != origZone.activationRadius ||
                loadedZone.assetDependencies.size() != origZone.assetDependencies.size() ||
                loadedZone.neighbors.size() != origZone.neighbors.size() ||
                loadedZone.gameplayTags.size() != origZone.gameplayTags.size() ||
                loadedZone.assetDependencies[0].assetUUIDHigh != origZone.assetDependencies[0].assetUUIDHigh ||
                loadedZone.assetDependencies[0].assetPath != origZone.assetDependencies[0].assetPath ||
                loadedZone.neighbors[0].zoneUUIDHigh != origZone.neighbors[0].zoneUUIDHigh ||
                loadedZone.gameplayTags[0] != origZone.gameplayTags[0] ||
                loadedZone.state != ZoneState::Unloaded) // Loaded zones start as Unloaded
            {
                LOG_ERROR("[FormatTest] Test 13 FAILED: Deserialized WorldZone mismatched original!");
                std::filesystem::remove(tempZonePath);
                return false;
            }
            LOG_INFO("[FormatTest]   -> Round trip serialization OK.");

            // --- 4. Unsupported future version test ---
            {
                std::ifstream inFile(tempZonePath, std::ios::binary);
                std::vector<uint8_t> bytes((std::istreambuf_iterator<char>(inFile)), std::istreambuf_iterator<char>());
                inFile.close();

                if (bytes.size() >= sizeof(OmnixZoneHeader)) {
                    OmnixZoneHeader* rawHeader = reinterpret_cast<OmnixZoneHeader*>(bytes.data());
                    rawHeader->version = OMNIX_ZONE_VERSION + 1;
                    
                    std::memset(bytes.data() + rawHeader->checksumOffset, 0, sizeof(uint32_t));
                    uint32_t newCRC = SerializationCommon::CalculateCRC32(bytes.data(), bytes.size());
                    std::memcpy(bytes.data() + rawHeader->checksumOffset, &newCRC, sizeof(uint32_t));

                    std::ofstream outFile(tempZonePath, std::ios::binary);
                    outFile.write(reinterpret_cast<const char*>(bytes.data()), bytes.size());
                    outFile.close();
                }

                WorldZone badVerZone;
                WorldFileResult badVerRes = WorldZoneReader::ReadFromFile(tempZonePath, badVerZone);
                if (badVerRes.Success() || badVerRes.error != WorldFileError::UnsupportedVersion) {
                    LOG_ERROR("[FormatTest] Test 13 FAILED: Expected UnsupportedVersion error, got: %d", static_cast<int>(badVerRes.error));
                    std::filesystem::remove(tempZonePath);
                    return false;
                }
                LOG_INFO("[FormatTest]   -> Unsupported version rejection OK.");
            }

            // Restore valid file
            WorldZoneWriter::WriteToFile(tempZonePath, origZone);

            // --- 5. Checksum mismatch test ---
            {
                std::ifstream inFile(tempZonePath, std::ios::binary);
                std::vector<uint8_t> bytes((std::istreambuf_iterator<char>(inFile)), std::istreambuf_iterator<char>());
                inFile.close();

                if (bytes.size() > 100) {
                    bytes[100] ^= 0xFF;
                }

                std::ofstream outFile(tempZonePath, std::ios::binary);
                outFile.write(reinterpret_cast<const char*>(bytes.data()), bytes.size());
                outFile.close();

                WorldZone corruptedZone;
                WorldFileResult corruptRes = WorldZoneReader::ReadFromFile(tempZonePath, corruptedZone);
                if (corruptRes.Success() || corruptRes.error != WorldFileError::ChecksumMismatch) {
                    LOG_ERROR("[FormatTest] Test 13 FAILED: Expected ChecksumMismatch error, got: %d", static_cast<int>(corruptRes.error));
                    std::filesystem::remove(tempZonePath);
                    return false;
                }
                LOG_INFO("[FormatTest]   -> Checksum mismatch rejection OK.");
            }

            // --- 6. Empty and too small file checks ---
            {
                std::string emptyFile = "test_empty.omnixzone";
                std::ofstream outFile(emptyFile, std::ios::binary);
                outFile.close();

                WorldZone dummy;
                WorldFileResult emptyRes = WorldZoneReader::ReadFromFile(emptyFile, dummy);
                std::filesystem::remove(emptyFile);
                if (emptyRes.Success() || emptyRes.error != WorldFileError::FileTooSmall) {
                    LOG_ERROR("[FormatTest] Test 13 FAILED: Expected FileTooSmall error for empty file, got: %d", static_cast<int>(emptyRes.error));
                    std::filesystem::remove(tempZonePath);
                    return false;
                }
                LOG_INFO("[FormatTest]   -> Empty/too small file rejection OK.");
            }

            // --- 7. Determinism test ---
            {
                WorldZoneWriter::WriteToFile(tempZonePath, origZone);

                std::ifstream inFile(tempZonePath, std::ios::binary);
                std::vector<uint8_t> bytesA((std::istreambuf_iterator<char>(inFile)), std::istreambuf_iterator<char>());
                inFile.close();

                WorldZoneWriter::WriteToFile(tempZonePath, origZone);

                std::ifstream inFile2(tempZonePath, std::ios::binary);
                std::vector<uint8_t> bytesB((std::istreambuf_iterator<char>(inFile2)), std::istreambuf_iterator<char>());
                inFile2.close();

                if (bytesA != bytesB) {
                    LOG_ERROR("[FormatTest] Test 13 FAILED: Binary output is not deterministic!");
                    std::filesystem::remove(tempZonePath);
                    return false;
                }

                WorldZone zoneA;
                WorldZone zoneB;
                WorldZoneReader::ReadFromFile(tempZonePath, zoneA);
                WorldZoneReader::ReadFromFile(tempZonePath, zoneB);

                if (zoneA.zoneName != zoneB.zoneName ||
                    zoneA.assetDependencies.size() != zoneB.assetDependencies.size() ||
                    zoneA.neighbors.size() != zoneB.neighbors.size() ||
                    zoneA.gameplayTags.size() != zoneB.gameplayTags.size()) {
                    LOG_ERROR("[FormatTest] Test 13 FAILED: Parsed zone is not deterministic!");
                    std::filesystem::remove(tempZonePath);
                    return false;
                }
                LOG_INFO("[FormatTest]   -> Deterministic serialization & parsing OK.");
            }

            std::filesystem::remove(tempZonePath);
            LOG_INFO("[FormatTest] Test 13 Passed: WorldZone Format and validation verified successfully.");
        }

        // -----------------------------------------------------------------------------
        // Test 14 — WorldManager Subsystem Validation Tests
        // -----------------------------------------------------------------------------
        LOG_INFO("[FormatTest] Running Test 14: WorldManager Subsystem Validation Tests...");
        {
            using namespace Omnix;

            // Define MockAssetManager inside the test scope
            class MockAssetManager : public eng::runtime::IAssetManager {
            public:
                struct LoadRecord {
                    std::string path;
                    const std::type_info* typeInfo;
                };
                std::vector<LoadRecord> loadedAssets;

            protected:
                void* LoadRaw(const std::string& path, const std::type_info& typeInfo) override {
                    loadedAssets.push_back({path, &typeInfo});
                    return nullptr; 
                }
            };

            std::filesystem::path tempZonePath = "test_manager_zone.omnixzone";
            std::filesystem::path tempWorldPath = "test_manager_world.omnixworld";

            // 1. Write a mock zone file
            WorldZone zone;
            zone.zoneUUIDHigh = 0xAAULL;
            zone.zoneUUIDLow = 0xBBULL;
            zone.zoneName = "ManagerTestZone";
            zone.sceneAssetPath = "Assets/Scenes/ManagerTest.omnixscene";
            zone.bounds.min = {-10.0f, -10.0f, -10.0f};
            zone.bounds.max = {10.0f, 10.0f, 10.0f};
            zone.loadingPriority = 1;
            zone.activationRadius = 100.0f;
            zone.state = ZoneState::Unloaded;

            ZoneAssetDependency d1;
            d1.assetUUIDHigh = 101; d1.assetUUIDLow = 201;
            d1.assetPath = "Assets/Textures/Bark.png";
            d1.assetType = static_cast<uint32_t>(AssetType::Texture);
            zone.assetDependencies.push_back(d1);

            ZoneAssetDependency d2;
            d2.assetUUIDHigh = 102; d2.assetUUIDLow = 202;
            d2.assetPath = "Assets/Meshes/Tree.omnixmesh";
            d2.assetType = static_cast<uint32_t>(AssetType::Mesh);
            zone.assetDependencies.push_back(d2);

            ZoneAssetDependency d3;
            d3.assetUUIDHigh = 103; d3.assetUUIDLow = 203;
            d3.assetPath = "Assets/Materials/Bark.omnixmaterial";
            d3.assetType = static_cast<uint32_t>(AssetType::Material);
            zone.assetDependencies.push_back(d3);

            WorldFileResult zWrite = WorldZoneWriter::WriteToFile(tempZonePath, zone);
            if (!zWrite.Success()) {
                LOG_ERROR("[FormatTest] Test 14 FAILED: Failed to write mock zone file!");
                return false;
            }

            // 2. Write a mock world file
            WorldDescriptor world;
            world.worldUUIDHigh = 0x11ULL;
            world.worldUUIDLow = 0x22ULL;
            world.worldName = "ManagerTestWorld";
            world.settings.gravityY = -9.81f;
            world.entryPoint.spawnPositionX = 0.0f;
            world.entryPoint.spawnPositionY = 2.0f;
            world.entryPoint.spawnPositionZ = 0.0f;

            WorldZoneEntry entry;
            entry.zoneUUIDHigh = zone.zoneUUIDHigh;
            entry.zoneUUIDLow = zone.zoneUUIDLow;
            std::strncpy(entry.zonePath, tempZonePath.string().c_str(), sizeof(entry.zonePath) - 1);
            entry.zonePath[sizeof(entry.zonePath) - 1] = '\0';
            std::strncpy(entry.zoneName, zone.zoneName.c_str(), sizeof(entry.zoneName) - 1);
            entry.zoneName[sizeof(entry.zoneName) - 1] = '\0';
            world.zones.push_back(entry);

            WorldFileResult wWrite = WorldFileWriter::WriteToFile(tempWorldPath, world);
            if (!wWrite.Success()) {
                LOG_ERROR("[FormatTest] Test 14 FAILED: Failed to write mock world file!");
                std::filesystem::remove(tempZonePath);
                return false;
            }

            // 3. Test loading
            {
                eng::runtime::AssetRegistry registry;
                MockAssetManager assetManager;
                WorldManager manager(&assetManager, &registry);

                WorldFileResult loadRes = manager.LoadWorld(tempWorldPath);
                if (!loadRes.Success()) {
                    LOG_ERROR("[FormatTest] Test 14 FAILED: LoadWorld failed with error code %d", static_cast<int>(loadRes.error));
                    std::filesystem::remove(tempZonePath);
                    std::filesystem::remove(tempWorldPath);
                    return false;
                }

                if (!manager.HasActiveWorld() || manager.GetActiveWorld() == nullptr) {
                    LOG_ERROR("[FormatTest] Test 14 FAILED: Active world not set after LoadWorld!");
                    std::filesystem::remove(tempZonePath);
                    std::filesystem::remove(tempWorldPath);
                    return false;
                }

                if (manager.GetActiveWorld()->worldName != "ManagerTestWorld") {
                    LOG_ERROR("[FormatTest] Test 14 FAILED: Loaded world name mismatch!");
                    std::filesystem::remove(tempZonePath);
                    std::filesystem::remove(tempWorldPath);
                    return false;
                }

                if (manager.GetLoadedZones().size() != 1) {
                    LOG_ERROR("[FormatTest] Test 14 FAILED: Loaded zone count mismatch!");
                    std::filesystem::remove(tempZonePath);
                    std::filesystem::remove(tempWorldPath);
                    return false;
                }

                // Verify registered assets
                auto checkRegistered = [&](const std::string& path, AssetType type) {
                    AssetHandle h = eng::runtime::GenerateAssetUUID(path, type);
                    if (!registry.Contains(h)) {
                        LOG_ERROR("[FormatTest] Test 14 FAILED: Asset '%s' not registered in registry!", path.c_str());
                        return false;
                    }
                    return true;
                };

                if (!checkRegistered(tempWorldPath.string(), AssetType::Unknown) ||
                    !checkRegistered("Assets/Scenes/ManagerTest.omnixscene", AssetType::Scene) ||
                    !checkRegistered("Assets/Textures/Bark.png", AssetType::Texture) ||
                    !checkRegistered("Assets/Meshes/Tree.omnixmesh", AssetType::Mesh) ||
                    !checkRegistered("Assets/Materials/Bark.omnixmaterial", AssetType::Material)) 
                {
                    std::filesystem::remove(tempZonePath);
                    std::filesystem::remove(tempWorldPath);
                    return false;
                }

                // Verify loaded assets in MockAssetManager
                if (assetManager.loadedAssets.size() != 4) {
                    LOG_ERROR("[FormatTest] Test 14 FAILED: Expected 4 loaded assets in manager, got %d!", static_cast<int>(assetManager.loadedAssets.size()));
                    std::filesystem::remove(tempZonePath);
                    std::filesystem::remove(tempWorldPath);
                    return false;
                }

                auto findLoadRecord = [&](const std::string& path, const std::type_info& expectedType) {
                    for (const auto& rec : assetManager.loadedAssets) {
                        if (rec.path == path && *rec.typeInfo == expectedType) {
                            return true;
                        }
                    }
                    return false;
                };

                if (!findLoadRecord("Assets/Scenes/ManagerTest.omnixscene", typeid(Scene)) ||
                    !findLoadRecord("Assets/Textures/Bark.png", typeid(eng::renderer::Texture)) ||
                    !findLoadRecord("Assets/Meshes/Tree.omnixmesh", typeid(eng::renderer::Mesh)) ||
                    !findLoadRecord("Assets/Materials/Bark.omnixmaterial", typeid(eng::renderer::Material)))
                {
                    LOG_ERROR("[FormatTest] Test 14 FAILED: Assets not correctly loaded with correct types in IAssetManager!");
                    std::filesystem::remove(tempZonePath);
                    std::filesystem::remove(tempWorldPath);
                    return false;
                }

                // Test UnloadWorld
                manager.UnloadWorld();
                if (manager.HasActiveWorld() || manager.GetActiveWorld() != nullptr) {
                    LOG_ERROR("[FormatTest] Test 14 FAILED: HasActiveWorld true after UnloadWorld!");
                    std::filesystem::remove(tempZonePath);
                    std::filesystem::remove(tempWorldPath);
                    return false;
                }

                if (!manager.GetLoadedZones().empty()) {
                    LOG_ERROR("[FormatTest] Test 14 FAILED: Loaded zones not empty after UnloadWorld!");
                    std::filesystem::remove(tempZonePath);
                    std::filesystem::remove(tempWorldPath);
                    return false;
                }
            }

            // 4. Stress loading: run 50 load/unload cycles
            {
                eng::runtime::AssetRegistry registry;
                MockAssetManager assetManager;
                WorldManager manager(&assetManager, &registry);

                LOG_INFO("[FormatTest]   -> Running 50 load/unload stress cycles...");
                for (int i = 0; i < 50; ++i) {
                    WorldFileResult res = manager.LoadWorld(tempWorldPath);
                    if (!res.Success()) {
                        LOG_ERROR("[FormatTest] Test 14 FAILED: Failed during stress cycle %d", i);
                        std::filesystem::remove(tempZonePath);
                        std::filesystem::remove(tempWorldPath);
                        return false;
                    }
                    manager.UnloadWorld();
                }
                LOG_INFO("[FormatTest]   -> 50 cycles completed successfully without memory leaks or crashes.");
            }

            std::filesystem::remove(tempZonePath);
            std::filesystem::remove(tempWorldPath);
            LOG_INFO("[FormatTest] Test 14 Passed: WorldManager lifecycle verified successfully.");
        }

        // -----------------------------------------------------------------------------
        // Test 15 — Zone Activation and Transition Tests
        // -----------------------------------------------------------------------------
        LOG_INFO("[FormatTest] Running Test 15: Zone Activation and Transition Tests...");
        {
            using namespace Omnix;

            // 1. Create two mock zone scene files
            std::filesystem::path tempSceneAPath = "test_zone_a.omnixscene";
            std::filesystem::path tempSceneBPath = "test_zone_b.omnixscene";

            Scene* sceneA = new Scene("SceneA");
            auto objA = std::make_shared<SceneObject>("ObjectInZoneA");
            sceneA->AddSceneObject(objA);
            if (!SceneSerializer::SaveScene(sceneA, tempSceneAPath.string())) {
                LOG_ERROR("[FormatTest] Test 15 FAILED: Failed to save scene A!");
                delete sceneA;
                return false;
            }
            delete sceneA;

            Scene* sceneB = new Scene("SceneB");
            auto objB = std::make_shared<SceneObject>("ObjectInZoneB");
            sceneB->AddSceneObject(objB);
            if (!SceneSerializer::SaveScene(sceneB, tempSceneBPath.string())) {
                LOG_ERROR("[FormatTest] Test 15 FAILED: Failed to save scene B!");
                delete sceneB;
                return false;
            }
            delete sceneB;

            // 2. Create mock zone descriptors
            std::filesystem::path tempZoneAPath = "test_zone_a.omnixzone";
            std::filesystem::path tempZoneBPath = "test_zone_b.omnixzone";

            WorldZone zoneA;
            zoneA.zoneUUIDHigh = 0x1111ULL;
            zoneA.zoneUUIDLow = 0x2222ULL;
            zoneA.zoneName = "ZoneA";
            zoneA.sceneAssetPath = tempSceneAPath.string();
            zoneA.bounds.min = {-10.0f, -10.0f, -10.0f};
            zoneA.bounds.max = {10.0f, 10.0f, 10.0f};
            zoneA.state = ZoneState::Unloaded;

            WorldZone zoneB;
            zoneB.zoneUUIDHigh = 0x3333ULL;
            zoneB.zoneUUIDLow = 0x4444ULL;
            zoneB.zoneName = "ZoneB";
            zoneB.sceneAssetPath = tempSceneBPath.string();
            zoneB.bounds.min = {10.0f, -10.0f, -10.0f};
            zoneB.bounds.max = {30.0f, 10.0f, 10.0f};
            zoneB.state = ZoneState::Unloaded;

            if (!WorldZoneWriter::WriteToFile(tempZoneAPath, zoneA).Success()) {
                LOG_ERROR("[FormatTest] Test 15 FAILED: Failed to write zone A!");
                return false;
            }
            if (!WorldZoneWriter::WriteToFile(tempZoneBPath, zoneB).Success()) {
                LOG_ERROR("[FormatTest] Test 15 FAILED: Failed to write zone B!");
                return false;
            }

            // 3. Create mock world descriptor
            std::filesystem::path tempWorldPath = "test_world.omnixworld";
            WorldDescriptor world;
            world.worldUUIDHigh = 0x9999ULL;
            world.worldUUIDLow = 0x8888ULL;
            world.worldName = "ActivationTestWorld";
            
            WorldZoneEntry entryA;
            entryA.zoneUUIDHigh = zoneA.zoneUUIDHigh;
            entryA.zoneUUIDLow = zoneA.zoneUUIDLow;
            std::strncpy(entryA.zoneName, zoneA.zoneName.c_str(), sizeof(entryA.zoneName) - 1);
            entryA.zoneName[sizeof(entryA.zoneName) - 1] = '\0';
            std::strncpy(entryA.zonePath, tempZoneAPath.string().c_str(), sizeof(entryA.zonePath) - 1);
            entryA.zonePath[sizeof(entryA.zonePath) - 1] = '\0';
            world.zones.push_back(entryA);

            WorldZoneEntry entryB;
            entryB.zoneUUIDHigh = zoneB.zoneUUIDHigh;
            entryB.zoneUUIDLow = zoneB.zoneUUIDLow;
            std::strncpy(entryB.zoneName, zoneB.zoneName.c_str(), sizeof(entryB.zoneName) - 1);
            entryB.zoneName[sizeof(entryB.zoneName) - 1] = '\0';
            std::strncpy(entryB.zonePath, tempZoneBPath.string().c_str(), sizeof(entryB.zonePath) - 1);
            entryB.zonePath[sizeof(entryB.zonePath) - 1] = '\0';
            world.zones.push_back(entryB);

            if (!WorldFileWriter::WriteToFile(tempWorldPath, world).Success()) {
                LOG_ERROR("[FormatTest] Test 15 FAILED: Failed to write world file!");
                return false;
            }

            // 4. Initialize local subsystems for testing
            AssetRegistry registry;
            class MockAssetManager : public eng::runtime::IAssetManager {
            protected:
                void* LoadRaw(const std::string& path, const std::type_info& typeInfo) override {
                    return nullptr; 
                }
            };
            MockAssetManager assetManager;

            auto ecsWorld = std::make_unique<World>();
            auto sceneManager = std::make_unique<SceneManager>(&ecsWorld->getCoordinator());
            sceneManager->SetAssetRegistry(&registry);
            sceneManager->CreateNewScene("TestActiveScene");

            // Register player
            auto& coordinator = ecsWorld->getCoordinator();
            uint32_t player = coordinator.CreateEntity();
            
            TransformComponent transform;
            transform.position = {0.0f, 0.0f, 0.0f};
            coordinator.AddComponent<TransformComponent>(player, transform);
            
            CharacterControllerComponent ccc;
            coordinator.AddComponent<CharacterControllerComponent>(player, ccc);

            if (auto playerControllerSys = ecsWorld->GetSystem<PlayerControllerSystem>()) {
                playerControllerSys->SetPlayerEntity(player);
            }

            // Set up a mock gameplay event bus
            eng::runtime::GameplayEventBus eventBus;
            std::vector<eng::runtime::GameplayEvent> receivedEvents;
            eventBus.Subscribe(eng::runtime::GameplayEventType::ZoneEnter, [&](const eng::runtime::GameplayEvent& ev) {
                receivedEvents.push_back(ev);
            });
            eventBus.Subscribe(eng::runtime::GameplayEventType::ZoneExit, [&](const eng::runtime::GameplayEvent& ev) {
                receivedEvents.push_back(ev);
            });

            // Set up RuntimeContext
            eng::runtime::RuntimeContext context;
            context.mode = RuntimeMode::Game;
            context.editorSimulationState = EditorSimulationState::Play;
            context.assets = &assetManager;
            context.assetRegistry = &registry;
            context.scenes = sceneManager.get();
            context.ecs = ecsWorld.get();
            context.gameplayEventBus = &eventBus;

            // Instantiate WorldManager
            WorldManager worldManager(&assetManager, &registry, sceneManager.get());

            // 5. Load the world
            WorldFileResult loadRes = worldManager.LoadWorld(tempWorldPath);
            if (!loadRes.Success()) {
                LOG_ERROR("[FormatTest] Test 15 FAILED: LoadWorld failed!");
                return false;
            }

            // Verify both zones are loaded, but state is Inactive
            const auto& zones = worldManager.GetLoadedZones();
            if (zones.size() != 2) {
                LOG_ERROR("[FormatTest] Test 15 FAILED: Expected 2 zones, got %d!", static_cast<int>(zones.size()));
                return false;
            }
            if (zones[0].state != ZoneState::Inactive || zones[1].state != ZoneState::Inactive) {
                LOG_ERROR("[FormatTest] Test 15 FAILED: Zones should start as Inactive!");
                return false;
            }

            // Find the zone objects and check their simulating flags in ECS
            uint32_t entA = INVALID_ENTITY;
            uint32_t entB = INVALID_ENTITY;
            for (uint32_t ent : coordinator.GetActiveEntities()) {
                if (coordinator.IsEntityAlive(ent)) {
                    auto sig = coordinator.GetSignature(ent);
                    if (sig.test(coordinator.GetComponentType<NameComponent>())) {
                        const auto& nameComp = coordinator.GetComponent<NameComponent>(ent);
                        if (nameComp.name == "ObjectInZoneA") {
                            entA = ent;
                        }
                        if (nameComp.name == "ObjectInZoneB") {
                            entB = ent;
                        }
                    }
                }
            }

            if (entA == INVALID_ENTITY || entB == INVALID_ENTITY) {
                LOG_ERROR("[FormatTest] Test 15 FAILED: Zone objects not instantiated in active scene!");
                return false;
            }

            auto zecType = coordinator.GetComponentType<eng::runtime::ZoneEntityComponent>();
            if (!coordinator.GetSignature(entA).test(zecType) || !coordinator.GetSignature(entB).test(zecType)) {
                LOG_ERROR("[FormatTest] Test 15 FAILED: ZoneEntityComponent not attached to zone objects!");
                return false;
            }

            if (coordinator.GetComponent<eng::runtime::ZoneEntityComponent>(entA).simulating ||
                coordinator.GetComponent<eng::runtime::ZoneEntityComponent>(entB).simulating) {
                LOG_ERROR("[FormatTest] Test 15 FAILED: Zone objects simulating state should initially be false!");
                return false;
            }

            // 6. Run update - player is at [0, 0, 0] (inside Zone A)
            worldManager.Update(context, 0.1f);
            eventBus.FlushEvents();

            // Zone A should now be active, Zone B inactive
            if (worldManager.GetActiveZoneUUIDHigh() != zoneA.zoneUUIDHigh) {
                LOG_ERROR("[FormatTest] Test 15 FAILED: Zone A should be active!");
                return false;
            }
            if (coordinator.GetComponent<eng::runtime::ZoneEntityComponent>(entA).simulating == false) {
                LOG_ERROR("[FormatTest] Test 15 FAILED: Zone A objects should be simulating!");
                return false;
            }
            if (coordinator.GetComponent<eng::runtime::ZoneEntityComponent>(entB).simulating == true) {
                LOG_ERROR("[FormatTest] Test 15 FAILED: Zone B objects should NOT be simulating!");
                return false;
            }

            // Check event bus - should have received ZoneEnter ZoneA event
            if (receivedEvents.size() != 1 || receivedEvents[0].Type != eng::runtime::GameplayEventType::ZoneEnter ||
                receivedEvents[0].ZoneUUIDHigh != zoneA.zoneUUIDHigh) {
                LOG_ERROR("[FormatTest] Test 15 FAILED: Expected ZoneEnter event for Zone A!");
                return false;
            }
            receivedEvents.clear();

            // 7. Move player to [20, 0, 0] (inside Zone B)
            coordinator.GetComponent<TransformComponent>(player).position = {20.0f, 0.0f, 0.0f};
            worldManager.Update(context, 0.1f);
            eventBus.FlushEvents();

            // Zone B should now be active, Zone A inactive
            if (worldManager.GetActiveZoneUUIDHigh() != zoneB.zoneUUIDHigh) {
                LOG_ERROR("[FormatTest] Test 15 FAILED: Zone B should be active!");
                return false;
            }
            if (coordinator.GetComponent<eng::runtime::ZoneEntityComponent>(entA).simulating == true) {
                LOG_ERROR("[FormatTest] Test 15 FAILED: Zone A objects should NOT be simulating!");
                return false;
            }
            if (coordinator.GetComponent<eng::runtime::ZoneEntityComponent>(entB).simulating == false) {
                LOG_ERROR("[FormatTest] Test 15 FAILED: Zone B objects should be simulating!");
                return false;
            }

            // Check event bus - should have received ZoneExit ZoneA and ZoneEnter ZoneB events
            if (receivedEvents.size() != 2 ||
                receivedEvents[0].Type != eng::runtime::GameplayEventType::ZoneExit || receivedEvents[0].ZoneUUIDHigh != zoneA.zoneUUIDHigh ||
                receivedEvents[1].Type != eng::runtime::GameplayEventType::ZoneEnter || receivedEvents[1].ZoneUUIDHigh != zoneB.zoneUUIDHigh) {
                LOG_ERROR("[FormatTest] Test 15 FAILED: Expected ZoneExit for A and ZoneEnter for B!");
                return false;
            }
            receivedEvents.clear();

            // 8. Repeated zone transitions - move player back to Zone A
            coordinator.GetComponent<TransformComponent>(player).position = {0.0f, 0.0f, 0.0f};
            worldManager.Update(context, 0.1f);
            eventBus.FlushEvents();

            if (worldManager.GetActiveZoneUUIDHigh() != zoneA.zoneUUIDHigh) {
                LOG_ERROR("[FormatTest] Test 15 FAILED: Zone A should be active again!");
                return false;
            }

            // Verify player state is preserved
            const TransformComponent& playerTrans = coordinator.GetComponent<TransformComponent>(player);
            if (playerTrans.position.x != 0.0f) {
                LOG_ERROR("[FormatTest] Test 15 FAILED: Player position corrupted during transition!");
                return false;
            }

            // 9. Unload the world - verify zone objects are destroyed and active scene is cleaned up
            worldManager.UnloadWorld();

            bool entAFound = false;
            bool entBFound = false;
            for (uint32_t ent : coordinator.GetActiveEntities()) {
                if (coordinator.IsEntityAlive(ent)) {
                    if (ent == entA) entAFound = true;
                    if (ent == entB) entBFound = true;
                }
            }
            if (entAFound || entBFound) {
                LOG_ERROR("[FormatTest] Test 15 FAILED: Zone entities not destroyed after UnloadWorld!");
                return false;
            }

            // Clean up files
            std::filesystem::remove(tempSceneAPath);
            std::filesystem::remove(tempSceneBPath);
            std::filesystem::remove(tempZoneAPath);
            std::filesystem::remove(tempZoneBPath);
            std::filesystem::remove(tempWorldPath);

            LOG_INFO("[FormatTest] Test 15 Passed: Zone Activation and Transition verified successfully.");
        }

        LOG_INFO("================================================================================");
        LOG_INFO("                   ALL RUNTIME FORMAT TESTS PASSED SUCCESSFULLY                 ");
        LOG_INFO("================================================================================");
        return true;
    }

} // namespace eng::runtime
