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
#include "Core/Logger.h"
#include "Scene/Scene.h"
#include "Scene/SceneObject.h"
#include "Scene/SceneSerializer.h"
#include "Scene/SceneLoader.h"
#include "Scene/SceneManager.h"
#include "ECS/Coordinator.h"
#include "ECS/ECSComponents.h"
#include "ECS/PhysicsSystem.h"
#include "ECS/PlayerSystem.h"
#include "Physics/Public/PhysicsValidation.h"
#include "Physics/Public/PhysicsWorld.h"
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

        LOG_INFO("================================================================================");
        LOG_INFO("                   ALL RUNTIME FORMAT TESTS PASSED SUCCESSFULLY                 ");
        LOG_INFO("================================================================================");
        return true;
    }

} // namespace eng::runtime
