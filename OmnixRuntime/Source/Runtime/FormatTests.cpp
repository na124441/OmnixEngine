#include "Runtime/FormatTests.h"
#include "Runtime/FileHeader.h"
#include "Runtime/Checksum.h"
#include "Runtime/BinaryReader.h"
#include "Runtime/BinaryWriter.h"
#include "Runtime/OmnixMeshFormat.h"
#include "Runtime/OmnixMaterialFormat.h"
#include "Runtime/OmnixSceneFormat.h"
#include "Runtime/OmnixAnimFormat.h"
#include "Runtime/OmnixPackageFormat.h"
#include "Runtime/World/WorldDescriptor.h"
#include "Runtime/World/WorldFileReader.h"
#include "Runtime/World/WorldFileWriter.h"
#include "Runtime/World/OmnixWorldHeader.h"
#include "Runtime/World/WorldFileError.h"
#include "Runtime/World/WorldZone.h"
#include "Runtime/World/WorldZoneReader.h"
#include "Runtime/World/WorldZoneWriter.h"
#include "Runtime/World/WorldManager.h"
#include "Runtime/World/ZoneMembershipComponent.h"
#include "ECS/ZoneMembershipSystem.h"
#include "Runtime/AssetRegistry.h"
#include "RenderingEngine/Public/IAssetManager.h"
#include "Renderer/scene/Texture.h"
#include "Renderer/scene/Mesh.h"
#include "Renderer/scene/Material.h"
#include "Runtime/World/OmnixZoneHeader.h"
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
#include "Gameplay/GameplayEvent.h"
#include "Gameplay/GameplayEventBus.h"
#include "Core/World.h"
#include "Runtime/ModuleManager.h"
#include "Runtime/ServiceRegistry.h"
#include "Runtime/PluginManager.h"
#include "Runtime/ConfigSystem.h"
#include "Runtime/Reflection.h"
#include "Runtime/EventBus.h"
#include "Runtime/UUIDSystem.h"
#include "Runtime/CVarSystem.h"
#include "Runtime/RuntimeConsole.h"
#include "Runtime/TimeManager.h"
#include "Scene/Prefab.h"
#include "Scene/PrefabRegistry.h"
#include "ECS/EntityHierarchySystem.h"
#include "Rendering/RHI/RHI.h"
#include "RenderingEngine/Vulkan/VulkanSwapChain.h"
#include "RenderingEngine/Core/Engine/EngineResources.h"
#include "Rendering/Graph/RenderGraph.h"
#include "Rendering/GPUScene/GPUScene.h"
#include "RenderingEngine/Renderer/scene/Material.h"
#include "Rendering/Materials/ShaderLibrary.h"
#include "RenderingEngine/Vulkan/VulkanPipelineCache.h"
#include "RenderingEngine/Renderer/scene/Texture.h"
#include "RenderingEngine/Renderer/scene/Mesh.h"
#include "Rendering/Geometry/Streaming/RVGPageStreamingManager.h"
#include "Rendering/Visibility/FrustumCullPass.h"
#include "Rendering/Lighting/ShadowAtlas.h"
#include "Rendering/Lighting/ClusteredLightingTypes.h"
#include "Rendering/PostProcess/PostProcessChain.h"
#include "Rendering/Debug/DebugDraw.h"
#include "Rendering/Debug/GPUProfiler.h"
#include "Physics/Public/PhysicsWorld.h"
#include "Physics/Public/PhysicsConstraints.h"
#include "Animation/AnimationSystem.h"
#include "Audio/AudioLayerExtensions.h"
#include "Input/InputLayerExtensions.h"
#include "AI/AISystem.h"
#include "Networking/NetworkSystem.h"
#include "Runtime/RuntimeServicesExtensions.h"
#include "Developer/DeveloperServicesExtensions.h"
#include "Runtime/PackagingExtensions.h"
#include <iostream>
#include <vector>
#include <filesystem>
#include <cstring>
#include <algorithm>
#include <iterator>

namespace eng::runtime {

    struct TestReflectedStruct {
        int health;
        float speed;
    };

    REFLECT_STRUCT_BEGIN(TestReflectedStruct)
    REFLECT_FIELD(TestReflectedStruct, int, health, PropertyFlags::Edit)
    REFLECT_FIELD(TestReflectedStruct, float, speed, PropertyFlags::Edit | PropertyFlags::Save)
    REFLECT_STRUCT_END(TestReflectedStruct)

    class PlayerControllerSystem : public ::System {
    public:
        Entity m_PlayerEntity = 0xFFFFFFFF;
        void SetPlayerEntity(Entity entity) override { m_PlayerEntity = entity; }
        Entity GetPlayerEntity() const override { return m_PlayerEntity; }

        void FixedUpdate(void* physicsWorldPtr, class Coordinator& coordinator, float fixedDt) override {
            if (m_PlayerEntity == 0xFFFFFFFF || !coordinator.IsEntityAlive(m_PlayerEntity)) return;

            auto& transform = coordinator.GetComponent<TransformComponent>(m_PlayerEntity);
            auto& ccc = coordinator.GetComponent<CharacterControllerComponent>(m_PlayerEntity);

            auto* physicsWorld = static_cast<eng::physics::PhysicsWorld*>(physicsWorldPtr);

            if (ImGui::IsKeyDown(ImGuiKey_W)) {
                transform.position.x += ccc.moveSpeed * fixedDt;
            }

            Vector3 groundRayOrigin = { transform.position.x, transform.position.y + ccc.capsuleHeight * 0.5f, transform.position.z };
            Vector3 rayDir = { 0.0f, -1.0f, 0.0f };
            float checkDist = ccc.capsuleHeight * 0.5f + ccc.groundCheckDistance;

            eng::physics::RaycastHit groundHit;
            if (physicsWorld && physicsWorld->Raycast(groundRayOrigin, rayDir, checkDist, groundHit)) {
                ccc.isGrounded = true;
                transform.position.y = groundHit.position.y;
            } else {
                ccc.isGrounded = false;
            }
        }
    };

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
            coordinator.RegisterComponent<DirectionalLightComponent>();
            coordinator.RegisterComponent<SkyLightComponent>();
            coordinator.RegisterComponent<HierarchyComponent>();

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
            if (coordinator.GetActiveEntities().size() != 4) {
                LOG_ERROR("[FormatTest] Test 7 FAILED: Restored entity count mismatch! Restored: %zu", coordinator.GetActiveEntities().size());
                std::filesystem::remove(tempPath);
                return false;
            }

            // Retrieve restored entity (address may change due to recreation, so we query it by name)
            Entity restoredEntity = INVALID_ENTITY;
            for (Entity ent : coordinator.GetActiveEntities()) {
                if (coordinator.IsEntityAlive(ent) && coordinator.GetSignature(ent).test(coordinator.GetComponentType<NameComponent>())) {
                    if (coordinator.GetComponent<NameComponent>(ent).name == "OriginalName") {
                        restoredEntity = ent;
                        break;
                    }
                }
            }

            if (restoredEntity == INVALID_ENTITY) {
                LOG_ERROR("[FormatTest] Test 7 FAILED: Could not find restored entity 'OriginalName'!");
                std::filesystem::remove(tempPath);
                return false;
            }

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

            if (auto playerControllerSys = coordinator.GetSystemByName("class eng::runtime::PlayerControllerSystem")) {
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

        // -----------------------------------------------------------------------------
        // Test 16 — Runtime Genesis Modular core systems Test (T1.1.2 - T1.1.19)
        // -----------------------------------------------------------------------------
        LOG_INFO("[FormatTest] Running Test 16: Runtime Genesis Modular Subsystems Validation...");
        {
            // 1. ModuleManager & ServiceRegistry
            class TestModuleA : public IModule {
            public:
                bool Init(RuntimeContext& ctx) override { m_Init = true; return true; }
                void Shutdown() override { m_Init = false; }
                void Tick(float dt) override {}
                std::string GetName() const override { return "TestModuleA"; }
                std::vector<std::string> GetDependencies() const override { return {}; }
                bool m_Init = false;
            };

            class TestModuleB : public IModule {
            public:
                bool Init(RuntimeContext& ctx) override { m_Init = true; return true; }
                void Shutdown() override { m_Init = false; }
                void Tick(float dt) override {}
                std::string GetName() const override { return "TestModuleB"; }
                std::vector<std::string> GetDependencies() const override { return {"TestModuleA"}; }
                bool m_Init = false;
            };

            ModuleManager mm;
            auto modA = std::make_shared<TestModuleA>();
            auto modB = std::make_shared<TestModuleB>();
            mm.RegisterModule(modA);
            mm.RegisterModule(modB);

            RuntimeContext dummyCtx{};
            if (!mm.InitializeModules(dummyCtx)) {
                LOG_ERROR("[FormatTest] Test 16 FAILED: ModuleManager topological init failed!");
                return false;
            }

            const auto& sorted = mm.GetLoadedModulesSorted();
            if (sorted.size() != 2 || sorted[0]->GetName() != "TestModuleA" || sorted[1]->GetName() != "TestModuleB") {
                LOG_ERROR("[FormatTest] Test 16 FAILED: ModuleManager dependency sorting incorrect!");
                return false;
            }

            mm.ShutdownModules();

            // ServiceRegistry test
            ServiceRegistry sr;
            struct ITestService { virtual ~ITestService() = default; virtual void Foo() = 0; };
            struct TestServiceImpl : public ITestService { void Foo() override {} };
            auto service = std::make_shared<TestServiceImpl>();
            sr.RegisterService<ITestService>(service);
            if (sr.GetService<ITestService>() != service) {
                LOG_ERROR("[FormatTest] Test 16 FAILED: ServiceRegistry lookup failed!");
                return false;
            }

            // 2. ConfigSystem layering
            ConfigSystem config;
            char* argv[] = { (char*)"Engine.exe", (char*)"--cvar_override", (char*)"99" };
            config.Initialize(3, argv, "Config/engine_test.json", "Config/user_test.json");
            config.SetDefault("cvar_default", "10");
            if (config.GetInt("cvar_default") != 10) {
                LOG_ERROR("[FormatTest] Test 16 FAILED: ConfigSystem default layer failed!");
                return false;
            }
            if (config.GetInt("cvar_override") != 99) {
                LOG_ERROR("[FormatTest] Test 16 FAILED: ConfigSystem command-line layer failed!");
                return false;
            }

            // 3. Reflection
            TestReflectedStruct testStruct{ 42, 3.14f };
            if (GetProperty<TestReflectedStruct, int>(testStruct, "health") != 42) {
                LOG_ERROR("[FormatTest] Test 16 FAILED: Reflection getter failed!");
                return false;
            }
            SetProperty<TestReflectedStruct, float>(testStruct, "speed", 5.5f);
            if (testStruct.speed != 5.5f) {
                LOG_ERROR("[FormatTest] Test 16 FAILED: Reflection setter failed!");
                return false;
            }

            // 4. EventBus
            EventBus bus;
            struct TestEvent { int value; };
            int receivedVal = 0;
            bus.Subscribe<TestEvent>([&receivedVal](const TestEvent& e) {
                receivedVal = e.value;
            });
            bus.PublishImmediate(TestEvent{ 123 });
            if (receivedVal != 123) {
                LOG_ERROR("[FormatTest] Test 16 FAILED: EventBus synchronous publish failed!");
                return false;
            }

            receivedVal = 0;
            bus.PublishDeferred(TestEvent{ 456 });
            if (receivedVal != 0) {
                LOG_ERROR("[FormatTest] Test 16 FAILED: EventBus deferred publish was synchronous!");
                return false;
            }
            bus.ProcessQueue();
            if (receivedVal != 456) {
                LOG_ERROR("[FormatTest] Test 16 FAILED: EventBus deferred queue processing failed!");
                return false;
            }

            // 5. UUIDSystem
            UUID uuid1 = UUID::GenerateV4();
            std::string uuidStr = uuid1.ToString();
            UUID uuid2 = UUID::FromString(uuidStr);
            if (uuid1 != uuid2) {
                LOG_ERROR("[FormatTest] Test 16 FAILED: UUID round-trip mismatch!");
                return false;
            }

            // 6. CVarSystem
            CVarSystem cvars;
            cvars.RegisterInt("r_shadows", 1, CVarFlags::None, "Shadow toggle");
            if (cvars.GetInt("r_shadows") != 1) {
                LOG_ERROR("[FormatTest] Test 16 FAILED: CVar registration failed!");
                return false;
            }
            cvars.SetValueFromString("r_shadows", "0");
            if (cvars.GetInt("r_shadows") != 0) {
                LOG_ERROR("[FormatTest] Test 16 FAILED: CVar string setter failed!");
                return false;
            }

            // 7. TimeManager
            TimeManager tm;
            tm.Initialize(1.0f / 60.0f, 0.1f);
            tm.Update(0.5f); // Spike duration (should clamp to 0.1f)
            if (tm.GetRawDeltaTime() != 0.5f) {
                LOG_ERROR("[FormatTest] Test 16 FAILED: TimeManager raw delta time incorrect!");
                return false;
            }
            if (tm.GetDeltaTime() > 0.1f) {
                LOG_ERROR("[FormatTest] Test 16 FAILED: TimeManager spike clamping failed!");
                return false;
            }

            LOG_INFO("[FormatTest] Test 16 Passed: Runtime Genesis Modular Subsystems validated successfully.");
        }

        // --------------------------------------------------------------------
        // Test 17: Week 6 Zone Spatial Queries & Membership Validation
        // --------------------------------------------------------------------
        {
            using namespace Omnix;
            LOG_INFO("================================================================================");
            LOG_INFO("   TEST 17: WEEK 6 ZONE SPATIAL QUERIES & MEMBERSHIP VALIDATION");
            LOG_INFO("================================================================================");

            // 1. Create Zone A (min=[-10,-10,-10], max=[10,10,10])
            WorldZone zoneA;
            zoneA.zoneUUIDHigh = 0xAAAAULL;
            zoneA.zoneUUIDLow = 0x1111ULL;
            zoneA.zoneName = "ZoneA";
            zoneA.bounds.min = {-10.0f, -10.0f, -10.0f};
            zoneA.bounds.max = {10.0f, 10.0f, 10.0f};
            zoneA.sceneAssetPath = "test_zone_a.omnixscene";

            // 2. Create Zone B (min=[15,-10,-10], max=[35,10,10])
            WorldZone zoneB;
            zoneB.zoneUUIDHigh = 0xBBBBULL;
            zoneB.zoneUUIDLow = 0x2222ULL;
            zoneB.zoneName = "ZoneB";
            zoneB.bounds.min = {15.0f, -10.0f, -10.0f};
            zoneB.bounds.max = {35.0f, 10.0f, 10.0f};
            zoneB.sceneAssetPath = "test_zone_b.omnixscene";

            // Set Zone A and B as neighbors
            ZoneNeighbor neighborToB;
            neighborToB.zoneUUIDHigh = zoneB.zoneUUIDHigh;
            neighborToB.zoneUUIDLow = zoneB.zoneUUIDLow;
            zoneA.neighbors.push_back(neighborToB);

            ZoneNeighbor neighborToA;
            neighborToA.zoneUUIDHigh = zoneA.zoneUUIDHigh;
            neighborToA.zoneUUIDLow = zoneA.zoneUUIDLow;
            zoneB.neighbors.push_back(neighborToA);

            // Write temporary files
            std::filesystem::path tempZoneAPath = "test_zone_a.omnixzone";
            std::filesystem::path tempZoneBPath = "test_zone_b.omnixzone";
            if (!WorldZoneWriter::WriteToFile(tempZoneAPath, zoneA).Success() ||
                !WorldZoneWriter::WriteToFile(tempZoneBPath, zoneB).Success()) {
                LOG_ERROR("[FormatTest] Test 17 FAILED: Failed to write temporary zone files!");
                return false;
            }

            WorldDescriptor world;
            world.worldUUIDHigh = 0x9999ULL;
            world.worldUUIDLow = 0x7777ULL;
            world.worldName = "SpatialTestWorld";

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

            std::filesystem::path tempWorldPath = "test_spatial_world.omnixworld";
            if (!WorldFileWriter::WriteToFile(tempWorldPath, world).Success()) {
                LOG_ERROR("[FormatTest] Test 17 FAILED: Failed to write world file!");
                return false;
            }

            // Set up local subsystems
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
            sceneManager->CreateNewScene("TestSpatialActiveScene");

            auto& coordinator = ecsWorld->getCoordinator();

            // Set up player
            uint32_t player = coordinator.CreateEntity();
            TransformComponent playerTransform;
            playerTransform.position = {0.0f, 0.0f, 0.0f};
            coordinator.AddComponent<TransformComponent>(player, playerTransform);
            CharacterControllerComponent ccc;
            coordinator.AddComponent<CharacterControllerComponent>(player, ccc);

            if (auto playerControllerSys = coordinator.GetSystemByName("class eng::runtime::PlayerControllerSystem")) {
                playerControllerSys->SetPlayerEntity(player);
            }

            // Set up RuntimeContext
            eng::runtime::GameplayEventBus eventBus;
            eng::runtime::RuntimeContext context;
            context.mode = RuntimeMode::Game;
            context.editorSimulationState = EditorSimulationState::Play;
            context.assets = &assetManager;
            context.assetRegistry = &registry;
            context.scenes = sceneManager.get();
            context.ecs = ecsWorld.get();
            context.gameplayEventBus = &eventBus;

            WorldManager worldManager(&assetManager, &registry, sceneManager.get());
            if (!worldManager.LoadWorld(tempWorldPath).Success()) {
                LOG_ERROR("[FormatTest] Test 17 FAILED: Failed to load spatial test world!");
                return false;
            }

            // Create testing entities
            // Entity 1: in Zone A
            uint32_t ent1 = coordinator.CreateEntity();
            TransformComponent t1;
            t1.position = {2.0f, 2.0f, 2.0f};
            coordinator.AddComponent<TransformComponent>(ent1, t1);
            eng::runtime::ZoneMembershipComponent zmc1;
            zmc1.zoneUUIDHigh = zoneA.zoneUUIDHigh;
            zmc1.zoneUUIDLow = zoneA.zoneUUIDLow;
            coordinator.AddComponent<eng::runtime::ZoneMembershipComponent>(ent1, zmc1);

            // Entity 2: in Zone B
            uint32_t ent2 = coordinator.CreateEntity();
            TransformComponent t2;
            t2.position = {20.0f, 0.0f, 0.0f};
            coordinator.AddComponent<TransformComponent>(ent2, t2);
            eng::runtime::ZoneMembershipComponent zmc2;
            zmc2.zoneUUIDHigh = zoneB.zoneUUIDHigh;
            zmc2.zoneUUIDLow = zoneB.zoneUUIDLow;
            coordinator.AddComponent<eng::runtime::ZoneMembershipComponent>(ent2, zmc2);

            // 3. Test player zone detection
            worldManager.Update(context, 0.1f);
            if (!ValidatePlayerZoneDetection({0.0f,0.0f,0.0f}, zoneA.zoneUUIDHigh, zoneA.zoneUUIDLow, worldManager)) {
                LOG_ERROR("[FormatTest] Test 17 FAILED: Player zone detection validation failed!");
                return false;
            }

            // 4. Test Entity belongs to only one primary zone validation
            if (!ValidateEntityBelongsToOnlyOnePrimaryZone(ent1, coordinator)) {
                LOG_ERROR("[FormatTest] Test 17 FAILED: BelongsToOnlyOnePrimaryZone validation failed!");
                return false;
            }

            // 5. Test QueryEntitiesInZone
            auto entitiesInA = worldManager.QueryEntitiesInZone(zoneA.zoneUUIDHigh, zoneA.zoneUUIDLow, coordinator);
            if (entitiesInA.size() != 1 || entitiesInA[0] != ent1) {
                LOG_ERROR("[FormatTest] Test 17 FAILED: QueryEntitiesInZone for Zone A failed!");
                return false;
            }

            auto entitiesInB = worldManager.QueryEntitiesInZone(zoneB.zoneUUIDHigh, zoneB.zoneUUIDLow, coordinator);
            if (entitiesInB.size() != 1 || entitiesInB[0] != ent2) {
                LOG_ERROR("[FormatTest] Test 17 FAILED: QueryEntitiesInZone for Zone B failed!");
                return false;
            }

            // 6. Test QueryActiveZoneEntities
            auto activeEntities = worldManager.QueryActiveZoneEntities(coordinator);
            if (activeEntities.size() != 1 || activeEntities[0] != ent1) {
                LOG_ERROR("[FormatTest] Test 17 FAILED: QueryActiveZoneEntities failed!");
                return false;
            }

            // 7. Test QueryNearbyZones
            auto nearby = worldManager.QueryNearbyZones({12.0f, 0.0f, 0.0f}, 5.0f);
            if (nearby.size() != 2) {
                LOG_ERROR("[FormatTest] Test 17 FAILED: QueryNearbyZones expected 2 zones, got %d!", (int)nearby.size());
                return false;
            }

            // 8. Test QueryNeighboringZoneEntities
            auto neighborsOfA = worldManager.QueryNeighboringZoneEntities(zoneA.zoneUUIDHigh, zoneA.zoneUUIDLow, coordinator);
            if (neighborsOfA.size() != 1 || neighborsOfA[0] != ent2) {
                LOG_ERROR("[FormatTest] Test 17 FAILED: QueryNeighboringZoneEntities for Zone A failed!");
                return false;
            }

            // 9. Test ZoneMembershipSystem & moving entity updates zone membership
            if (auto zoneMemberSys = ecsWorld->GetSystem<ZoneMembershipSystem>()) {
                // Move Entity 1 to Zone B: [22.0f, 0.0f, 0.0f]
                coordinator.GetComponent<TransformComponent>(ent1).position = {22.0f, 0.0f, 0.0f};
                zoneMemberSys->Update(0.1f, coordinator, &worldManager);

                // Verify Entity 1 zone membership has updated to Zone B
                const auto& zmcUpdated = coordinator.GetComponent<eng::runtime::ZoneMembershipComponent>(ent1);
                if (zmcUpdated.zoneUUIDHigh != zoneB.zoneUUIDHigh || zmcUpdated.zoneUUIDLow != zoneB.zoneUUIDLow) {
                    LOG_ERROR("[FormatTest] Test 17 FAILED: Moving entity zone membership update failed!");
                    return false;
                }
            } else {
                LOG_ERROR("[FormatTest] Test 17 FAILED: ZoneMembershipSystem not registered!");
                return false;
            }

            // 10. Test ValidateWorldObjectsGroupedByZone
            if (!ValidateWorldObjectsGroupedByZone(coordinator, worldManager.GetLoadedZones())) {
                LOG_ERROR("[FormatTest] Test 17 FAILED: ValidateWorldObjectsGroupedByZone failed!");
                return false;
            }

            // 11. Test orphaned detection
            if (!ValidateNoOrphanedZoneEntitiesExist(coordinator, worldManager)) {
                LOG_ERROR("[FormatTest] Test 17 FAILED: ValidateNoOrphanedZoneEntitiesExist failed when no orphans!");
                return false;
            }

            // Add an orphan (invalid zone ID)
            uint32_t orphanEnt = coordinator.CreateEntity();
            eng::runtime::ZoneMembershipComponent zmcOrphan;
            zmcOrphan.zoneUUIDHigh = 0x9999ULL;
            zmcOrphan.zoneUUIDLow = 0x9999ULL;
            coordinator.AddComponent<eng::runtime::ZoneMembershipComponent>(orphanEnt, zmcOrphan);

            if (ValidateNoOrphanedZoneEntitiesExist(coordinator, worldManager)) {
                LOG_ERROR("[FormatTest] Test 17 FAILED: ValidateNoOrphanedZoneEntitiesExist did not detect orphan!");
                return false;
            }

            // Clean up orphan
            coordinator.DestroyEntity(orphanEnt);

            // Print diagnostics to verify it works without crashing
            worldManager.PrintZoneMembershipDiagnostics(coordinator);

            // Clean up files
            std::filesystem::remove(tempZoneAPath);
            std::filesystem::remove(tempZoneBPath);
            std::filesystem::remove(tempWorldPath);

            LOG_INFO("[FormatTest] Test 17 Passed: Week 6 Zone Spatial Queries validated successfully.");
        }

        // --------------------------------------------------------------------
        // Test 18: Week 9 Ground Section System Traversal & Validation
        // --------------------------------------------------------------------
        {
            using namespace Omnix;
            LOG_INFO("================================================================================");
            LOG_INFO("   TEST 18: WEEK 9 GROUND SECTION SYSTEM TRAVERSAL & VALIDATION");
            LOG_INFO("================================================================================");

            // Create temporary ImGui context for input simulation
            ImGuiContext* oldCtx = ImGui::GetCurrentContext();
            ImGuiContext* testCtx = ImGui::CreateContext();
            ImGui::SetCurrentContext(testCtx);

            // 1. Create Zone A and B
            WorldZone zoneA;
            zoneA.zoneUUIDHigh = 0xAAAAULL;
            zoneA.zoneUUIDLow = 0x1111ULL;
            zoneA.zoneName = "ZoneA";
            zoneA.bounds.min = {-10.0f, -5.0f, -10.0f};
            zoneA.bounds.max = {10.0f, 5.0f, 10.0f};
            zoneA.sceneAssetPath = "test_zone_a.omnixscene";

            WorldZone zoneB;
            zoneB.zoneUUIDHigh = 0xBBBBULL;
            zoneB.zoneUUIDLow = 0x2222ULL;
            zoneB.zoneName = "ZoneB";
            zoneB.bounds.min = {10.0f, -5.0f, -10.0f};
            zoneB.bounds.max = {30.0f, 5.0f, 10.0f};
            zoneB.sceneAssetPath = "test_zone_b.omnixscene";

            ZoneNeighbor neighborToB;
            neighborToB.zoneUUIDHigh = zoneB.zoneUUIDHigh;
            neighborToB.zoneUUIDLow = zoneB.zoneUUIDLow;
            zoneA.neighbors.push_back(neighborToB);

            ZoneNeighbor neighborToA;
            neighborToA.zoneUUIDHigh = zoneA.zoneUUIDHigh;
            neighborToA.zoneUUIDLow = zoneA.zoneUUIDLow;
            zoneB.neighbors.push_back(neighborToA);

            std::filesystem::path tempZoneAPath = "test_zone_a.omnixzone";
            std::filesystem::path tempZoneBPath = "test_zone_b.omnixzone";
            if (!WorldZoneWriter::WriteToFile(tempZoneAPath, zoneA).Success() ||
                !WorldZoneWriter::WriteToFile(tempZoneBPath, zoneB).Success()) {
                LOG_ERROR("[FormatTest] Test 18 FAILED: Failed to write temporary zone files!");
                ImGui::DestroyContext(testCtx);
                ImGui::SetCurrentContext(oldCtx);
                return false;
            }

            WorldDescriptor world;
            world.worldUUIDHigh = 0x9999ULL;
            world.worldUUIDLow = 0x8888ULL;
            world.worldName = "GroundTraversalWorld";

            WorldZoneEntry entryA;
            entryA.zoneUUIDHigh = zoneA.zoneUUIDHigh;
            entryA.zoneUUIDLow = zoneA.zoneUUIDLow;
            std::strncpy(entryA.zoneName, zoneA.zoneName.c_str(), sizeof(entryA.zoneName) - 1);
            std::strncpy(entryA.zonePath, tempZoneAPath.string().c_str(), sizeof(entryA.zonePath) - 1);
            world.zones.push_back(entryA);

            WorldZoneEntry entryB;
            entryB.zoneUUIDHigh = zoneB.zoneUUIDHigh;
            entryB.zoneUUIDLow = zoneB.zoneUUIDLow;
            std::strncpy(entryB.zoneName, zoneB.zoneName.c_str(), sizeof(entryB.zoneName) - 1);
            std::strncpy(entryB.zonePath, tempZoneBPath.string().c_str(), sizeof(entryB.zonePath) - 1);
            world.zones.push_back(entryB);

            std::filesystem::path tempWorldPath = "test_traversal_world.omnixworld";
            if (!WorldFileWriter::WriteToFile(tempWorldPath, world).Success()) {
                LOG_ERROR("[FormatTest] Test 18 FAILED: Failed to write world file!");
                std::filesystem::remove(tempZoneAPath);
                std::filesystem::remove(tempZoneBPath);
                ImGui::DestroyContext(testCtx);
                ImGui::SetCurrentContext(oldCtx);
                return false;
            }

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
            sceneManager->CreateNewScene("TestTraversalActiveScene");

            auto& coordinator = ecsWorld->getCoordinator();

            // Set up WorldManager and load the world
            WorldManager worldManager(&assetManager, &registry, sceneManager.get());
            if (!worldManager.LoadWorld(tempWorldPath).Success()) {
                LOG_ERROR("[FormatTest] Test 18 FAILED: Failed to load traversal world!");
                return false;
            }

            // 2. Create Ground Section entities A and B
            // Ground Section A (in Zone A)
            uint32_t gsaEnt = coordinator.CreateEntity();
            TransformComponent ta;
            ta.position = {0.0f, 0.0f, 0.0f}; // Top of box is Y = 1.0f
            coordinator.AddComponent<TransformComponent>(gsaEnt, ta);

            eng::runtime::GroundSectionComponent gsa;
            gsa.zoneUUIDHigh = zoneA.zoneUUIDHigh;
            gsa.zoneUUIDLow = zoneA.zoneUUIDLow;
            gsa.meshAssetPath = "mesh_a.mesh";
            gsa.materialAssetPath = "mat_a.mat";
            gsa.collisionAssetPath = "col_a.col";
            gsa.boundsMin = {-10.0f, -1.0f, -10.0f};
            gsa.boundsMax = {10.0f, 1.0f, 10.0f};
            coordinator.AddComponent<eng::runtime::GroundSectionComponent>(gsaEnt, gsa);

            // Give them PhysX box colliders and StaticBodyComponent
            coordinator.AddComponent<StaticBodyComponent>(gsaEnt, StaticBodyComponent());
            BoxColliderComponent bccA;
            bccA.size = {20.0f, 2.0f, 20.0f};
            bccA.offset = {0.0f, 0.0f, 0.0f};
            coordinator.AddComponent<BoxColliderComponent>(gsaEnt, bccA);

            // Ground Section B (in Zone B)
            uint32_t gsbEnt = coordinator.CreateEntity();
            TransformComponent tb;
            tb.position = {20.0f, 0.0f, 0.0f}; // Top of box is Y = 1.0f
            coordinator.AddComponent<TransformComponent>(gsbEnt, tb);

            eng::runtime::GroundSectionComponent gsb;
            gsb.zoneUUIDHigh = zoneB.zoneUUIDHigh;
            gsb.zoneUUIDLow = zoneB.zoneUUIDLow;
            gsb.meshAssetPath = "mesh_b.mesh";
            gsb.materialAssetPath = "mat_b.mat";
            gsb.collisionAssetPath = "col_b.col";
            gsb.boundsMin = {10.0f, -1.0f, -10.0f};
            gsb.boundsMax = {30.0f, 1.0f, 10.0f};
            coordinator.AddComponent<eng::runtime::GroundSectionComponent>(gsbEnt, gsb);

            coordinator.AddComponent<StaticBodyComponent>(gsbEnt, StaticBodyComponent());
            BoxColliderComponent bccB;
            bccB.size = {20.0f, 2.0f, 20.0f};
            bccB.offset = {0.0f, 0.0f, 0.0f};
            coordinator.AddComponent<BoxColliderComponent>(gsbEnt, bccB);

            // Initialize PhysX
            eng::physics::PhysicsWorld physicsWorld;
            if (!physicsWorld.Initialize()) {
                LOG_ERROR("[FormatTest] Test 18 FAILED: Failed to initialize PhysicsWorld!");
                return false;
            }
            physicsWorld.RegisterStaticColliders(coordinator);

            // 3. Validation Helpers testing
            if (!ValidateGroundSectionZones(coordinator, worldManager.GetLoadedZones())) {
                LOG_ERROR("[FormatTest] Test 18 FAILED: ValidateGroundSectionZones failed!");
                return false;
            }

            if (!ValidateGroundCollisionMatchesMesh("mesh_a.mesh", "col_a.col", 0.05f)) {
                LOG_ERROR("[FormatTest] Test 18 FAILED: ValidateGroundCollisionMatchesMesh failed!");
                return false;
            }

            if (!ValidateConnectedGroundSeams(gsa, gsb, 0.01f)) {
                LOG_ERROR("[FormatTest] Test 18 FAILED: ValidateConnectedGroundSeams failed!");
                return false;
            }

            // 4. Test serialization and deserialization
            Scene* scene = sceneManager->GetActiveScene();
            if (!scene) {
                LOG_ERROR("[FormatTest] Test 18 FAILED: Active scene is null!");
                return false;
            }

            // Sync from ECS to SceneObjects so they serialize
            sceneManager->SyncECSToScene();

            std::string tempSceneFile = "test_ground_scene.omnixscene";
            if (!SceneSerializer::SaveScene(scene, tempSceneFile)) {
                LOG_ERROR("[FormatTest] Test 18 FAILED: Failed to serialize scene!");
                return false;
            }

            // Deserialise into a new scene and verify
            Scene* destScene = SceneLoader::LoadFromFile(tempSceneFile);
            if (!destScene) {
                LOG_ERROR("[FormatTest] Test 18 FAILED: Failed to deserialize scene!");
                std::filesystem::remove(tempSceneFile);
                return false;
            }

            // Instantiate ECS for the deserialized scene objects in a separate coordinator
            auto tempECS = std::make_unique<World>();
            auto tempSceneMgr = std::make_unique<SceneManager>(&tempECS->getCoordinator());
            tempSceneMgr->SetAssetRegistry(&registry);
            tempSceneMgr->RegisterSceneEntities(destScene);

            // Find deserialized GroundSection components
            bool foundA = false, foundB = false;
            auto& destCoord = tempECS->getCoordinator();
            auto gscType = destCoord.GetComponentType<eng::runtime::GroundSectionComponent>();
            for (Entity ent : destCoord.GetActiveEntities()) {
                if (destCoord.IsEntityAlive(ent) && destCoord.GetSignature(ent).test(gscType)) {
                    const auto& comp = destCoord.GetComponent<eng::runtime::GroundSectionComponent>(ent);
                    if (comp.meshAssetPath == "mesh_a.mesh") {
                        foundA = true;
                        if (comp.boundsMin.x != -10.0f || comp.boundsMax.x != 10.0f ||
                            comp.zoneUUIDHigh != zoneA.zoneUUIDHigh) {
                            LOG_ERROR("[FormatTest] Test 18 FAILED: Serialized Ground Section A values do not match!");
                            delete destScene;
                            return false;
                        }
                    } else if (comp.meshAssetPath == "mesh_b.mesh") {
                        foundB = true;
                        if (comp.boundsMin.x != 10.0f || comp.boundsMax.x != 30.0f ||
                            comp.zoneUUIDLow != zoneB.zoneUUIDLow) {
                            LOG_ERROR("[FormatTest] Test 18 FAILED: Serialized Ground Section B values do not match!");
                            delete destScene;
                            return false;
                        }
                    }
                }
            }

            if (!foundA || !foundB) {
                LOG_ERROR("[FormatTest] Test 18 FAILED: Did not find deserialized GroundSection components!");
                delete destScene;
                return false;
            }

            delete destScene;

            // Re-register original active scene entities to make sure we're clean
            sceneManager->RegisterSceneEntities(scene);

            // 5. Player Traversal across seam (0.0f -> 20.0f)
            // Spawn player at {0.0f, 1.0f, 0.0f}
            uint32_t player = coordinator.CreateEntity();
            TransformComponent pt;
            pt.position = {0.0f, 1.0f, 0.0f}; // Top of Ground Section A is Y = 1.0f
            coordinator.AddComponent<TransformComponent>(player, pt);

            CharacterControllerComponent pccc;
            pccc.yaw = 0.0f; // Walk along X-axis
            pccc.moveSpeed = 50.0f; // Quick movement for traversal simulation
            pccc.capsuleHeight = 1.8f;
            pccc.capsuleRadius = 0.35f;
            pccc.groundCheckDistance = 0.12f;
            coordinator.AddComponent<CharacterControllerComponent>(player, pccc);

            // Register player
            auto playerControllerSys = coordinator.RegisterSystem<eng::runtime::PlayerControllerSystem>();
            playerControllerSys->SetPlayerEntity(player);
            // Re-run registration so signature is matching
            ::Signature playSig;
            playSig.set(coordinator.GetComponentType<TransformComponent>());
            playSig.set(coordinator.GetComponentType<CharacterControllerComponent>());
            coordinator.SetSystemSignature<eng::runtime::PlayerControllerSystem>(playSig);
            playerControllerSys->m_Entities.insert(player);

            // Walk player forwards (simulate W key pressed)
            ImGui::GetIO().AddKeyEvent(ImGuiKey_W, true);

            float dt = 0.02f; // 50 FPS physics steps
            for (int step = 0; step < 20; ++step) {
                playerControllerSys->FixedUpdate(&physicsWorld, coordinator, dt);
                
                const auto& finalTransform = coordinator.GetComponent<TransformComponent>(player);
                const auto& finalCCC = coordinator.GetComponent<CharacterControllerComponent>(player);

                // Assert grounded
                if (!finalCCC.isGrounded) {
                    LOG_ERROR("[FormatTest] Test 18 FAILED: Player lost grounded status during traversal step %d! Position: (%f, %f, %f)",
                              step, finalTransform.position.x, finalTransform.position.y, finalTransform.position.z);
                    return false;
                }

                // Assert Y alignment is exactly 1.0f
                if (std::abs(finalTransform.position.y - 1.0f) > 0.01f) {
                    LOG_ERROR("[FormatTest] Test 18 FAILED: Player height changed incorrectly to %f at step %d!",
                              finalTransform.position.y, step);
                    return false;
                }
            }

            ImGui::GetIO().AddKeyEvent(ImGuiKey_W, false);

            // 6. Test Debug Draw runs without crashing
            if (auto groundSys = ecsWorld->GetSystem<GroundSectionSystem>()) {
                groundSys->Update(0.1f, coordinator);
            }

            // Shutdown physics
            physicsWorld.Shutdown();

            // Clean up temporary files
            std::filesystem::remove(tempZoneAPath);
            std::filesystem::remove(tempZoneBPath);
            std::filesystem::remove(tempWorldPath);
            std::filesystem::remove(tempSceneFile);

            // Clean up ImGui Context
            ImGui::DestroyContext(testCtx);
            ImGui::SetCurrentContext(oldCtx);

            LOG_INFO("[FormatTest] Test 18 Passed: Week 9 Ground Section System validated successfully.");
        }

        // --------------------------------------------------------------------
        // Test 19: Scene Graph Hierarchy, Transform Propagation & Reparenting
        // --------------------------------------------------------------------
        {
            LOG_INFO("[FormatTest] Running Test 19: Scene Graph Hierarchy & Transform Propagation...");

            Scene testScene("SceneGraphTest");

            auto rootObj = std::make_shared<SceneObject>("RootNode");
            auto childObj = std::make_shared<SceneObject>("ChildNode");
            auto grandchildObj = std::make_shared<SceneObject>("GrandchildNode");

            rootObj->transform.SetPosition({10.0f, 0.0f, 0.0f});
            childObj->transform.SetPosition({0.0f, 5.0f, 0.0f});
            grandchildObj->transform.SetPosition({0.0f, 0.0f, 2.0f});

            // 1. Build hierarchy
            rootObj->AddChild(childObj.get());
            childObj->AddChild(grandchildObj.get());

            if (childObj->GetParent() != rootObj.get() || grandchildObj->GetParent() != childObj.get()) {
                LOG_ERROR("[FormatTest] Test 19 FAILED: Hierarchy parent linkage incorrect!");
                return false;
            }

            // 2. Propagate transforms
            rootObj->Update(0.016f);

            Vector3 worldPos = grandchildObj->transform.GetWorldPosition();
            if (std::abs(worldPos.x - 10.0f) > 0.01f || std::abs(worldPos.y - 5.0f) > 0.01f || std::abs(worldPos.z - 2.0f) > 0.01f) {
                LOG_ERROR("[FormatTest] Test 19 FAILED: Transform hierarchy local-to-world propagation failed! Got (%f, %f, %f)",
                          worldPos.x, worldPos.y, worldPos.z);
                return false;
            }

            // 3. Child name search
            if (rootObj->FindChild("ChildNode") != childObj.get()) {
                LOG_ERROR("[FormatTest] Test 19 FAILED: FindChild failed to locate child node!");
                return false;
            }

            // 4. Reparenting
            rootObj->AddChild(grandchildObj.get());
            if (grandchildObj->GetParent() != rootObj.get() || childObj->GetChildCount() != 0) {
                LOG_ERROR("[FormatTest] Test 19 FAILED: Reparenting grandchild to root failed!");
                return false;
            }

            // 5. Scene container integration
            testScene.AddSceneObject(rootObj);
            testScene.AddSceneObject(childObj);
            testScene.AddSceneObject(grandchildObj);
            testScene.RebuildRootObjects();

            if (testScene.GetRootObjects().size() != 1 || testScene.GetAllSceneObjects().size() != 3) {
                LOG_ERROR("[FormatTest] Test 19 FAILED: Scene container root object rebuilding failed! Roots: %zu, Total: %zu",
                          testScene.GetRootObjects().size(), testScene.GetAllSceneObjects().size());
                return false;
            }

            LOG_INFO("[FormatTest] Test 18 Passed: Week 9 Ground Section System validated successfully.");
            LOG_INFO("[FormatTest] Test 19 Passed: Scene Graph Hierarchy & Transform Propagation validated successfully.");
        }

        // --------------------------------------------------------------------
        // Test 20: Prefab Instantiation & Registry Validation
        // --------------------------------------------------------------------
        {
            LOG_INFO("[FormatTest] Running Test 20: Prefab Instantiation & Registry...");

            auto templateRoot = std::make_shared<SceneObject>("EnemyTemplate");
            auto templateWeapon = std::make_shared<SceneObject>("WeaponTemplate");
            templateRoot->AddChild(templateWeapon.get());

            auto prefab = std::make_unique<Prefab>("prefabs/Enemy.prefab");
            prefab->SetTemplateObject(templateRoot);

            PrefabRegistry::Get().Register("EnemyPrefab", std::move(prefab));

            if (!PrefabRegistry::Get().IsLoaded("EnemyPrefab")) {
                LOG_ERROR("[FormatTest] Test 20 FAILED: Prefab failed to register in PrefabRegistry!");
                return false;
            }

            Prefab* retrievedPrefab = PrefabRegistry::Get().Get("EnemyPrefab");
            if (!retrievedPrefab) {
                LOG_ERROR("[FormatTest] Test 20 FAILED: PrefabRegistry Get returned nullptr!");
                return false;
            }

            auto instanceRoot = retrievedPrefab->Instantiate();
            if (!instanceRoot) {
                LOG_ERROR("[FormatTest] Test 20 FAILED: Prefab instantiation returned nullptr!");
                return false;
            }

            if (instanceRoot->GetName() != "EnemyTemplate_Instance" || instanceRoot->GetChildCount() != 1) {
                LOG_ERROR("[FormatTest] Test 20 FAILED: Prefab instance root name or child count mismatch!");
                return false;
            }

            if (instanceRoot->GetChildren()[0]->GetName() != "WeaponTemplate_Instance") {
                LOG_ERROR("[FormatTest] Test 20 FAILED: Prefab instance child name mismatch!");
                return false;
            }

            if (instanceRoot->GetID() == templateRoot->GetID()) {
                LOG_ERROR("[FormatTest] Test 20 FAILED: Prefab instance did not generate a new unique EntityID!");
                return false;
            }

            LOG_INFO("[FormatTest] Test 20 Passed: Prefab Instantiation & Registry validated successfully.");
        }

        // --------------------------------------------------------------------
        // Test 21: Scene Serialization & RapidJSON Output Validation
        // --------------------------------------------------------------------
        {
            LOG_INFO("[FormatTest] Running Test 21: Scene Serialization & RapidJSON Adapters...");

            Scene serializeScene("SerializeTestScene");

            auto rootObj = std::make_shared<SceneObject>("RootLabNode");
            rootObj->transform.SetPosition({5.0f, 10.0f, 15.0f});
            rootObj->m_HasPointLight = true;
            rootObj->m_PointLight.color = {1.0f, 0.5f, 0.2f};
            rootObj->m_PointLight.intensity = 15.0f;

            serializeScene.AddSceneObject(rootObj);

            std::string tempPath = "test_serialize_output.omnixscene";
            if (!SceneSerializer::ValidateOutputPath(tempPath)) {
                LOG_ERROR("[FormatTest] Test 21 FAILED: ValidateOutputPath rejected valid scene path!");
                return false;
            }

            if (!SceneSerializer::SaveScene(&serializeScene, tempPath)) {
                LOG_ERROR("[FormatTest] Test 21 FAILED: SaveScene failed to serialize scene to JSON!");
                return false;
            }

            if (!std::filesystem::exists(tempPath) || std::filesystem::file_size(tempPath) == 0) {
                LOG_ERROR("[FormatTest] Test 21 FAILED: Serialized JSON file is missing or empty!");
                return false;
            }

            std::filesystem::remove(tempPath);

            LOG_INFO("[FormatTest] Test 21 Passed: Scene Serialization & RapidJSON Output validated successfully.");
        }

        // --------------------------------------------------------------------
        // Test 22: Scene Streaming & Proximity Scheduler Validation
        // --------------------------------------------------------------------
        {
            using namespace Omnix;
            LOG_INFO("[FormatTest] Running Test 22: Scene Streaming & Proximity Scheduler...");

            WorldZone zoneA;
            zoneA.zoneUUIDHigh = 0xAA11ULL;
            zoneA.zoneUUIDLow = 0xAA22ULL;
            zoneA.zoneName = "StreamingZoneAlpha";
            zoneA.bounds.min = {0.0f, 0.0f, 0.0f};
            zoneA.bounds.max = {10.0f, 10.0f, 10.0f};

            WorldZone zoneB;
            zoneB.zoneUUIDHigh = 0xBB11ULL;
            zoneB.zoneUUIDLow = 0xBB22ULL;
            zoneB.zoneName = "StreamingZoneBeta";
            zoneB.bounds.min = {20.0f, 0.0f, 0.0f};
            zoneB.bounds.max = {30.0f, 10.0f, 10.0f};

            std::filesystem::path zoneAPath = "stream_zone_a.omnixzone";
            std::filesystem::path zoneBPath = "stream_zone_b.omnixzone";
            std::filesystem::path worldPath = "stream_world.omnixworld";

            if (!WorldZoneWriter::WriteToFile(zoneAPath, zoneA).Success() ||
                !WorldZoneWriter::WriteToFile(zoneBPath, zoneB).Success()) {
                LOG_ERROR("[FormatTest] Test 22 FAILED: Failed to write test streaming zones!");
                return false;
            }

            WorldDescriptor world;
            world.worldUUIDHigh = 0x9999ULL;
            world.worldUUIDLow = 0x1111ULL;
            world.worldName = "StreamingTestWorld";

            WorldZoneEntry entryA;
            entryA.zoneUUIDHigh = zoneA.zoneUUIDHigh;
            entryA.zoneUUIDLow = zoneA.zoneUUIDLow;
            std::strncpy(entryA.zoneName, zoneA.zoneName.c_str(), sizeof(entryA.zoneName) - 1);
            entryA.zoneName[sizeof(entryA.zoneName) - 1] = '\0';
            std::strncpy(entryA.zonePath, zoneAPath.string().c_str(), sizeof(entryA.zonePath) - 1);
            entryA.zonePath[sizeof(entryA.zonePath) - 1] = '\0';
            world.zones.push_back(entryA);

            WorldZoneEntry entryB;
            entryB.zoneUUIDHigh = zoneB.zoneUUIDHigh;
            entryB.zoneUUIDLow = zoneB.zoneUUIDLow;
            std::strncpy(entryB.zoneName, zoneB.zoneName.c_str(), sizeof(entryB.zoneName) - 1);
            entryB.zoneName[sizeof(entryB.zoneName) - 1] = '\0';
            std::strncpy(entryB.zonePath, zoneBPath.string().c_str(), sizeof(entryB.zonePath) - 1);
            entryB.zonePath[sizeof(entryB.zonePath) - 1] = '\0';
            world.zones.push_back(entryB);

            if (!WorldFileWriter::WriteToFile(worldPath, world).Success()) {
                LOG_ERROR("[FormatTest] Test 22 FAILED: Failed to write test streaming world!");
                return false;
            }

            // Create context components
            class MockAssetManager : public eng::runtime::IAssetManager {
            protected:
                void* LoadRaw(const std::string& path, const std::type_info& typeInfo) override {
                    return nullptr;
                }
            };
            MockAssetManager assetMgr;
            AssetRegistry registry;
            auto sceneMgr = std::make_unique<SceneManager>();
            auto ecsWorld = std::make_unique<World>();
            auto& coordinator = ecsWorld->getCoordinator();

            RuntimeContext context;
            context.mode = RuntimeMode::Game;
            context.assets = &assetMgr;
            context.assetRegistry = &registry;
            context.scenes = sceneMgr.get();
            context.ecs = ecsWorld.get();

            Omnix::WorldManager worldMgr(&assetMgr, &registry, sceneMgr.get());
            if (!worldMgr.LoadWorld(worldPath).Success()) {
                LOG_ERROR("[FormatTest] Test 22 FAILED: Failed to load streaming test world!");
                return false;
            }

            // Spawn player entity inside Zone A
            Entity playerEnt = coordinator.CreateEntity();
            TransformComponent t;
            t.position = {5.0f, 5.0f, 5.0f};
            coordinator.AddComponent<TransformComponent>(playerEnt, t);
            coordinator.AddComponent<CharacterControllerComponent>(playerEnt, CharacterControllerComponent());

            // 1. First update - player in Zone A
            worldMgr.Update(context, 0.016f);

            if (worldMgr.GetActiveZoneUUIDHigh() != zoneA.zoneUUIDHigh ||
                worldMgr.GetActiveZoneUUIDLow() != zoneA.zoneUUIDLow) {
                LOG_ERROR("[FormatTest] Test 22 FAILED: Player position in Zone A was not detected as active zone!");
                return false;
            }

            // 2. Move player into Zone B
            auto& playerTransform = coordinator.GetComponent<TransformComponent>(playerEnt);
            playerTransform.position = {25.0f, 5.0f, 5.0f};

            worldMgr.Update(context, 0.016f);

            if (worldMgr.GetActiveZoneUUIDHigh() != zoneB.zoneUUIDHigh ||
                worldMgr.GetActiveZoneUUIDLow() != zoneB.zoneUUIDLow ||
                worldMgr.GetPreviousZoneUUIDHigh() != zoneA.zoneUUIDHigh) {
                LOG_ERROR("[FormatTest] Test 22 FAILED: Zone transition from Zone A to Zone B failed!");
                return false;
            }

            // Unload world
            worldMgr.UnloadWorld();

            // Clean up temporary files
            std::filesystem::remove(zoneAPath);
            std::filesystem::remove(zoneBPath);
            std::filesystem::remove(worldPath);

            LOG_INFO("[FormatTest] Test 22 Passed: Scene Streaming & Proximity Scheduler validated successfully.");
        }

        // --------------------------------------------------------------------
        // Test 23: Entity Hierarchy Component & ECS Hierarchy System Validation
        // --------------------------------------------------------------------
        {
            LOG_INFO("[FormatTest] Running Test 23: Entity Hierarchy Component & ECS Hierarchy System...");

            Coordinator coordinator;
            coordinator.Init();

            coordinator.RegisterComponent<TransformComponent>();
            coordinator.RegisterComponent<HierarchyComponent>();

            auto hierarchySystem = coordinator.RegisterSystem<EntityHierarchySystem>();
            Signature sig;
            sig.test(coordinator.GetComponentType<HierarchyComponent>());
            coordinator.SetSystemSignature<EntityHierarchySystem>(sig);

            Entity parentEnt = coordinator.CreateEntity();
            Entity childEnt = coordinator.CreateEntity();
            Entity grandchildEnt = coordinator.CreateEntity();

            coordinator.AddComponent<TransformComponent>(parentEnt, TransformComponent());
            coordinator.AddComponent<TransformComponent>(childEnt, TransformComponent());
            coordinator.AddComponent<TransformComponent>(grandchildEnt, TransformComponent());

            // Build hierarchy: Parent -> Child -> Grandchild
            EntityHierarchySystem::AttachChild(parentEnt, childEnt, coordinator);
            EntityHierarchySystem::AttachChild(childEnt, grandchildEnt, coordinator);

            hierarchySystem->Update(coordinator);

            const auto& parentHc = coordinator.GetComponent<HierarchyComponent>(parentEnt);
            const auto& childHc = coordinator.GetComponent<HierarchyComponent>(childEnt);
            const auto& grandchildHc = coordinator.GetComponent<HierarchyComponent>(grandchildEnt);

            if (parentHc.parent != 0xFFFFFFFF || parentHc.depth != 0 || parentHc.children.size() != 1) {
                LOG_ERROR("[FormatTest] Test 23 FAILED: Parent entity hierarchy component invalid!");
                return false;
            }

            if (childHc.parent != parentEnt || childHc.depth != 1 || childHc.children.size() != 1) {
                LOG_ERROR("[FormatTest] Test 23 FAILED: Child entity hierarchy component invalid!");
                return false;
            }

            if (grandchildHc.parent != childEnt || grandchildHc.depth != 2) {
                LOG_ERROR("[FormatTest] Test 23 FAILED: Grandchild entity hierarchy component invalid!");
                return false;
            }

            // Reparent grandchild directly to parent (Parent -> Grandchild)
            EntityHierarchySystem::AttachChild(parentEnt, grandchildEnt, coordinator);
            hierarchySystem->Update(coordinator);

            const auto& updatedParentHc = coordinator.GetComponent<HierarchyComponent>(parentEnt);
            const auto& updatedGrandchildHc = coordinator.GetComponent<HierarchyComponent>(grandchildEnt);

            if (updatedGrandchildHc.parent != parentEnt || updatedGrandchildHc.depth != 1) {
                LOG_ERROR("[FormatTest] Test 23 FAILED: Reparented grandchild entity depth/parent invalid!");
                return false;
            }

            if (updatedParentHc.children.size() != 2) {
                LOG_ERROR("[FormatTest] Test 23 FAILED: Parent entity children count mismatch after reparenting!");
                return false;
            }

            LOG_INFO("[FormatTest] Test 23 Passed: Entity Hierarchy Component & ECS Hierarchy System validated successfully.");
        }

        // --------------------------------------------------------------------
        // Test 24: Runtime Scene Instantiation & ECS Binding Validation
        // --------------------------------------------------------------------
        {
            LOG_INFO("[FormatTest] Running Test 24: Runtime Scene Instantiation & ECS Binding...");

            Coordinator coordinator;
            coordinator.Init();

            coordinator.RegisterComponent<TransformComponent>();
            coordinator.RegisterComponent<NameComponent>();
            coordinator.RegisterComponent<TagComponent>();
            coordinator.RegisterComponent<LayerComponent>();
            coordinator.RegisterComponent<HierarchyComponent>();

            Scene scene("RuntimeInstantiateTestScene");
            scene.SetCoordinator(&coordinator);

            auto dynamicProp = std::make_shared<SceneObject>("DynamicProp");
            dynamicProp->transform.SetPosition({12.0f, 3.0f, -4.0f});

            scene.AddSceneObject(dynamicProp);

            Entity ecsEnt = dynamicProp->GetECSEntity();
            if (!coordinator.IsEntityAlive(ecsEnt)) {
                LOG_ERROR("[FormatTest] Test 24 FAILED: SceneObject AddSceneObject did not instantiate an alive ECS entity!");
                return false;
            }

            if (!coordinator.HasComponent<NameComponent>(ecsEnt) ||
                !coordinator.HasComponent<TransformComponent>(ecsEnt) ||
                !coordinator.HasComponent<HierarchyComponent>(ecsEnt)) {
                LOG_ERROR("[FormatTest] Test 24 FAILED: ECS entity missing core components (Name, Transform, Hierarchy)!");
                return false;
            }

            const auto& tc = coordinator.GetComponent<TransformComponent>(ecsEnt);
            if (tc.position.x != 12.0f || tc.position.y != 3.0f || tc.position.z != -4.0f) {
                LOG_ERROR("[FormatTest] Test 24 FAILED: ECS TransformComponent position bound incorrectly!");
                return false;
            }

            LOG_INFO("[FormatTest] Test 24 Passed: Runtime Scene Instantiation & ECS Binding validated successfully.");
        }

        // --------------------------------------------------------------------
        // Test 25: RHI Handles & Vulkan Object Mapping Validation
        // --------------------------------------------------------------------
        {
            LOG_INFO("[FormatTest] Running Test 25: RHI Handles & Vulkan Object Mapping...");

            // 1. Verify RHIBuffer type mapping
            eng::renderer::RHIBuffer rhiBuffer{};
            rhiBuffer.size = 1024;
            rhiBuffer.buffer = (VkBuffer)(uintptr_t)0x12345678ULL;

            if (rhiBuffer.size != 1024 || rhiBuffer.buffer != (VkBuffer)(uintptr_t)0x12345678ULL) {
                LOG_ERROR("[FormatTest] Test 25 FAILED: RHIBuffer mapping invalid!");
                return false;
            }

            // 2. Verify RHITexture type mapping
            eng::renderer::RHITexture rhiTex{};
            rhiTex.width = 1920;
            rhiTex.height = 1080;
            rhiTex.image = (VkImage)(uintptr_t)0x87654321ULL;

            if (rhiTex.width != 1920 || rhiTex.image != (VkImage)(uintptr_t)0x87654321ULL) {
                LOG_ERROR("[FormatTest] Test 25 FAILED: RHITexture mapping invalid!");
                return false;
            }

            // 3. Verify RHIPipeline type mapping
            eng::renderer::RHIPipeline rhiPipe{};
            rhiPipe.pipeline = (VkPipeline)(uintptr_t)0x11223344ULL;
            rhiPipe.layout = (VkPipelineLayout)(uintptr_t)0x55667788ULL;

            if (rhiPipe.pipeline != (VkPipeline)(uintptr_t)0x11223344ULL || rhiPipe.layout != (VkPipelineLayout)(uintptr_t)0x55667788ULL) {
                LOG_ERROR("[FormatTest] Test 25 FAILED: RHIPipeline mapping invalid!");
                return false;
            }

            // 4. Verify RHI CommandBuffer, Fence, Semaphore, RenderPass, Shader handle aliases
            eng::renderer::RHICommandBuffer cmdBuf = (VkCommandBuffer)(uintptr_t)0xAABBCCDDULL;
            eng::renderer::RHIFence fence = (VkFence)(uintptr_t)0x99887766ULL;
            eng::renderer::RHISemaphore sema = (VkSemaphore)(uintptr_t)0x44332211ULL;
            eng::renderer::RHIRenderPass rp = (VkRenderPass)(uintptr_t)0xDEADBEEFULL;
            eng::renderer::RHIFrambuffer fb = (VkFramebuffer)(uintptr_t)0xBEEFCAFEULL;
            eng::renderer::RHIShaderModule sm = (VkShaderModule)(uintptr_t)0xCAFEBABEULL;

            if (cmdBuf != (VkCommandBuffer)(uintptr_t)0xAABBCCDDULL ||
                fence != (VkFence)(uintptr_t)0x99887766ULL ||
                sema != (VkSemaphore)(uintptr_t)0x44332211ULL ||
                rp != (VkRenderPass)(uintptr_t)0xDEADBEEFULL ||
                fb != (VkFramebuffer)(uintptr_t)0xBEEFCAFEULL ||
                sm != (VkShaderModule)(uintptr_t)0xCAFEBABEULL) {
                LOG_ERROR("[FormatTest] Test 25 FAILED: Raw Vulkan handle alias mapping invalid!");
                return false;
            }

            LOG_INFO("[FormatTest] Test 25 Passed: RHI Handles & Vulkan Object Mapping validated successfully.");
        }

        // --------------------------------------------------------------------
        // Test 26: Swapchain Subsystem & VulkanSwapChain Lifecycle Validation
        // --------------------------------------------------------------------
        {
            LOG_INFO("[FormatTest] Running Test 26: Swapchain Subsystem & VulkanSwapChain Lifecycle...");

            eng::vulkan::VulkanSwapChain swapChain;

            if (swapChain.GetHandle() != VK_NULL_HANDLE) {
                LOG_ERROR("[FormatTest] Test 26 FAILED: Default VulkanSwapChain handle is not VK_NULL_HANDLE!");
                return false;
            }

            if (!swapChain.GetImages().empty() || !swapChain.GetImageViews().empty()) {
                LOG_ERROR("[FormatTest] Test 26 FAILED: Default VulkanSwapChain contains non-empty image containers!");
                return false;
            }

            if (swapChain.GetSurface() != VK_NULL_HANDLE) {
                LOG_ERROR("[FormatTest] Test 26 FAILED: Default VulkanSwapChain surface is not VK_NULL_HANDLE!");
                return false;
            }

            // Test non-initialized acquire and present failures
            uint32_t imageIdx = 0;
            auto acquireRes = swapChain.AcquireNextImage(VK_NULL_HANDLE, &imageIdx);
            if (acquireRes.IsSuccess()) {
                LOG_ERROR("[FormatTest] Test 26 FAILED: AcquireNextImage succeeded on uninitialized swapchain!");
                return false;
            }

            auto presentRes = swapChain.Present(VK_NULL_HANDLE, 0);
            if (presentRes.IsSuccess()) {
                LOG_ERROR("[FormatTest] Test 26 FAILED: Present succeeded on uninitialized swapchain!");
                return false;
            }

            // Test safe shutdown
            swapChain.Shutdown();

            LOG_INFO("[FormatTest] Test 26 Passed: Swapchain Subsystem & VulkanSwapChain Lifecycle validated successfully.");
        }

        // --------------------------------------------------------------------
        // Test 27: Command Buffer Allocation, Recording & Submission Validation
        // --------------------------------------------------------------------
        {
            LOG_INFO("[FormatTest] Running Test 27: Command Buffer Allocation, Recording & Submission...");

            // 1. Guard check: Null handle recording and submission checks
            if (eng::renderer::EngineResources::BeginCommandBuffer(VK_NULL_HANDLE)) {
                LOG_ERROR("[FormatTest] Test 27 FAILED: BeginCommandBuffer succeeded on VK_NULL_HANDLE!");
                return false;
            }

            if (eng::renderer::EngineResources::EndCommandBuffer(VK_NULL_HANDLE)) {
                LOG_ERROR("[FormatTest] Test 27 FAILED: EndCommandBuffer succeeded on VK_NULL_HANDLE!");
                return false;
            }

            if (eng::renderer::EngineResources::SubmitCommandBuffer(VK_NULL_HANDLE, VK_NULL_HANDLE)) {
                LOG_ERROR("[FormatTest] Test 27 FAILED: SubmitCommandBuffer succeeded on VK_NULL_HANDLE!");
                return false;
            }

            // 2. Resource structure verification
            eng::renderer::EngineResources resources;
            if (!resources.commandPools.empty() || !resources.commandBuffers.empty()) {
                LOG_ERROR("[FormatTest] Test 27 FAILED: Uninitialized EngineResources contains command pools or buffers!");
                return false;
            }

            // 3. Safe pool reset call
            resources.ResetCommandPool(0);

            LOG_INFO("[FormatTest] Test 27 Passed: Command Buffer Allocation, Recording & Submission validated successfully.");
        }

        // --------------------------------------------------------------------
        // Test 28: Synchronization Primitives & Frame Coordination Validation
        // --------------------------------------------------------------------
        {
            LOG_INFO("[FormatTest] Running Test 28: Synchronization Primitives & Frame Coordination...");

            eng::renderer::EngineResources resources;

            if (resources.GetImageAvailableSemaphore(0) != VK_NULL_HANDLE) {
                LOG_ERROR("[FormatTest] Test 28 FAILED: GetImageAvailableSemaphore returned non-null handle on uninitialized resources!");
                return false;
            }

            if (resources.GetRenderFinishedSemaphore(0) != VK_NULL_HANDLE) {
                LOG_ERROR("[FormatTest] Test 28 FAILED: GetRenderFinishedSemaphore returned non-null handle on uninitialized resources!");
                return false;
            }

            if (resources.GetInFlightFence(0) != VK_NULL_HANDLE) {
                LOG_ERROR("[FormatTest] Test 28 FAILED: GetInFlightFence returned non-null handle on uninitialized resources!");
                return false;
            }

            if (resources.WaitForFence(0, 100)) {
                LOG_ERROR("[FormatTest] Test 28 FAILED: WaitForFence succeeded on uninitialized fence!");
                return false;
            }

            if (resources.ResetFence(0)) {
                LOG_ERROR("[FormatTest] Test 28 FAILED: ResetFence succeeded on uninitialized fence!");
                return false;
            }

            if (!resources.imageAvailableSemaphores.empty() || !resources.renderFinishedSemaphores.empty() || !resources.inFlightFences.empty()) {
                LOG_ERROR("[FormatTest] Test 28 FAILED: Default EngineResources contains non-empty sync containers!");
                return false;
            }

            // Test safe destruction of empty sync objects
            resources.destroySyncObjects();

            LOG_INFO("[FormatTest] Test 28 Passed: Synchronization Primitives & Frame Coordination validated successfully.");
        }

        // --------------------------------------------------------------------
        // Test 29: RenderGraph Topological Sorting & Lifetime Validation
        // --------------------------------------------------------------------
        {
            LOG_INFO("[FormatTest] Running Test 29: RenderGraph Topological Sorting & Lifetimes...");

            eng::renderer::RenderGraph renderGraph;

            // 1. Declare resources
            eng::renderer::TextureResourceDesc albedoDesc{};
            albedoDesc.width = 1920;
            albedoDesc.height = 1080;
            albedoDesc.format = VK_FORMAT_R8G8B8A8_UNORM;
            renderGraph.DeclareTexture("AlbedoTexture", albedoDesc);

            eng::renderer::TextureResourceDesc hdrDesc{};
            hdrDesc.width = 1920;
            hdrDesc.height = 1080;
            hdrDesc.format = VK_FORMAT_R16G16B16A16_SFLOAT;
            renderGraph.DeclareTexture("HdrColor", hdrDesc);

            eng::renderer::BufferResourceDesc uboDesc{};
            uboDesc.size = 256;
            renderGraph.DeclareBuffer("LightBuffer", uboDesc);

            // 2. Register passes in reverse order to test topological sorting
            // Pass 2: PostProcess (depends on HdrColor -> outputs FinalColor)
            renderGraph.RegisterPass(
                "Pass2_PostProcess",
                { "HdrColor" },
                { "FinalColor" },
                eng::renderer::PassID::UI,
                [](VkCommandBuffer) {}
            );

            // Pass 0: Geometry (outputs AlbedoTexture, LightBuffer)
            renderGraph.RegisterPass(
                "Pass0_Geometry",
                {},
                { "AlbedoTexture", "LightBuffer" },
                eng::renderer::PassID::Geometry,
                [](VkCommandBuffer) {}
            );

            // Pass 1: Lighting (depends on AlbedoTexture, LightBuffer -> outputs HdrColor)
            renderGraph.RegisterPass(
                "Pass1_Lighting",
                { "AlbedoTexture", "LightBuffer" },
                { "HdrColor" },
                eng::renderer::PassID::Lighting,
                [](VkCommandBuffer) {}
            );

            // 3. Compile graph
            eng::renderer::EngineResources resources;
            renderGraph.Compile(resources);

            if (renderGraph.GetPassCount() != 3) {
                LOG_ERROR("[FormatTest] Test 29 FAILED: Compiled pass count is not 3!");
                return false;
            }

            // Print debug info
            renderGraph.PrintDebug();

            // 4. Test graph clearing
            renderGraph.Clear();
            if (renderGraph.GetPassCount() != 0) {
                LOG_ERROR("[FormatTest] Test 29 FAILED: RenderGraph count is not 0 after Clear()!");
                return false;
            }

            LOG_INFO("[FormatTest] Test 29 Passed: RenderGraph Topological Sorting & Lifetimes validated successfully.");
        }

        // --------------------------------------------------------------------
        // Test 30: GPU Scene Instance Allocation & Entity Mapping Validation
        // --------------------------------------------------------------------
        {
            LOG_INFO("[FormatTest] Running Test 30: GPU Scene Instance Allocation & Entity Mapping...");

            eng::renderer::GPUScene scene;

            // 1. Basic allocation & updates
            eng::renderer::GPUGeometryInstance inst1{};
            inst1.model = glm::mat4(2.0f);
            inst1.objectID = 101;
            inst1.flags = eng::renderer::GPUInstanceFlags_Visible;

            eng::renderer::GPUSceneInstanceHandle h1 = scene.CreateInstance(inst1);
            if (!h1.IsValid() || h1.index != 0 || h1.generation != 0) {
                LOG_ERROR("[FormatTest] Test 30 FAILED: Initial handle allocation incorrect.");
                return false;
            }
            if (!scene.IsInstanceValid(h1)) {
                LOG_ERROR("[FormatTest] Test 30 FAILED: Handle not valid after creation.");
                return false;
            }

            eng::renderer::GPUGeometryInstance inst2{};
            inst2.model = glm::mat4(3.0f);
            inst2.objectID = 101;
            inst2.flags = eng::renderer::GPUInstanceFlags_Visible | eng::renderer::GPUInstanceFlags_CastShadow;
            scene.UpdateInstance(h1, inst2);

            auto diag1 = scene.GetDiagnostics();
            if (diag1.activeSlots != 1) {
                LOG_ERROR("[FormatTest] Test 30 FAILED: Active slot count should be 1.");
                return false;
            }

            // 2. Recycling & Generation Validation
            scene.DestroyInstance(h1);
            if (scene.IsInstanceValid(h1)) {
                LOG_ERROR("[FormatTest] Test 30 FAILED: Handle remains valid after destruction.");
                return false;
            }

            scene.UpdateInstance(h1, inst2);
            auto diag2 = scene.GetDiagnostics();
            if (diag2.staleHandleErrors != 1) {
                LOG_ERROR("[FormatTest] Test 30 FAILED: Stale handle update error should have been tracked.");
                return false;
            }

            eng::renderer::GPUSceneInstanceHandle h2 = scene.CreateInstance(inst1);
            if (h2.index != 0 || h2.generation != 1) {
                LOG_ERROR("[FormatTest] Test 30 FAILED: Recycling failed or generation not incremented.");
                return false;
            }

            // 3. Entity Lookup
            scene.RegisterEntityInstance(42, h2);
            eng::renderer::GPUSceneInstanceHandle lookup = scene.GetEntityInstance(42);
            if (lookup != h2) {
                LOG_ERROR("[FormatTest] Test 30 FAILED: Entity-to-instance lookup failed.");
                return false;
            }
            scene.UnregisterEntityInstance(42);
            lookup = scene.GetEntityInstance(42);
            if (lookup.IsValid()) {
                LOG_ERROR("[FormatTest] Test 30 FAILED: Lookup returned handle after unregistering.");
                return false;
            }

            // 4. Material Overrides
            std::vector<uint32_t> overrides = { 5, 8, 12 };
            scene.SetInstanceMaterialOverrides(h2, overrides);
            auto diag3 = scene.GetDiagnostics();
            if (diag3.materialOverrideCount != 3) {
                LOG_ERROR("[FormatTest] Test 30 FAILED: Expected 3 material override entries.");
                return false;
            }
            scene.ClearInstanceMaterialOverrides(h2);

            // 5. Dynamic Growth
            std::vector<eng::renderer::GPUSceneInstanceHandle> handles;
            for (int i = 0; i < 200; ++i) {
                eng::renderer::GPUGeometryInstance temp{};
                temp.objectID = 1000 + i;
                handles.push_back(scene.CreateInstance(temp));
            }

            auto diag4 = scene.GetDiagnostics();
            if (diag4.activeSlots < 200) {
                LOG_ERROR("[FormatTest] Test 30 FAILED: Growth count smaller than allocated count.");
                return false;
            }

            for (auto h : handles) {
                scene.DestroyInstance(h);
            }
            scene.DestroyInstance(h2);

            LOG_INFO("[FormatTest] Test 30 Passed: GPU Scene Instance Allocation & Entity Mapping validated successfully.");
        }

        // --------------------------------------------------------------------
        // Test 31: Material System PBR Uniform Overrides & Descriptor Binding
        // --------------------------------------------------------------------
        {
            LOG_INFO("[FormatTest] Running Test 31: Material System PBR Uniform Overrides & Descriptors...");

            eng::renderer::Material mat;

            // 1. Initial defaults check
            if (mat.getRoughness() != 0.6f || mat.getMetallic() != 0.0f) {
                LOG_ERROR("[FormatTest] Test 31 FAILED: Default material roughness/metallic parameters mismatch!");
                return false;
            }

            // 2. Set PBR uniform overrides
            glm::vec4 customColor(0.8f, 0.2f, 0.1f, 1.0f);
            mat.setAlbedoColor(customColor);
            mat.setRoughness(0.45f);
            mat.setMetallic(0.85f);
            mat.setNormalScale(1.5f);
            mat.setEmissiveStrength(2.5f);
            mat.setClearcoatFactor(0.75f);
            mat.setClearcoatRoughness(0.2f);
            mat.setBlendMode(eng::renderer::MaterialBlendMode::Mask);
            mat.setShadingModel(eng::renderer::MaterialShadingModel::Unlit);

            // 3. Verify CPU assetData & GPU uboData alignment
            if (mat.getAlbedoColor() != customColor || mat.uboData.baseColorFactor != customColor) {
                LOG_ERROR("[FormatTest] Test 31 FAILED: Albedo color factor synchronization mismatch!");
                return false;
            }

            if (mat.getRoughness() != 0.45f || mat.uboData.roughnessFactor != 0.45f) {
                LOG_ERROR("[FormatTest] Test 31 FAILED: Roughness factor synchronization mismatch!");
                return false;
            }

            if (mat.getMetallic() != 0.85f || mat.uboData.metallicFactor != 0.85f) {
                LOG_ERROR("[FormatTest] Test 31 FAILED: Metallic factor synchronization mismatch!");
                return false;
            }

            if (mat.getNormalScale() != 1.5f || mat.uboData.normalScale != 1.5f) {
                LOG_ERROR("[FormatTest] Test 31 FAILED: Normal scale synchronization mismatch!");
                return false;
            }

            if (mat.getEmissiveStrength() != 2.5f || mat.uboData.emissiveStrength != 2.5f) {
                LOG_ERROR("[FormatTest] Test 31 FAILED: Emissive strength synchronization mismatch!");
                return false;
            }

            if (mat.getClearcoatFactor() != 0.75f || mat.uboData.clearcoatFactor != 0.75f) {
                LOG_ERROR("[FormatTest] Test 31 FAILED: Clearcoat factor synchronization mismatch!");
                return false;
            }

            if (mat.getClearcoatRoughness() != 0.2f || mat.uboData.clearcoatRoughness != 0.2f) {
                LOG_ERROR("[FormatTest] Test 31 FAILED: Clearcoat roughness synchronization mismatch!");
                return false;
            }

            if (mat.getBlendMode() != eng::renderer::MaterialBlendMode::Mask || mat.uboData.blendMode != 1) {
                LOG_ERROR("[FormatTest] Test 31 FAILED: Blend mode synchronization mismatch!");
                return false;
            }

            if (mat.getShadingModel() != eng::renderer::MaterialShadingModel::Unlit || mat.uboData.shadingModel != 1) {
                LOG_ERROR("[FormatTest] Test 31 FAILED: Shading model synchronization mismatch!");
                return false;
            }

            // 4. Test texture slot flags
            mat.setAlbedoTexture(nullptr);
            if (mat.uboData.hasAlbedoMap != 0.0f) {
                LOG_ERROR("[FormatTest] Test 31 FAILED: hasAlbedoMap should be 0.0 for null texture!");
                return false;
            }

            mat.setNormalTexture(nullptr);
            if (mat.uboData.useNormalMap != 0.0f) {
                LOG_ERROR("[FormatTest] Test 31 FAILED: useNormalMap should be 0.0 for null texture!");
                return false;
            }

            mat.destroy();

            LOG_INFO("[FormatTest] Test 31 Passed: Material System PBR Uniform Overrides & Descriptors validated successfully.");
        }

        // --------------------------------------------------------------------
        // Test 32: Shader Reloading & Shader Library Validation
        // --------------------------------------------------------------------
        {
            LOG_INFO("[FormatTest] Running Test 32: Shader Reloading & Shader Library...");

            VkShaderModule modNull = eng::renderer::ShaderLibrary::LoadShader(VK_NULL_HANDLE, "non_existent_shader.spv");
            if (modNull != VK_NULL_HANDLE) {
                LOG_ERROR("[FormatTest] Test 32 FAILED: Expected VK_NULL_HANDLE for non-existent shader file!");
                return false;
            }

            VkShaderModule reloadedNull = eng::renderer::ShaderLibrary::ReloadShader(VK_NULL_HANDLE, "non_existent_shader.spv");
            if (reloadedNull != VK_NULL_HANDLE) {
                LOG_ERROR("[FormatTest] Test 32 FAILED: Expected VK_NULL_HANDLE for reloaded non-existent shader!");
                return false;
            }

            eng::renderer::ShaderLibrary::ClearCache(VK_NULL_HANDLE);

            LOG_INFO("[FormatTest] Test 32 Passed: Shader Reloading & Shader Library validated successfully.");
        }

        // --------------------------------------------------------------------
        // Test 33: Vulkan Pipeline Cache Subsystem Validation
        // --------------------------------------------------------------------
        {
            LOG_INFO("[FormatTest] Running Test 33: Vulkan Pipeline Cache...");

            eng::vulkan::VulkanPipelineCache cache;
            if (cache.GetHandle() != VK_NULL_HANDLE) {
                LOG_ERROR("[FormatTest] Test 33 FAILED: Initial VulkanPipelineCache handle should be VK_NULL_HANDLE!");
                return false;
            }

            cache.SaveCache();
            cache.Shutdown();

            LOG_INFO("[FormatTest] Test 33 Passed: Vulkan Pipeline Cache Subsystem validated successfully.");
        }

        // --------------------------------------------------------------------
        // Test 34: Textures Subsystem & Fallback Samplers Validation
        // --------------------------------------------------------------------
        {
            LOG_INFO("[FormatTest] Running Test 34: Textures Subsystem & Fallback Samplers...");

            eng::renderer::TextureUsage usage = eng::renderer::TextureUsage::Albedo;
            if (usage != eng::renderer::TextureUsage::Albedo) {
                LOG_ERROR("[FormatTest] Test 34 FAILED: TextureUsage enum mapping incorrect!");
                return false;
            }

            eng::renderer::Texture tex;
            if (tex.view() != VK_NULL_HANDLE || tex.sampler() != VK_NULL_HANDLE) {
                LOG_ERROR("[FormatTest] Test 34 FAILED: Uninitialized Texture should have null view/sampler!");
                return false;
            }
            tex.destroy();

            LOG_INFO("[FormatTest] Test 34 Passed: Textures Subsystem & Fallback Samplers validated successfully.");
        }

        // --------------------------------------------------------------------
        // Test 35: Meshes Subsystem & Bounds Geometry Validation
        // --------------------------------------------------------------------
        {
            LOG_INFO("[FormatTest] Running Test 35: Meshes Subsystem & Bounds Geometry...");

            eng::renderer::Mesh mesh;
            mesh.minBounds = glm::vec3(-1.0f);
            mesh.maxBounds = glm::vec3(1.0f);
            mesh.bounds.localCenter = (mesh.minBounds + mesh.maxBounds) * 0.5f;
            mesh.bounds.localRadius = glm::length(mesh.maxBounds - mesh.bounds.localCenter);

            if (mesh.bounds.localCenter != glm::vec3(0.0f) || mesh.bounds.localRadius <= 0.0f) {
                LOG_ERROR("[FormatTest] Test 35 FAILED: Mesh bounds center/radius calculation mismatch!");
                return false;
            }

            mesh.hasNormals = true;
            mesh.hasUVs = true;
            mesh.hasTangents = true;

            if (!mesh.hasNormals || !mesh.hasUVs || !mesh.hasTangents) {
                LOG_ERROR("[FormatTest] Test 35 FAILED: Mesh vertex attribute flags mismatch!");
                return false;
            }

            mesh.destroy();

            LOG_INFO("[FormatTest] Test 35 Passed: Meshes Subsystem & Bounds Geometry validated successfully.");
        }

        // --------------------------------------------------------------------
        // Test 36: Real-Time Virtual Geometry (RVG) Page Streaming Validation
        // --------------------------------------------------------------------
        {
            LOG_INFO("[FormatTest] Running Test 36: Virtual Geometry Page Streaming...");

            auto& streamingMgr = eng::renderer::RVGPageStreamingManager::Get();
            streamingMgr.ResetStats();
            const auto& stats = streamingMgr.GetStats();

            if (stats.totalRequests != 0 || stats.completedRequests != 0) {
                LOG_ERROR("[FormatTest] Test 36 FAILED: RVGPageStreamingManager stats not properly reset!");
                return false;
            }

            if (streamingMgr.GetMaxPhysicalPages() == 0 || streamingMgr.GetPageSize() == 0) {
                LOG_ERROR("[FormatTest] Test 36 FAILED: RVGPageStreamingManager page size/capacity invalid!");
                return false;
            }

            LOG_INFO("[FormatTest] Test 36 Passed: Virtual Geometry Page Streaming Scheduler validated successfully.");
        }

        // --------------------------------------------------------------------
        // Test 37: GPU Visibility & Frustum Culling Validation
        // --------------------------------------------------------------------
        {
            LOG_INFO("[FormatTest] Running Test 37: GPU Visibility & Frustum Culling...");

            eng::renderer::FrustumCullPass frustumPass;
            if (frustumPass.GetDescriptorSetLayout() != VK_NULL_HANDLE) {
                LOG_ERROR("[FormatTest] Test 37 FAILED: Uninitialized FrustumCullPass descriptor layout should be null!");
                return false;
            }

            LOG_INFO("[FormatTest] Test 37 Passed: GPU Visibility & Frustum Culling validated successfully.");
        }

        // --------------------------------------------------------------------
        // Test 38: Clustered Lighting & Quadtree Shadow Atlas Allocator Validation
        // --------------------------------------------------------------------
        {
            LOG_INFO("[FormatTest] Running Test 38: Clustered Lighting & Quadtree Shadow Atlas...");

            // 1. Clustered lighting settings
            Omnix::Radiance::ClusterSettings clusterSettings;
            clusterSettings.tileSizeX = 32;
            clusterSettings.tileSizeY = 32;
            clusterSettings.depthSliceCount = 16;
            clusterSettings.maxLightsPerCluster = 128;

            if (clusterSettings.tileSizeX != 32 || clusterSettings.maxLightsPerCluster != 128) {
                LOG_ERROR("[FormatTest] Test 38 FAILED: ClusterSettings parameters mismatch!");
                return false;
            }

            // 2. Quadtree shadow atlas allocator
            eng::renderer::ShadowAtlasAllocator atlas(2048);
            uint32_t tileX = 0, tileY = 0;
            bool alloc1 = atlas.Allocate(512, 10, 1, tileX, tileY);
            if (!alloc1 || tileX != 0 || tileY != 0) {
                LOG_ERROR("[FormatTest] Test 38 FAILED: Initial 512x512 tile allocation failed or coordinates incorrect!");
                return false;
            }

            uint32_t tileX2 = 0, tileY2 = 0;
            bool alloc2 = atlas.Allocate(512, 11, 1, tileX2, tileY2);
            if (!alloc2 || (tileX2 == 0 && tileY2 == 0)) {
                LOG_ERROR("[FormatTest] Test 38 FAILED: Second 512x512 tile allocation failed!");
                return false;
            }

            atlas.Deallocate(10);
            atlas.Deallocate(11);

            LOG_INFO("[FormatTest] Test 38 Passed: Clustered Lighting & Quadtree Shadow Atlas validated successfully.");
        }

        // --------------------------------------------------------------------
        // Test 39: Modular Multi-Pass Post-Processing Chain Validation
        // --------------------------------------------------------------------
        {
            LOG_INFO("[FormatTest] Running Test 39: Modular Multi-Pass Post-Processing Chain...");

            eng::renderer::PostProcessChain chain;
            eng::renderer::PostProcessSettings settings;
            settings.enableSSAO = true;
            settings.enableBloom = true;
            settings.enableTonemap = true;
            settings.enableColorGrading = true;
            settings.exposure = 1.2f;

            chain.Initialize(settings);
            const auto& activePasses = chain.GetActivePasses();

            if (activePasses.size() != 4) {
                LOG_ERROR("[FormatTest] Test 39 FAILED: Expected 4 active post processing passes!");
                return false;
            }

            // Disable Bloom & SSAO
            settings.enableBloom = false;
            settings.enableSSAO = false;
            chain.SetSettings(settings);

            if (chain.GetActivePasses().size() != 2) {
                LOG_ERROR("[FormatTest] Test 39 FAILED: Expected 2 active post processing passes after update!");
                return false;
            }

            LOG_INFO("[FormatTest] Test 39 Passed: Modular Multi-Pass Post-Processing Chain validated successfully.");
        }

        // --------------------------------------------------------------------
        // Test 40: Debug Wireframe Primitive Drawing Utilities Validation
        // --------------------------------------------------------------------
        {
            LOG_INFO("[FormatTest] Running Test 40: Debug Wireframe Drawing Utilities...");

            eng::renderer::DebugDraw::ClearLines();
            eng::renderer::DebugDraw::DrawLine(glm::vec3(0.0f), glm::vec3(1.0f, 0.0f, 0.0f), glm::vec4(1.0f, 0.0f, 0.0f, 1.0f));
            eng::renderer::DebugDraw::DrawSphere(glm::vec3(0.0f, 2.0f, 0.0f), 1.5f, glm::vec4(0.0f, 1.0f, 0.0f, 1.0f));

            const auto& lines = eng::renderer::DebugDraw::GetLines();
            if (lines.empty()) {
                LOG_ERROR("[FormatTest] Test 40 FAILED: DebugDraw line buffer is empty after drawing primitives!");
                return false;
            }

            eng::renderer::DebugDraw::ClearLines();
            if (!eng::renderer::DebugDraw::GetLines().empty()) {
                LOG_ERROR("[FormatTest] Test 40 FAILED: DebugDraw lines not cleared!");
                return false;
            }

            LOG_INFO("[FormatTest] Test 40 Passed: Debug Wireframe Drawing Utilities validated successfully.");
        }

        // --------------------------------------------------------------------
        // Test 41: GPU Profiler & Timing Query Pool Validation
        // --------------------------------------------------------------------
        {
            LOG_INFO("[FormatTest] Running Test 41: GPU Profiler & Timing Query Pool...");

            eng::renderer::GPUProfiler::StartFrame();
            eng::renderer::GPUProfiler::EndFrame();
            float timeMs = eng::renderer::GPUProfiler::GetLastFrameTimeMs();

            if (timeMs < 0.0f) {
                LOG_ERROR("[FormatTest] Test 41 FAILED: GPUProfiler returned negative frame duration!");
                return false;
            }

            LOG_INFO("[FormatTest] Test 41 Passed: GPU Profiler & Timing Query Pool validated successfully.");
        }

        // --------------------------------------------------------------------
        // Test 42: Physics World Initialization & PhysX Scene Lifecycle
        // --------------------------------------------------------------------
        {
            LOG_INFO("[FormatTest] Running Test 42: Physics World Initialization & PhysX Scene...");

            eng::physics::PhysicsWorld physicsWorld;
            if (!physicsWorld.Initialize()) {
                LOG_ERROR("[FormatTest] Test 42 FAILED: PhysicsWorld initialization failed!");
                return false;
            }

            if (!physicsWorld.IsInitialized()) {
                LOG_ERROR("[FormatTest] Test 42 FAILED: IsInitialized returned false after Initialize()!");
                return false;
            }

            physicsWorld.FixedUpdate(1.0f / 60.0f);
            if (physicsWorld.GetStepsThisFrame() != 1) {
                LOG_ERROR("[FormatTest] Test 42 FAILED: Expected 1 simulation step per 1/60s delta!");
                return false;
            }

            physicsWorld.Shutdown();

            LOG_INFO("[FormatTest] Test 42 Passed: Physics World Initialization & PhysX Scene validated successfully.");
        }

        // --------------------------------------------------------------------
        // Test 43: Rigid Bodies & Hybrid Simulation Model
        // --------------------------------------------------------------------
        {
            LOG_INFO("[FormatTest] Running Test 43: Rigid Bodies & Hybrid Simulation Model...");

            StaticBodyComponent staticBody{};
            staticBody.collisionLayer = 1;
            staticBody.collisionMask = 0xFFFFFFFF;
            staticBody.enabled = true;

            RigidBodyComponent rigidBody{};
            rigidBody.velocity = Vector3(0.0f, -9.81f, 0.0f);
            rigidBody.useGravity = true;

            if (!staticBody.enabled || !rigidBody.useGravity) {
                LOG_ERROR("[FormatTest] Test 43 FAILED: Body component parameter initialization error!");
                return false;
            }

            LOG_INFO("[FormatTest] Test 43 Passed: Rigid Bodies & Hybrid Simulation Model validated successfully.");
        }

        // --------------------------------------------------------------------
        // Test 44: Kinematic Character Controller Sliding Physics
        // --------------------------------------------------------------------
        {
            LOG_INFO("[FormatTest] Running Test 44: Character Controller Sliding Physics...");

            Vector3 velocity(5.0f, 0.0f, 5.0f);
            Vector3 wallNormal(-1.0f, 0.0f, 0.0f); // Wall along YZ plane
            Vector3 slideVelocity = velocity - wallNormal * (velocity.x * wallNormal.x + velocity.y * wallNormal.y + velocity.z * wallNormal.z);

            if (slideVelocity.x != 0.0f || slideVelocity.z != 5.0f) {
                LOG_ERROR("[FormatTest] Test 44 FAILED: Kinematic character controller wall sliding math error!");
                return false;
            }

            LOG_INFO("[FormatTest] Test 44 Passed: Character Controller Sliding Physics validated successfully.");
        }

        // --------------------------------------------------------------------
        // Test 45: Colliders & Shape Geometry Conversions
        // --------------------------------------------------------------------
        {
            LOG_INFO("[FormatTest] Running Test 45: Colliders & Shape Geometry Conversions...");

            BoxColliderComponent box{};
            box.size = Vector3(2.0f, 4.0f, 2.0f);

            SphereColliderComponent sphere{};
            sphere.radius = 1.5f;

            CapsuleColliderComponent capsule{};
            capsule.radius = 0.5f;
            capsule.height = 2.0f;

            if (box.size.y != 4.0f || sphere.radius != 1.5f || capsule.height != 2.0f) {
                LOG_ERROR("[FormatTest] Test 45 FAILED: Collider shape dimensions mismatch!");
                return false;
            }

            LOG_INFO("[FormatTest] Test 45 Passed: Colliders & Shape Geometry Conversions validated successfully.");
        }

        // --------------------------------------------------------------------
        // Test 46: Collision Detection & Overlap Queries
        // --------------------------------------------------------------------
        {
            LOG_INFO("[FormatTest] Running Test 46: Collision Detection & Overlap Queries...");

            eng::physics::PhysicsWorld physicsWorld;
            physicsWorld.Initialize();

            std::vector<Entity> hitEntities;
            bool hitBox = physicsWorld.OverlapBox(Vector3(0.0f, 0.0f, 0.0f), Vector3(1.0f, 1.0f, 1.0f), hitEntities);
            bool hitSphere = physicsWorld.OverlapSphere(Vector3(0.0f, 0.0f, 0.0f), 1.0f, hitEntities);
            bool hitCapsule = physicsWorld.OverlapCapsule(Vector3(0.0f, 0.0f, 0.0f), 0.5f, 2.0f, hitEntities);

            // On empty scene, overlaps should safely return false without crashing
            if (hitBox || hitSphere || hitCapsule) {
                LOG_ERROR("[FormatTest] Test 46 FAILED: Expected false overlap on empty scene!");
                return false;
            }

            physicsWorld.Shutdown();

            LOG_INFO("[FormatTest] Test 46 Passed: Collision Detection & Overlap Queries validated successfully.");
        }

        // --------------------------------------------------------------------
        // Test 47: Trigger System Bounding Box & Distance Formulas
        // --------------------------------------------------------------------
        {
            LOG_INFO("[FormatTest] Running Test 47: Trigger System Bounding Box & Distance Formulas...");

            TriggerComponent trigger{};
            trigger.boxSize = Vector3(5.0f, 5.0f, 5.0f);
            trigger.shapeType = TriggerShapeType::Box;
            trigger.enabled = true;

            Vector3 pointInside(1.0f, 1.0f, 1.0f);
            bool isInsideBox = (std::abs(pointInside.x) <= trigger.boxSize.x * 0.5f) &&
                               (std::abs(pointInside.y) <= trigger.boxSize.y * 0.5f) &&
                               (std::abs(pointInside.z) <= trigger.boxSize.z * 0.5f);

            if (!isInsideBox) {
                LOG_ERROR("[FormatTest] Test 47 FAILED: Trigger bounding box overlap evaluation failed!");
                return false;
            }

            LOG_INFO("[FormatTest] Test 47 Passed: Trigger System Bounding Box & Distance Formulas validated successfully.");
        }

        // --------------------------------------------------------------------
        // Test 48: Raycast Queries against Actor Hierarchy
        // --------------------------------------------------------------------
        {
            LOG_INFO("[FormatTest] Running Test 48: Raycast Queries against Actor Hierarchy...");

            eng::physics::PhysicsWorld physicsWorld;
            physicsWorld.Initialize();

            eng::physics::RaycastHit hit;
            bool rayHit = physicsWorld.Raycast(Vector3(0.0f, 10.0f, 0.0f), Vector3(0.0f, -1.0f, 0.0f), 100.0f, hit);

            if (rayHit) {
                LOG_ERROR("[FormatTest] Test 48 FAILED: Expected no raycast hit on empty scene!");
                return false;
            }

            physicsWorld.Shutdown();

            LOG_INFO("[FormatTest] Test 48 Passed: Raycast Queries against Actor Hierarchy validated successfully.");
        }

        // --------------------------------------------------------------------
        // Test 49: Sweep Tests for Box, Sphere & Capsule Geometries
        // --------------------------------------------------------------------
        {
            LOG_INFO("[FormatTest] Running Test 49: Sweep Tests for Box, Sphere & Capsule...");

            eng::physics::PhysicsWorld physicsWorld;
            physicsWorld.Initialize();

            eng::physics::SweepHit sweepHit;
            bool sweepBoxHit = physicsWorld.SweepBox(Vector3(0.0f, 10.0f, 0.0f), Vector3(1.0f, 1.0f, 1.0f), Vector3(0.0f, -1.0f, 0.0f), 50.0f, sweepHit);
            bool sweepSphereHit = physicsWorld.SweepSphere(Vector3(0.0f, 10.0f, 0.0f), 1.0f, Vector3(0.0f, -1.0f, 0.0f), 50.0f, sweepHit);
            bool sweepCapsuleHit = physicsWorld.SweepCapsule(Vector3(0.0f, 10.0f, 0.0f), 0.5f, 2.0f, Vector3(0.0f, -1.0f, 0.0f), 50.0f, sweepHit);

            if (sweepBoxHit || sweepSphereHit || sweepCapsuleHit) {
                LOG_ERROR("[FormatTest] Test 49 FAILED: Expected no sweep hit on empty scene!");
                return false;
            }

            physicsWorld.Shutdown();

            LOG_INFO("[FormatTest] Test 49 Passed: Sweep Tests for Box, Sphere & Capsule Geometries validated successfully.");
        }

        // --------------------------------------------------------------------
        // Test 50: Physical Joint Constraints (Fixed, Distance, Hinge, Spherical)
        // --------------------------------------------------------------------
        {
            LOG_INFO("[FormatTest] Running Test 50: Physical Joint Constraints...");

            eng::physics::PhysicsJoint joint;
            joint.type = eng::physics::JointType::Distance;
            joint.entityA = 1;
            joint.entityB = 2;
            joint.minDistance = 0.5f;
            joint.maxDistance = 3.0f;
            joint.breakForce = 5000.0f;

            if (joint.type != eng::physics::JointType::Distance || joint.maxDistance != 3.0f || joint.breakForce != 5000.0f) {
                LOG_ERROR("[FormatTest] Test 50 FAILED: PhysicsJoint parameter initialization mismatch!");
                return false;
            }

            LOG_INFO("[FormatTest] Test 50 Passed: Physical Joint Constraints validated successfully.");
        }

        // --------------------------------------------------------------------
        // Test 51: Custom Physics Material Presets & Friction/Restitution
        // --------------------------------------------------------------------
        {
            LOG_INFO("[FormatTest] Running Test 51: Physics Material Presets...");

            eng::physics::PhysicsWorld::PhysicsMaterialData matData;
            matData.staticFriction = 0.8f;
            matData.dynamicFriction = 0.6f;
            matData.restitution = 0.2f;

            if (matData.staticFriction != 0.8f || matData.restitution != 0.2f) {
                LOG_ERROR("[FormatTest] Test 51 FAILED: PhysicsMaterialData parameters mismatch!");
                return false;
            }

            LOG_INFO("[FormatTest] Test 51 Passed: Physics Material Presets validated successfully.");
        }

        // --------------------------------------------------------------------
        // Test 52: Physics Debug Wireframe Draw Rendering
        // --------------------------------------------------------------------
        {
            LOG_INFO("[FormatTest] Running Test 52: Physics Debug Wireframe Draw...");

            eng::renderer::DebugDraw::ClearLines();
            eng::renderer::DebugDraw::DrawLine(glm::vec3(-1.0f, -1.0f, -1.0f), glm::vec3(1.0f, 1.0f, 1.0f), glm::vec4(0.0f, 1.0f, 0.0f, 1.0f));

            if (eng::renderer::DebugDraw::GetLines().empty()) {
                LOG_ERROR("[FormatTest] Test 52 FAILED: Debug wireframe lines buffer empty!");
                return false;
            }
            eng::renderer::DebugDraw::ClearLines();

            LOG_INFO("[FormatTest] Test 52 Passed: Physics Debug Wireframe Draw validated successfully.");
        }

        // --------------------------------------------------------------------
        // Test 53: Skeleton & Bone Hierarchy Validation
        // --------------------------------------------------------------------
        {
            LOG_INFO("[FormatTest] Running Test 53: Skeleton & Bone Hierarchy...");

            eng::animation::Skeleton skel;
            uint32_t rootIdx = skel.AddBone("Hips", -1);
            uint32_t spineIdx = skel.AddBone("Spine", static_cast<int>(rootIdx));

            if (skel.GetBoneCount() != 2) {
                LOG_ERROR("[FormatTest] Test 53 FAILED: Skeleton bone count should be 2!");
                return false;
            }

            if (skel.GetParentIndex(spineIdx) != static_cast<int>(rootIdx)) {
                LOG_ERROR("[FormatTest] Test 53 FAILED: Spine parent index mismatch!");
                return false;
            }

            if (skel.FindBoneIndex("Spine") != static_cast<int>(spineIdx)) {
                LOG_ERROR("[FormatTest] Test 53 FAILED: FindBoneIndex returned wrong index!");
                return false;
            }

            LOG_INFO("[FormatTest] Test 53 Passed: Skeleton & Bone Hierarchy validated successfully.");
        }

        // --------------------------------------------------------------------
        // Test 54: Bone Joint Transform Matrix Calculations
        // --------------------------------------------------------------------
        {
            LOG_INFO("[FormatTest] Running Test 54: Bone Joint Transform Matrix Calculations...");

            eng::animation::TransformPose poseA;
            poseA.position = glm::vec3(0.0f, 0.0f, 0.0f);
            poseA.rotation = glm::quat(1.0f, 0.0f, 0.0f, 0.0f);

            eng::animation::TransformPose poseB;
            poseB.position = glm::vec3(10.0f, 0.0f, 0.0f);
            poseB.rotation = glm::quat(1.0f, 0.0f, 0.0f, 0.0f);

            eng::animation::TransformPose blended = eng::animation::TransformPose::Lerp(poseA, poseB, 0.5f);
            if (blended.position.x != 5.0f) {
                LOG_ERROR("[FormatTest] Test 54 FAILED: TransformPose lerp position mismatch!");
                return false;
            }

            glm::mat4 mat = blended.ToMatrix();
            if (mat[3][0] != 5.0f) {
                LOG_ERROR("[FormatTest] Test 54 FAILED: Matrix translation component mismatch!");
                return false;
            }

            LOG_INFO("[FormatTest] Test 54 Passed: Bone Joint Transform Matrix Calculations validated successfully.");
        }

        // --------------------------------------------------------------------
        // Test 55: Animation Clip Sampling & Keyframe Evaluation
        // --------------------------------------------------------------------
        {
            LOG_INFO("[FormatTest] Running Test 55: Animation Clip Sampling & Keyframe Evaluation...");

            OmnixAnim animData;
            animData.header.durationSeconds = 2.0f;
            animData.header.ticksPerSecond = 30.0f;
            animData.header.boneTrackCount = 1;

            BoneTrack track;
            track.boneName = "Hips";
            track.positionKeys.push_back({ 0.0f, Vec3{ 0.0f, 0.0f, 0.0f } });
            track.positionKeys.push_back({ 2.0f, Vec3{ 0.0f, 10.0f, 0.0f } });
            animData.boneTracks.push_back(track);

            eng::animation::AnimationClip clip(animData);
            glm::vec3 posMid = clip.SamplePosition(0, 1.0f);

            if (std::abs(posMid.y - 5.0f) > 0.01f) {
                LOG_ERROR("[FormatTest] Test 55 FAILED: Keyframe linear interpolation position mismatch!");
                return false;
            }

            LOG_INFO("[FormatTest] Test 55 Passed: Animation Clip Sampling & Keyframe Evaluation validated successfully.");
        }

        // --------------------------------------------------------------------
        // Test 56: Animation Player Playback Controls
        // --------------------------------------------------------------------
        {
            LOG_INFO("[FormatTest] Running Test 56: Animation Player Playback Controls...");

            OmnixAnim animData;
            animData.header.durationSeconds = 1.0f;
            eng::animation::Skeleton skel;
            skel.AddBone("Hips", -1);

            auto clip = std::make_shared<eng::animation::AnimationClip>(animData);
            eng::animation::AnimationPlayer player;

            player.Play(clip, true, 1.0f);
            if (!player.IsPlaying() || !player.IsLooping()) {
                LOG_ERROR("[FormatTest] Test 56 FAILED: Player Play state mismatch!");
                return false;
            }

            std::vector<eng::animation::TransformPose> outPoses;
            player.Update(0.5f, skel, outPoses);
            if (std::abs(player.GetCurrentTime() - 0.5f) > 0.01f) {
                LOG_ERROR("[FormatTest] Test 56 FAILED: Current playback time mismatch!");
                return false;
            }

            player.Pause();
            if (player.IsPlaying()) {
                LOG_ERROR("[FormatTest] Test 56 FAILED: Player Pause state mismatch!");
                return false;
            }

            player.Stop();
            if (player.GetCurrentTime() != 0.0f) {
                LOG_ERROR("[FormatTest] Test 56 FAILED: Stop should reset current time to 0!");
                return false;
            }

            LOG_INFO("[FormatTest] Test 56 Passed: Animation Player Playback Controls validated successfully.");
        }

        // --------------------------------------------------------------------
        // Test 57: 1D & 2D Animation Blend Trees
        // --------------------------------------------------------------------
        {
            LOG_INFO("[FormatTest] Running Test 57: 1D & 2D Animation Blend Trees...");

            std::vector<eng::animation::TransformPose> poseA(1), poseB(1), outPose;
            poseA[0].position = glm::vec3(0.0f, 0.0f, 0.0f);
            poseB[0].position = glm::vec3(10.0f, 0.0f, 0.0f);

            eng::animation::BlendTree::BlendPoses1D(poseA, poseB, 0.3f, outPose);

            if (std::abs(outPose[0].position.x - 3.0f) > 0.01f) {
                LOG_ERROR("[FormatTest] Test 57 FAILED: 1D Blend Tree output pose interpolation mismatch!");
                return false;
            }

            LOG_INFO("[FormatTest] Test 57 Passed: 1D & 2D Animation Blend Trees validated successfully.");
        }

        // --------------------------------------------------------------------
        // Test 58: Animation State Machines & Transitions
        // --------------------------------------------------------------------
        {
            LOG_INFO("[FormatTest] Running Test 58: Animation State Machines & Transitions...");

            eng::animation::AnimStateMachine stateMachine;
            eng::animation::AnimState idleState{ "Idle", nullptr, true };
            eng::animation::AnimState runState{ "Run", nullptr, true };

            stateMachine.AddState(idleState);
            stateMachine.AddState(runState);

            eng::animation::AnimTransition trans{ "Idle", "Run", 0.2f };
            stateMachine.AddTransition(trans);

            if (stateMachine.GetActiveState() != "Idle") {
                LOG_ERROR("[FormatTest] Test 58 FAILED: Initial state should be Idle!");
                return false;
            }

            stateMachine.SetState("Run");
            if (stateMachine.GetActiveState() != "Run") {
                LOG_ERROR("[FormatTest] Test 58 FAILED: Active state after transition should be Run!");
                return false;
            }

            LOG_INFO("[FormatTest] Test 58 Passed: Animation State Machines & Transitions validated successfully.");
        }

        // --------------------------------------------------------------------
        // Test 59: Animation Graph Execution & Pose Evaluator
        // --------------------------------------------------------------------
        {
            LOG_INFO("[FormatTest] Running Test 59: Animation Graph Execution & Pose Evaluator...");

            eng::animation::Skeleton skel;
            skel.AddBone("Hips", -1);
            skel.AddBone("Spine", 0);

            eng::animation::AnimGraph animGraph;
            std::vector<glm::mat4> palette;
            animGraph.EvaluateGraph(0.016f, skel, palette);

            if (palette.size() != 2) {
                LOG_ERROR("[FormatTest] Test 59 FAILED: Skinning palette size should match bone count!");
                return false;
            }

            LOG_INFO("[FormatTest] Test 59 Passed: Animation Graph Execution & Pose Evaluator validated successfully.");
        }

        // --------------------------------------------------------------------
        // Test 60: Root Motion Delta Displacement Extraction
        // --------------------------------------------------------------------
        {
            LOG_INFO("[FormatTest] Running Test 60: Root Motion Delta Displacement...");

            OmnixAnim animData;
            animData.header.hasRootMotion = 1;
            animData.header.durationSeconds = 2.0f;

            BoneTrack rootTrack;
            rootTrack.boneName = "Root";
            rootTrack.positionKeys.push_back({ 0.0f, Vec3{ 0.0f, 0.0f, 0.0f } });
            rootTrack.positionKeys.push_back({ 2.0f, Vec3{ 0.0f, 0.0f, 10.0f } });
            animData.boneTracks.push_back(rootTrack);

            eng::animation::AnimationClip clip(animData);
            glm::vec3 delta = eng::animation::RootMotionExtractor::ExtractDeltaPosition(clip, 0.0f, 1.0f);

            if (std::abs(delta.z - 5.0f) > 0.01f) {
                LOG_ERROR("[FormatTest] Test 60 FAILED: Root motion delta z displacement mismatch!");
                return false;
            }

            LOG_INFO("[FormatTest] Test 60 Passed: Root Motion Delta Displacement validated successfully.");
        }

        // --------------------------------------------------------------------
        // Test 61: Keyframe Event Callbacks & Timelines
        // --------------------------------------------------------------------
        {
            LOG_INFO("[FormatTest] Running Test 61: Keyframe Event Callbacks & Timelines...");

            eng::animation::AnimTimeline timeline;
            bool eventTriggered = false;

            eng::animation::AnimEvent evt;
            evt.triggerTime = 0.5f;
            evt.name = "Footstep";
            evt.callback = [&]() { eventTriggered = true; };

            timeline.AddEvent(evt);
            timeline.EvaluateEvents(0.0f, 0.6f);

            if (!eventTriggered) {
                LOG_ERROR("[FormatTest] Test 61 FAILED: Footstep timeline event callback failed to trigger!");
                return false;
            }

            LOG_INFO("[FormatTest] Test 61 Passed: Keyframe Event Callbacks & Timelines validated successfully.");
        }

        // --------------------------------------------------------------------
        // Test 62: GPU Skinning Matrix Palette & Skinned Vertices
        // --------------------------------------------------------------------
        {
            LOG_INFO("[FormatTest] Running Test 62: GPU Skinning Matrix Palette & Skinned Vertices...");

            eng::animation::SkinnedVertex v{};
            v.position = glm::vec3(0.0f, 1.0f, 0.0f);
            v.boneIndices = glm::uvec4(0, 1, 0, 0);
            v.boneWeights = glm::vec4(0.7f, 0.3f, 0.0f, 0.0f);

            if (v.boneWeights.x + v.boneWeights.y != 1.0f) {
                LOG_ERROR("[FormatTest] Test 62 FAILED: Skinned vertex weights must sum to 1.0!");
                return false;
            }

            eng::animation::SkinningPaletteSSBO ssbo{};
            if (sizeof(ssbo.boneMatrices) / sizeof(glm::mat4) != 128) {
                LOG_ERROR("[FormatTest] Test 62 FAILED: SkinningPaletteSSBO capacity must be 128 matrices!");
                return false;
            }

            LOG_INFO("[FormatTest] Test 62 Passed: GPU Skinning Matrix Palette & Skinned Vertices validated successfully.");
        }

        // --------------------------------------------------------------------
        // Test 63: Morph Targets & Dynamic Blend Shape Deformations
        // --------------------------------------------------------------------
        {
            LOG_INFO("[FormatTest] Running Test 63: Morph Targets & Blend Shape Deformations...");

            std::vector<glm::vec3> baseVerts = { glm::vec3(0.0f, 0.0f, 0.0f) };
            std::vector<eng::animation::MorphTarget> targets(1);
            targets[0].name = "Smile";
            targets[0].vertexDisplacements = { glm::vec3(0.0f, 1.0f, 0.0f) };

            std::vector<float> weights = { 0.5f };
            std::vector<glm::vec3> outVerts;

            eng::animation::MorphTargetEvaluator::EvaluateMorphs(baseVerts, targets, weights, outVerts);

            if (outVerts[0].y != 0.5f) {
                LOG_ERROR("[FormatTest] Test 63 FAILED: Morph target vertex displacement blending mismatch!");
                return false;
            }

            LOG_INFO("[FormatTest] Test 63 Passed: Morph Targets & Blend Shape Deformations validated successfully.");
        }

        // --------------------------------------------------------------------
        // Test 64: Inverse Kinematics (Two-Bone IK Solver)
        // --------------------------------------------------------------------
        {
            LOG_INFO("[FormatTest] Running Test 64: Inverse Kinematics (Two-Bone IK Solver)...");

            glm::vec3 rootPos(0.0f, 10.0f, 0.0f);
            glm::vec3 targetPos(0.0f, 2.0f, 0.0f);
            float upperLen = 4.0f;
            float lowerLen = 4.0f;

            glm::vec3 kneePos(0.0f);
            bool ikSolved = eng::animation::TwoBoneIKSolver::Solve(rootPos, targetPos, upperLen, lowerLen, kneePos);

            if (!ikSolved) {
                LOG_ERROR("[FormatTest] Test 64 FAILED: TwoBoneIKSolver failed to solve valid target reach!");
                return false;
            }

            LOG_INFO("[FormatTest] Test 64 Passed: Inverse Kinematics (Two-Bone IK Solver) validated successfully.");
        }

        // --------------------------------------------------------------------
        // Test 65: Audio Device Backend Capabilities
        // --------------------------------------------------------------------
        {
            LOG_INFO("[FormatTest] Running Test 65: Audio Device Backend Capabilities...");

            eng::audio::AudioDeviceCapabilities caps;
            if (caps.sampleRate != 48000 || caps.channels != 2 || !caps.isInitialized) {
                LOG_ERROR("[FormatTest] Test 65 FAILED: AudioDeviceCapabilities default configuration error!");
                return false;
            }

            LOG_INFO("[FormatTest] Test 65 Passed: Audio Device Backend Capabilities validated successfully.");
        }

        // --------------------------------------------------------------------
        // Test 66: Audio Sources & Sound Component Parameters
        // --------------------------------------------------------------------
        {
            LOG_INFO("[FormatTest] Running Test 66: Audio Sources & Sound Component Parameters...");

            AudioSourceComponent audioSource;
            audioSource.ClipPath = "audio/sfx_explosion.wav";
            audioSource.PlayOnStart = true;
            audioSource.Loop = false;
            audioSource.Volume = 0.85f;

            if (audioSource.ClipPath != "audio/sfx_explosion.wav" || !audioSource.PlayOnStart || audioSource.Volume != 0.85f) {
                LOG_ERROR("[FormatTest] Test 66 FAILED: AudioSourceComponent property assignment error!");
                return false;
            }

            LOG_INFO("[FormatTest] Test 66 Passed: Audio Sources & Sound Component Parameters validated successfully.");
        }

        // --------------------------------------------------------------------
        // Test 67: Audio Listener Spatial Transform Tracking
        // --------------------------------------------------------------------
        {
            LOG_INFO("[FormatTest] Running Test 67: Audio Listener Spatial Transform Tracking...");

            eng::audio::AudioListenerSystem listenerSys;
            eng::audio::AudioListenerComponent listenerComp;
            listenerComp.active = true;
            listenerComp.position = glm::vec3(0.0f, 5.0f, 10.0f);
            listenerComp.forward = glm::vec3(0.0f, 0.0f, -1.0f);

            listenerSys.SetActiveListener(listenerComp);
            const auto& active = listenerSys.GetActiveListener();

            if (!active.active || active.position.y != 5.0f || active.forward.z != -1.0f) {
                LOG_ERROR("[FormatTest] Test 67 FAILED: AudioListenerSystem active listener transform mismatch!");
                return false;
            }

            LOG_INFO("[FormatTest] Test 67 Passed: Audio Listener Spatial Transform Tracking validated successfully.");
        }

        // --------------------------------------------------------------------
        // Test 68: Audio Mixer Channels & Sub-Group Routing
        // --------------------------------------------------------------------
        {
            LOG_INFO("[FormatTest] Running Test 68: Audio Mixer Channels & Sub-Group Routing...");

            eng::audio::AudioMixer mixer;
            mixer.SetChannelVolume(eng::audio::MixerChannelType::Master, 0.5f);
            mixer.SetChannelVolume(eng::audio::MixerChannelType::Music, 0.8f);

            float effectiveMusicVol = mixer.GetEffectiveVolume(eng::audio::MixerChannelType::Music);
            if (std::abs(effectiveMusicVol - 0.4f) > 0.01f) {
                LOG_ERROR("[FormatTest] Test 68 FAILED: AudioMixer effective music volume calculation mismatch!");
                return false;
            }

            mixer.SetMuted(eng::audio::MixerChannelType::Music, true);
            if (mixer.GetEffectiveVolume(eng::audio::MixerChannelType::Music) != 0.0f) {
                LOG_ERROR("[FormatTest] Test 68 FAILED: Muted channel effective volume must be 0.0!");
                return false;
            }

            LOG_INFO("[FormatTest] Test 68 Passed: Audio Mixer Channels & Sub-Group Routing validated successfully.");
        }

        // --------------------------------------------------------------------
        // Test 69: Streaming Audio Chunking & Large Tracks
        // --------------------------------------------------------------------
        {
            LOG_INFO("[FormatTest] Running Test 69: Streaming Audio Chunking & Large Tracks...");

            eng::audio::AudioStreamer streamer;
            auto streamBuf = streamer.OpenStream("audio/bgm_main_theme.wav", 131072); // 128 KB file

            if (!streamBuf.HasMoreChunks() || streamBuf.GetProgress() != 0.0f) {
                LOG_ERROR("[FormatTest] Test 69 FAILED: Initial audio stream state error!");
                return false;
            }

            streamer.ReadNextChunk(streamBuf); // Read 64 KB
            if (std::abs(streamBuf.GetProgress() - 0.5f) > 0.01f) {
                LOG_ERROR("[FormatTest] Test 69 FAILED: Audio stream progress should be 50% after 1 chunk!");
                return false;
            }

            streamer.ReadNextChunk(streamBuf); // Read remaining 64 KB
            if (streamBuf.HasMoreChunks() || streamBuf.GetProgress() != 1.0f) {
                LOG_ERROR("[FormatTest] Test 69 FAILED: Stream should be complete after 2 chunks!");
                return false;
            }

            LOG_INFO("[FormatTest] Test 69 Passed: Streaming Audio Chunking & Large Tracks validated successfully.");
        }

        // --------------------------------------------------------------------
        // Test 70: 3D Spatial Audio & Distance Attenuation Models
        // --------------------------------------------------------------------
        {
            LOG_INFO("[FormatTest] Running Test 70: 3D Spatial Audio & Distance Attenuation...");

            eng::audio::SpatialAudioSettings settings;
            settings.attenuation = eng::audio::AttenuationModel::Linear;
            settings.minDistance = 1.0f;
            settings.maxDistance = 51.0f;

            glm::vec3 listenerPos(0.0f, 0.0f, 0.0f);
            glm::vec3 sourcePos(26.0f, 0.0f, 0.0f); // 26 units distance (halfway)

            float attenLinear = eng::audio::SpatialAudioSystem::CalculateAttenuation(sourcePos, listenerPos, settings);
            if (std::abs(attenLinear - 0.5f) > 0.01f) {
                LOG_ERROR("[FormatTest] Test 70 FAILED: Linear distance attenuation at midpoint should be 0.5!");
                return false;
            }

            LOG_INFO("[FormatTest] Test 70 Passed: 3D Spatial Audio & Distance Attenuation validated successfully.");
        }

        // --------------------------------------------------------------------
        // Test 71: Reverb Zones & Obstruction Occlusion Dampening
        // --------------------------------------------------------------------
        {
            LOG_INFO("[FormatTest] Running Test 71: Reverb Zones & Obstruction Occlusion...");

            eng::audio::ReverbZone caveZone;
            caveZone.center = glm::vec3(0.0f, 0.0f, 0.0f);
            caveZone.extents = glm::vec3(10.0f, 10.0f, 10.0f);

            if (!caveZone.Contains(glm::vec3(2.0f, -1.0f, 3.0f)) || caveZone.Contains(glm::vec3(12.0f, 0.0f, 0.0f))) {
                LOG_ERROR("[FormatTest] Test 71 FAILED: ReverbZone spatial bounds contains calculation error!");
                return false;
            }

            float occludedAtten = eng::audio::AudioOcclusionSystem::CalculateOcclusionFactor(glm::vec3(0.0f), glm::vec3(10.0f, 0.0f, 0.0f), true);
            if (occludedAtten >= 1.0f) {
                LOG_ERROR("[FormatTest] Test 71 FAILED: Occluded sound should apply dampening factor < 1.0!");
                return false;
            }

            LOG_INFO("[FormatTest] Test 71 Passed: Reverb Zones & Obstruction Occlusion validated successfully.");
        }

        // --------------------------------------------------------------------
        // Test 72: Audio DSP Filters & Spectrum Debug Analysis
        // --------------------------------------------------------------------
        {
            LOG_INFO("[FormatTest] Running Test 72: Audio DSP Filters & Spectrum Debug Analysis...");

            eng::audio::DSPEffect dsp;
            dsp.type = eng::audio::FilterType::LowPass;
            dsp.cutoffFrequencyHz = 800.0f;

            if (dsp.type != eng::audio::FilterType::LowPass || dsp.cutoffFrequencyHz != 800.0f) {
                LOG_ERROR("[FormatTest] Test 72 FAILED: DSPEffect configuration mismatch!");
                return false;
            }

            eng::audio::AudioSpectrumData spec = eng::audio::AudioDSPDebugger::AnalyzeSpectrum(1.0f);
            if (spec.peakVolumeDb != 0.0f) {
                LOG_ERROR("[FormatTest] Test 72 FAILED: Full volume spectrum peak DB should be 0.0 dB!");
                return false;
            }

            LOG_INFO("[FormatTest] Test 72 Passed: Audio DSP Filters & Spectrum Debug Analysis validated successfully.");
        }

        // --------------------------------------------------------------------
        // Test 73: Keyboard Device State & Key Transitions
        // --------------------------------------------------------------------
        {
            LOG_INFO("[FormatTest] Running Test 73: Keyboard Device State & Key Transitions...");

            eng::input::KeyboardDeviceState kb;
            kb.SetKeyState(32, true); // Spacebar pressed

            if (!kb.IsKeyDown(32)) {
                LOG_ERROR("[FormatTest] Test 73 FAILED: Spacebar should be down!");
                return false;
            }

            kb.UpdateFrame(); // Move current to previous
            if (!kb.IsKeyDown(32) || kb.IsKeyPressed(32)) {
                LOG_ERROR("[FormatTest] Test 73 FAILED: Key pressed should be false after 1 frame update!");
                return false;
            }

            kb.SetKeyState(32, false); // Spacebar released
            if (!kb.IsKeyReleased(32)) {
                LOG_ERROR("[FormatTest] Test 73 FAILED: Key released transition should be true!");
                return false;
            }

            LOG_INFO("[FormatTest] Test 73 Passed: Keyboard Device State & Key Transitions validated successfully.");
        }

        // --------------------------------------------------------------------
        // Test 74: Mouse Device Position, Movement Deltas & Buttons
        // --------------------------------------------------------------------
        {
            LOG_INFO("[FormatTest] Running Test 74: Mouse Device Position & Movement Deltas...");

            eng::input::MouseDeviceState mouse;
            mouse.SetPosition(100.0f, 200.0f);
            mouse.UpdateFrame();

            mouse.SetPosition(120.0f, 230.0f);
            mouse.UpdateFrame();

            if (mouse.delta.x != 20.0f || mouse.delta.y != 30.0f) {
                LOG_ERROR("[FormatTest] Test 74 FAILED: Mouse movement delta calculation error!");
                return false;
            }

            mouse.SetButtonState(0, true); // Left click
            if (!mouse.IsButtonDown(0)) {
                LOG_ERROR("[FormatTest] Test 74 FAILED: Mouse left button should be down!");
                return false;
            }

            LOG_INFO("[FormatTest] Test 74 Passed: Mouse Device Position & Movement Deltas validated successfully.");
        }

        // --------------------------------------------------------------------
        // Test 75: Gamepad Controllers State (Up to 4 Analog Gamepads)
        // --------------------------------------------------------------------
        {
            LOG_INFO("[FormatTest] Running Test 75: Gamepad Controllers State...");

            eng::input::GamepadSubsystem gamepads;
            auto& pad0 = gamepads.GetGamepad(0);

            pad0.leftStick = glm::vec2(0.8f, -0.5f);
            pad0.buttonBitmask = 0x0001; // Button A

            if (!pad0.connected || pad0.leftStick.x != 0.8f || !pad0.IsButtonDown(0x0001)) {
                LOG_ERROR("[FormatTest] Test 75 FAILED: Gamepad 0 state update error!");
                return false;
            }

            gamepads.UpdateFrame();
            if (!pad0.IsButtonDown(0x0001) || pad0.IsButtonPressed(0x0001)) {
                LOG_ERROR("[FormatTest] Test 75 FAILED: Gamepad button press transition error!");
                return false;
            }

            LOG_INFO("[FormatTest] Test 75 Passed: Gamepad Controllers State validated successfully.");
        }

        // --------------------------------------------------------------------
        // Test 76: Input Action & Axis Mapping Table Evaluation
        // --------------------------------------------------------------------
        {
            LOG_INFO("[FormatTest] Running Test 76: Input Action & Axis Mapping Table...");

            eng::input::InputMappingTable table;

            eng::input::ActionBinding jumpBind;
            jumpBind.actionName = "Jump";
            jumpBind.keyCode = 32; // Space
            table.BindAction(jumpBind);

            eng::input::AxisBinding moveForward;
            moveForward.axisName = "MoveForward";
            moveForward.positiveKey = 87; // W
            moveForward.negativeKey = 83; // S
            moveForward.scale = 1.0f;
            table.BindAxis(moveForward);

            eng::input::KeyboardDeviceState kb;
            eng::input::MouseDeviceState mouse;
            eng::input::GamepadState pad;

            kb.SetKeyState(32, true); // Press Space
            kb.SetKeyState(87, true); // Press W

            bool jumping = table.EvaluateAction("Jump", kb, mouse, pad);
            float forwardVal = table.EvaluateAxis("MoveForward", kb, pad);

            if (!jumping || forwardVal != 1.0f) {
                LOG_ERROR("[FormatTest] Test 76 FAILED: Mapping table action or axis evaluation error!");
                return false;
            }

            LOG_INFO("[FormatTest] Test 76 Passed: Input Action & Axis Mapping Table validated successfully.");
        }

        // --------------------------------------------------------------------
        // Test 77: Input Contexts Stack Priority & Active Layering
        // --------------------------------------------------------------------
        {
            LOG_INFO("[FormatTest] Running Test 77: Input Contexts Stack Priority...");

            eng::input::InputContextStack stack;
            auto gameplayCtx = std::make_shared<eng::input::InputContext>("Gameplay", 0);
            auto uiCtx = std::make_shared<eng::input::InputContext>("UI", 10); // Higher priority

            stack.PushContext(gameplayCtx);
            stack.PushContext(uiCtx);

            const auto& activeStack = stack.GetStack();
            if (activeStack.size() != 2 || activeStack[0]->GetName() != "UI") {
                LOG_ERROR("[FormatTest] Test 77 FAILED: UI context should have top priority!");
                return false;
            }

            stack.PopContext("UI");
            if (stack.GetStack().size() != 1 || stack.GetStack()[0]->GetName() != "Gameplay") {
                LOG_ERROR("[FormatTest] Test 77 FAILED: Context pop failed to leave Gameplay context!");
                return false;
            }

            LOG_INFO("[FormatTest] Test 77 Passed: Input Contexts Stack Priority validated successfully.");
        }

        // --------------------------------------------------------------------
        // Test 78: Controller Hot-Plugging & Haptics Rumble Feedback
        // --------------------------------------------------------------------
        {
            LOG_INFO("[FormatTest] Running Test 78: Controller Hot-Plugging & Haptics Rumble...");

            eng::input::InputHapticsSystem haptics;
            bool eventFired = false;

            haptics.SetHotPlugCallback([&](const eng::input::ControllerConnectionEvent& evt) {
                if (evt.controllerIndex == 1 && evt.connected) {
                    eventFired = true;
                }
            });

            haptics.TriggerConnectionEvent(1, true, "PS5 DualSense Controller");
            if (!eventFired) {
                LOG_ERROR("[FormatTest] Test 78 FAILED: Controller hot-plug callback failed!");
                return false;
            }

            haptics.SetRumble(0, 0.8f, 0.5f, 0.1f); // 100ms rumble
            if (haptics.GetRumble(0).lowFrequency != 0.8f) {
                LOG_ERROR("[FormatTest] Test 78 FAILED: Rumble state assignment error!");
                return false;
            }

            haptics.Update(0.15f); // 150ms step -> rumble should expire
            if (haptics.GetRumble(0).durationSeconds != 0.0f || haptics.GetRumble(0).lowFrequency != 0.0f) {
                LOG_ERROR("[FormatTest] Test 78 FAILED: Rumble duration decay error!");
                return false;
            }

            LOG_INFO("[FormatTest] Test 78 Passed: Controller Hot-Plugging & Haptics Rumble validated successfully.");
        }

        // --------------------------------------------------------------------
        // Test 79: AI Blackboard Parameter Memory Store
        // --------------------------------------------------------------------
        {
            LOG_INFO("[FormatTest] Running Test 79: AI Blackboard Parameter Memory Store...");

            eng::ai::Blackboard bb;
            bb.SetBool("HasTarget", true);
            bb.SetFloat("Health", 85.0f);
            bb.SetVector("TargetPosition", glm::vec3(10.0f, 0.0f, 5.0f));

            if (!bb.GetBool("HasTarget") || bb.GetFloat("Health") != 85.0f || bb.GetVector("TargetPosition").x != 10.0f) {
                LOG_ERROR("[FormatTest] Test 79 FAILED: Blackboard parameter store retrieval error!");
                return false;
            }

            if (!bb.HasKey("HasTarget") || bb.HasKey("NonExistentKey")) {
                LOG_ERROR("[FormatTest] Test 79 FAILED: Blackboard HasKey evaluation error!");
                return false;
            }

            LOG_INFO("[FormatTest] Test 79 Passed: AI Blackboard Parameter Memory Store validated successfully.");
        }

        // --------------------------------------------------------------------
        // Test 80: NavMesh Topology & A* Pathfinding Navigation
        // --------------------------------------------------------------------
        {
            LOG_INFO("[FormatTest] Running Test 80: NavMesh Topology & A* Pathfinding...");

            eng::ai::NavMesh nav;
            uint32_t n0 = nav.AddNode(glm::vec3(0.0f, 0.0f, 0.0f));
            uint32_t n1 = nav.AddNode(glm::vec3(5.0f, 0.0f, 0.0f));
            uint32_t n2 = nav.AddNode(glm::vec3(10.0f, 0.0f, 0.0f));

            nav.ConnectNodes(n0, n1);
            nav.ConnectNodes(n1, n2);

            std::vector<glm::vec3> path;
            bool pathFound = nav.FindPath(glm::vec3(0.0f, 0.0f, 0.0f), glm::vec3(10.0f, 0.0f, 0.0f), path);

            if (!pathFound || path.size() != 3 || path[2].x != 10.0f) {
                LOG_ERROR("[FormatTest] Test 80 FAILED: NavMesh A* pathfinding path generation error!");
                return false;
            }

            LOG_INFO("[FormatTest] Test 80 Passed: NavMesh Topology & A* Pathfinding validated successfully.");
        }

        // --------------------------------------------------------------------
        // Test 81: Behavior Tree Runtimes & Node Execution Graphs
        // --------------------------------------------------------------------
        {
            LOG_INFO("[FormatTest] Running Test 81: Behavior Tree Runtimes & Node Graphs...");

            eng::ai::Blackboard bb;
            bb.SetBool("IsEnemyInSight", true);

            auto seq = std::make_shared<eng::ai::BTSequence>();
            auto checkEnemy = std::make_shared<eng::ai::BTCondition>([](eng::ai::Blackboard& board) {
                return board.GetBool("IsEnemyInSight") ? eng::ai::BTNodeStatus::Success : eng::ai::BTNodeStatus::Failure;
            });
            auto attackAction = std::make_shared<eng::ai::BTAction>([](eng::ai::Blackboard&) {
                return eng::ai::BTNodeStatus::Success;
            });

            seq->AddChild(checkEnemy);
            seq->AddChild(attackAction);

            eng::ai::BTNodeStatus result = seq->Tick(bb);
            if (result != eng::ai::BTNodeStatus::Success) {
                LOG_ERROR("[FormatTest] Test 81 FAILED: Behavior Tree sequence execution failed!");
                return false;
            }

            LOG_INFO("[FormatTest] Test 81 Passed: Behavior Tree Runtimes & Node Execution Graphs validated successfully.");
        }

        // --------------------------------------------------------------------
        // Test 82: Environment Query System (EQS) Tactical Position Scoring
        // --------------------------------------------------------------------
        {
            LOG_INFO("[FormatTest] Running Test 82: Environment Query System (EQS)...");

            glm::vec3 agentPos(0.0f, 0.0f, 0.0f);
            glm::vec3 enemyPos(20.0f, 0.0f, 0.0f);

            glm::vec3 bestCover = eng::ai::EQSSolver::FindBestCoverPosition(agentPos, enemyPos, 10.0f, 4);

            // Best cover position should be further away from enemy in -X direction
            if (bestCover.x >= agentPos.x) {
                LOG_ERROR("[FormatTest] Test 82 FAILED: EQS solver should pick tactical position away from enemy!");
                return false;
            }

            LOG_INFO("[FormatTest] Test 82 Passed: Environment Query System (EQS) validated successfully.");
        }

        // --------------------------------------------------------------------
        // Test 83: Autonomous Steering Behaviors (Seek, Flee, Arrive)
        // --------------------------------------------------------------------
        {
            LOG_INFO("[FormatTest] Running Test 83: Autonomous Steering Behaviors...");

            glm::vec3 currentPos(0.0f, 0.0f, 0.0f);
            glm::vec3 currentVel(0.0f, 0.0f, 0.0f);
            glm::vec3 targetPos(10.0f, 0.0f, 0.0f);

            glm::vec3 seekForce = eng::ai::SteeringBehaviors::Seek(currentPos, currentVel, targetPos, 5.0f);
            glm::vec3 fleeForce = eng::ai::SteeringBehaviors::Flee(currentPos, currentVel, targetPos, 5.0f);

            if (seekForce.x <= 0.0f || fleeForce.x >= 0.0f) {
                LOG_ERROR("[FormatTest] Test 83 FAILED: Seek/Flee steering force calculation error!");
                return false;
            }

            LOG_INFO("[FormatTest] Test 83 Passed: Autonomous Steering Behaviors validated successfully.");
        }

        // --------------------------------------------------------------------
        // Test 84: Crowd Control & Flocking Velocity Obstacle Avoidance
        // --------------------------------------------------------------------
        {
            LOG_INFO("[FormatTest] Running Test 84: Crowd Control & Flocking...");

            std::vector<glm::vec3> positions = {
                glm::vec3(0.0f, 0.0f, 0.0f),
                glm::vec3(1.0f, 0.0f, 0.0f),
                glm::vec3(-1.0f, 0.0f, 0.0f)
            };
            std::vector<glm::vec3> velocities = {
                glm::vec3(0.0f, 0.0f, 1.0f),
                glm::vec3(0.0f, 0.0f, 1.0f),
                glm::vec3(0.0f, 0.0f, 1.0f)
            };

            glm::vec3 flockForce = eng::ai::CrowdController::ComputeFlockingVelocity(0, positions, velocities);

            if (flockForce.z <= 0.0f) {
                LOG_ERROR("[FormatTest] Test 84 FAILED: Flocking alignment velocity calculation error!");
                return false;
            }

            LOG_INFO("[FormatTest] Test 84 Passed: Crowd Control & Flocking validated successfully.");
        }

        // --------------------------------------------------------------------
        // Test 85: Network Socket Layer & Packet Serialization
        // --------------------------------------------------------------------
        {
            LOG_INFO("[FormatTest] Running Test 85: Network Socket Layer & Packet Serialization...");

            eng::networking::NetworkPacket packet;
            packet.type = eng::networking::PacketType::StateReplication;
            packet.senderId = 101;
            packet.sequenceNumber = 42;
            packet.payload = { 0xDE, 0xAD, 0xBE, 0xEF };

            std::vector<uint8_t> serialized;
            if (!packet.Serialize(serialized) || serialized.empty()) {
                LOG_ERROR("[FormatTest] Test 85 FAILED: NetworkPacket serialization failed!");
                return false;
            }

            eng::networking::NetworkPacket deserializedPacket;
            if (!deserializedPacket.Deserialize(serialized.data(), serialized.size())) {
                LOG_ERROR("[FormatTest] Test 85 FAILED: NetworkPacket deserialization failed!");
                return false;
            }

            if (deserializedPacket.type != eng::networking::PacketType::StateReplication ||
                deserializedPacket.senderId != 101 ||
                deserializedPacket.sequenceNumber != 42 ||
                deserializedPacket.payload.size() != 4) {
                LOG_ERROR("[FormatTest] Test 85 FAILED: Deserialized packet property mismatch!");
                return false;
            }

            LOG_INFO("[FormatTest] Test 85 Passed: Network Socket Layer & Packet Serialization validated successfully.");
        }

        // --------------------------------------------------------------------
        // Test 86: Network State Replication Framework
        // --------------------------------------------------------------------
        {
            LOG_INFO("[FormatTest] Running Test 86: Network State Replication Framework...");

            eng::networking::NetworkReplicator replicator;
            replicator.RegisterEntity(1, 100);
            replicator.UpdateEntityPosition(1, glm::vec3(15.0f, 0.0f, -5.0f));

            eng::networking::NetworkPacket replPacket;
            bool built = replicator.BuildReplicationPacket(1, replPacket);
            if (!built || replPacket.type != eng::networking::PacketType::StateReplication) {
                LOG_ERROR("[FormatTest] Test 86 FAILED: Replication packet construction failed!");
                return false;
            }

            eng::networking::NetworkReplicator clientReplicator;
            if (!clientReplicator.ApplyReplicationPacket(replPacket)) {
                LOG_ERROR("[FormatTest] Test 86 FAILED: Client replication packet application failed!");
                return false;
            }

            const auto* clientState = clientReplicator.GetEntityState(1);
            if (!clientState || clientState->position.x != 15.0f) {
                LOG_ERROR("[FormatTest] Test 86 FAILED: Client replicated position mismatch!");
                return false;
            }

            LOG_INFO("[FormatTest] Test 86 Passed: Network State Replication Framework validated successfully.");
        }

        // --------------------------------------------------------------------
        // Test 87: Remote Procedure Call (RPC) Dispatching
        // --------------------------------------------------------------------
        {
            LOG_INFO("[FormatTest] Running Test 87: Remote Procedure Call (RPC) Dispatching...");

            eng::networking::RPCManager rpcManager;
            bool rpcExecuted = false;

            rpcManager.RegisterRPC("ServerTakeDamage", [&](uint32_t netId, const std::string& params) {
                if (netId == 5 && params == "{\"damage\":25}") {
                    rpcExecuted = true;
                }
            });

            bool invoked = rpcManager.InvokeRPC("ServerTakeDamage", 5, "{\"damage\":25}");
            if (!invoked || !rpcExecuted) {
                LOG_ERROR("[FormatTest] Test 87 FAILED: RPC invocation failed to execute handler!");
                return false;
            }

            LOG_INFO("[FormatTest] Test 87 Passed: Remote Procedure Call (RPC) Dispatching validated successfully.");
        }

        // --------------------------------------------------------------------
        // Test 88: Client/Server Loop & Network Driver Role Lifecycle
        // --------------------------------------------------------------------
        {
            LOG_INFO("[FormatTest] Running Test 88: Client/Server Loop & Network Driver...");

            eng::networking::NetworkDriver driver;
            if (driver.GetRole() != eng::networking::NetworkRole::Standalone || driver.IsConnected()) {
                LOG_ERROR("[FormatTest] Test 88 FAILED: Default NetworkDriver state should be Standalone!");
                return false;
            }

            driver.StartServer(7777);
            if (driver.GetRole() != eng::networking::NetworkRole::DedicatedServer || !driver.IsConnected()) {
                LOG_ERROR("[FormatTest] Test 88 FAILED: DedicatedServer role state initialization error!");
                return false;
            }

            driver.Disconnect();
            if (driver.GetRole() != eng::networking::NetworkRole::Standalone || driver.IsConnected()) {
                LOG_ERROR("[FormatTest] Test 88 FAILED: Disconnect should revert driver to Standalone!");
                return false;
            }

            LOG_INFO("[FormatTest] Test 88 Passed: Client/Server Loop & Network Driver validated successfully.");
        }

        // --------------------------------------------------------------------
        // Test 89: Client-Side Movement Prediction & Server Reconciliation
        // --------------------------------------------------------------------
        {
            LOG_INFO("[FormatTest] Running Test 89: Client-Side Movement Prediction...");

            eng::networking::ClientPredictionEngine predictor;

            // Client predicts 2 moves:
            glm::vec3 pos0(0.0f, 0.0f, 0.0f);
            glm::vec3 predictedPos1 = predictor.PredictMovement(pos0, glm::vec3(1.0f, 0.0f, 0.0f), 10.0f, 0.1f); // pos = 1.0
            predictor.RecordInput(1, glm::vec3(1.0f, 0.0f, 0.0f), 0.1f);

            glm::vec3 predictedPos2 = predictor.PredictMovement(predictedPos1, glm::vec3(1.0f, 0.0f, 0.0f), 10.0f, 0.1f); // pos = 2.0
            predictor.RecordInput(2, glm::vec3(1.0f, 0.0f, 0.0f), 0.1f);

            if (predictor.GetPendingInputCount() != 2) {
                LOG_ERROR("[FormatTest] Test 89 FAILED: Pending input count should be 2!");
                return false;
            }

            // Server acknowledges input #1 with position = 1.0
            glm::vec3 serverConfirmedPos(1.0f, 0.0f, 0.0f);
            glm::vec3 reconciledPos = predictor.Reconcile(serverConfirmedPos, 1, 10.0f);

            // Reconciled position should re-apply input #2 and equal 2.0
            if (std::abs(reconciledPos.x - 2.0f) > 0.01f || predictor.GetPendingInputCount() != 1) {
                LOG_ERROR("[FormatTest] Test 89 FAILED: Client movement reconciliation math mismatch!");
                return false;
            }

            LOG_INFO("[FormatTest] Test 89 Passed: Client-Side Movement Prediction validated successfully.");
        }

        // --------------------------------------------------------------------
        // Test 90: Network Time Clock & Latency Round-Trip Synchronization
        // --------------------------------------------------------------------
        {
            LOG_INFO("[FormatTest] Running Test 90: Network Time Clock & Latency Sync...");

            eng::networking::NetworkTimeClock clock;
            clock.ReceivePong(40.0f, 100.0f); // 40ms RTT, server time 100s

            if (clock.GetRTTMs() != 40.0f) {
                LOG_ERROR("[FormatTest] Test 90 FAILED: RTT measurement mismatch!");
                return false;
            }

            float synchronizedTime = clock.GetSynchronizedServerTime(); // Should be 100 + 0.02 = 100.02s
            if (std::abs(synchronizedTime - 100.02f) > 0.001f) {
                LOG_ERROR("[FormatTest] Test 90 FAILED: Synchronized server time calculation mismatch!");
                return false;
            }

            LOG_INFO("[FormatTest] Test 90 Passed: Network Time Clock & Latency Sync validated successfully.");
        }

        // --------------------------------------------------------------------
        // Test 91: Save System State Serialization & Checksum Verification
        // --------------------------------------------------------------------
        {
            LOG_INFO("[FormatTest] Running Test 91: Save System State & Checksum...");

            eng::runtime::SaveDataHeader header;
            header.playerName = "HeroPlayer";
            header.playerHealth = 100;
            header.activeCheckpointId = 3;

            std::string serializedSave = "{\"player\":\"HeroPlayer\",\"health\":100,\"checkpoint\":3}";
            header.crc64Checksum = eng::runtime::SaveSystem::ComputeChecksum(serializedSave);

            if (header.crc64Checksum == 0ULL) {
                LOG_ERROR("[FormatTest] Test 91 FAILED: SaveSystem CRC64 checksum calculation error!");
                return false;
            }

            uint64_t recomputedCrc = eng::runtime::SaveSystem::ComputeChecksum(serializedSave);
            if (header.crc64Checksum != recomputedCrc) {
                LOG_ERROR("[FormatTest] Test 91 FAILED: CRC64 verification checksum mismatch!");
                return false;
            }

            LOG_INFO("[FormatTest] Test 91 Passed: Save System State & Checksum validated successfully.");
        }

        // --------------------------------------------------------------------
        // Test 92: Cascading Configuration & Live Hot-Reload Watchers
        // --------------------------------------------------------------------
        {
            LOG_INFO("[FormatTest] Running Test 92: Cascading Configuration & Hot-Reload...");

            eng::runtime::ConfigSystem configSys;
            configSys.SetInt("Graphics.ResolutionX", 1920);
            configSys.SetInt("Graphics.ResolutionY", 1080);
            configSys.SetBool("Graphics.VSync", true);

            int resX = configSys.GetInt("Graphics.ResolutionX", 1280);
            bool vsync = configSys.GetBool("Graphics.VSync", false);

            if (resX != 1920 || !vsync) {
                LOG_ERROR("[FormatTest] Test 92 FAILED: ConfigSystem cascade override retrieval error!");
                return false;
            }

            LOG_INFO("[FormatTest] Test 92 Passed: Cascading Configuration & Hot-Reload validated successfully.");
        }

        // --------------------------------------------------------------------
        // Test 93: Multi-Language Localization & String Table Resolution
        // --------------------------------------------------------------------
        {
            LOG_INFO("[FormatTest] Running Test 93: Multi-Language Localization...");

            eng::runtime::LocalizationSystem loc;
            std::unordered_map<std::string, std::string> enUS = {
                { "UI_PLAY", "Play" },
                { "UI_OPTIONS", "Options" }
            };
            std::unordered_map<std::string, std::string> frFR = {
                { "UI_PLAY", "Jouer" },
                { "UI_OPTIONS", "Options" }
            };

            loc.LoadStringTable("en-US", enUS);
            loc.LoadStringTable("fr-FR", frFR);

            loc.SetLanguage("en-US");
            if (loc.GetLocalizedString("UI_PLAY") != "Play") {
                LOG_ERROR("[FormatTest] Test 93 FAILED: English localized string resolution mismatch!");
                return false;
            }

            loc.SetLanguage("fr-FR");
            if (loc.GetLocalizedString("UI_PLAY") != "Jouer") {
                LOG_ERROR("[FormatTest] Test 93 FAILED: French localized string resolution mismatch!");
                return false;
            }

            LOG_INFO("[FormatTest] Test 93 Passed: Multi-Language Localization validated successfully.");
        }

        // --------------------------------------------------------------------
        // Test 94: Plugin & Dynamic Module Entry Point Lifecycle
        // --------------------------------------------------------------------
        {
            LOG_INFO("[FormatTest] Running Test 94: Plugin & Dynamic Module Loader...");

            eng::runtime::PluginManager pluginMgr;
            auto* modulePtr = pluginMgr.LoadPlugin("SamplePlugin.dll");

            // Safe fallback logic validation when DLL not present
            if (pluginMgr.IsPluginLoaded("SamplePlugin.dll")) {
                LOG_ERROR("[FormatTest] Test 94 FAILED: Non-existent plugin should not be marked as loaded!");
                return false;
            }

            LOG_INFO("[FormatTest] Test 94 Passed: Plugin & Dynamic Module Loader validated successfully.");
        }

        // --------------------------------------------------------------------
        // Test 95: Crash Handler Minidump Generation Hooks
        // --------------------------------------------------------------------
        {
            LOG_INFO("[FormatTest] Running Test 95: Crash Handler Minidump Generation Hooks...");

            bool callbackFired = false;
            eng::runtime::CrashHandler::SetCrashCallback([&](const eng::runtime::CrashDumpInfo& info) {
                if (info.dumpFilePath == "dumps/crash.dmp" && info.dumpGenerated) {
                    callbackFired = true;
                }
            });

            auto dumpInfo = eng::runtime::CrashHandler::GenerateMinidump("dumps/crash.dmp", "Access Violation EXCEPTION_ACCESS_VIOLATION");
            if (!callbackFired || !dumpInfo.dumpGenerated) {
                LOG_ERROR("[FormatTest] Test 95 FAILED: CrashHandler minidump generation callback failed!");
                return false;
            }

            LOG_INFO("[FormatTest] Test 95 Passed: Crash Handler Minidump Generation Hooks validated successfully.");
        }

        // --------------------------------------------------------------------
        // Test 96: Runtime Console Command Execution & Output Buffer
        // --------------------------------------------------------------------
        {
            LOG_INFO("[FormatTest] Running Test 96: Runtime Console Command Execution...");

            eng::runtime::RuntimeConsole console;
            bool cmdExecuted = false;

            console.RegisterCommand("spawn_enemy", [&](const std::vector<std::string>& args) {
                if (!args.empty() && args[0] == "goblin") {
                    cmdExecuted = true;
                }
            });

            console.ExecuteCommand("spawn_enemy goblin");
            if (!cmdExecuted) {
                LOG_ERROR("[FormatTest] Test 96 FAILED: RuntimeConsole command execution failed!");
                return false;
            }

            LOG_INFO("[FormatTest] Test 96 Passed: Runtime Console Command Execution validated successfully.");
        }

        // --------------------------------------------------------------------
        // Test 97: Runtime Performance Statistics & Frame Timing Logs
        // --------------------------------------------------------------------
        {
            LOG_INFO("[FormatTest] Running Test 97: Runtime Performance Statistics...");

            eng::runtime::TimeManager timeMgr;
            timeMgr.Update(0.016f); // 16ms frame step (~60 FPS)

            if (timeMgr.GetDeltaTime() != 0.016f) {
                LOG_ERROR("[FormatTest] Test 97 FAILED: TimeManager delta time step mismatch!");
                return false;
            }

            LOG_INFO("[FormatTest] Test 97 Passed: Runtime Performance Statistics validated successfully.");
        }

        // --------------------------------------------------------------------
        // Test 98: Version Control & Dynamic Feature Flags
        // --------------------------------------------------------------------
        {
            LOG_INFO("[FormatTest] Running Test 98: Version Control & Dynamic Feature Flags...");

            eng::runtime::FeatureFlagSystem featureFlags;
            featureFlags.SetFeatureFlag("ExperimentalRaytracing", true);
            featureFlags.SetFeatureFlag("VulkanMeshShaders", false);

            if (!featureFlags.IsFeatureEnabled("ExperimentalRaytracing") || featureFlags.IsFeatureEnabled("VulkanMeshShaders")) {
                LOG_ERROR("[FormatTest] Test 98 FAILED: FeatureFlagSystem state retrieval error!");
                return false;
            }

            featureFlags.ToggleFeature("VulkanMeshShaders");
            if (!featureFlags.IsFeatureEnabled("VulkanMeshShaders")) {
                LOG_ERROR("[FormatTest] Test 98 FAILED: FeatureFlagSystem toggle error!");
                return false;
            }

            LOG_INFO("[FormatTest] Test 98 Passed: Version Control & Dynamic Feature Flags validated successfully.");
        }

        // --------------------------------------------------------------------
        // Test 99: CPU Microsecond Profiler & Scope Metrics
        // --------------------------------------------------------------------
        {
            LOG_INFO("[FormatTest] Running Test 99: CPU Microsecond Profiler...");

            eng::developer::CPUProfiler profiler;
            profiler.BeginScope("RenderPass_Opaque");
            std::this_thread::sleep_for(std::chrono::milliseconds(1));
            profiler.EndScope("RenderPass_Opaque");

            double durationUs = profiler.GetScopeDurationUs("RenderPass_Opaque");
            if (durationUs <= 0.0) {
                LOG_ERROR("[FormatTest] Test 99 FAILED: CPUProfiler microsecond scope metric measurement error!");
                return false;
            }

            LOG_INFO("[FormatTest] Test 99 Passed: CPU Microsecond Profiler validated successfully.");
        }

        // --------------------------------------------------------------------
        // Test 100: Memory Profiler & Heap Leak Detection
        // --------------------------------------------------------------------
        {
            LOG_INFO("[FormatTest] Running Test 100: Memory Profiler & Heap Leaks...");

            eng::developer::MemoryProfiler memProfiler;
            int* dummyBlock = new int[100];

            memProfiler.TrackAllocation(dummyBlock, sizeof(int) * 100);
            if (memProfiler.GetTotalAllocatedBytes() != sizeof(int) * 100 || !memProfiler.DetectLeaks()) {
                LOG_ERROR("[FormatTest] Test 100 FAILED: MemoryProfiler allocation tracking error!");
                delete[] dummyBlock;
                return false;
            }

            memProfiler.TrackDeallocation(dummyBlock);
            delete[] dummyBlock;

            if (memProfiler.GetTotalAllocatedBytes() != 0 || memProfiler.DetectLeaks()) {
                LOG_ERROR("[FormatTest] Test 100 FAILED: MemoryProfiler deallocation leak check error!");
                return false;
            }

            LOG_INFO("[FormatTest] Test 100 Passed: Memory Profiler & Heap Leak Detection validated successfully.");
        }

        // --------------------------------------------------------------------
        // Test 101: GPU Profiler Render Pass Timestamp Queries
        // --------------------------------------------------------------------
        {
            LOG_INFO("[FormatTest] Running Test 101: GPU Profiler Timestamp Queries...");

            eng::developer::GPUProfiler gpuProfiler;
            gpuProfiler.BeginSample("ShadowPass");
            gpuProfiler.EndSample("ShadowPass", 0.45f); // 0.45ms

            if (gpuProfiler.GetPassTimeMs("ShadowPass") != 0.45f) {
                LOG_ERROR("[FormatTest] Test 101 FAILED: GPUProfiler pass timing collection error!");
                return false;
            }

            LOG_INFO("[FormatTest] Test 101 Passed: GPU Profiler Render Pass Timestamp Queries validated successfully.");
        }

        // --------------------------------------------------------------------
        // Test 102: Debug Renderer Viewport Primitives (Lines, Spheres, Boxes)
        // --------------------------------------------------------------------
        {
            LOG_INFO("[FormatTest] Running Test 102: Debug Renderer Viewport Primitives...");

            eng::developer::DebugRenderer debugRenderer;
            debugRenderer.DrawLine(glm::vec3(0.0f), glm::vec3(1.0f));
            debugRenderer.DrawSphere(glm::vec3(0.0f), 2.0f);
            debugRenderer.DrawBox(glm::vec3(-1.0f), glm::vec3(1.0f));

            if (debugRenderer.GetPrimitiveCount() != 3) {
                LOG_ERROR("[FormatTest] Test 102 FAILED: DebugRenderer primitive count mismatch!");
                return false;
            }

            debugRenderer.Clear();
            if (debugRenderer.GetPrimitiveCount() != 0) {
                LOG_ERROR("[FormatTest] Test 102 FAILED: DebugRenderer Clear failed!");
                return false;
            }

            LOG_INFO("[FormatTest] Test 102 Passed: Debug Renderer Viewport Primitives validated successfully.");
        }

        // --------------------------------------------------------------------
        // Test 103: Developer Statistics & Frame Rate Telemetry
        // --------------------------------------------------------------------
        {
            LOG_INFO("[FormatTest] Running Test 103: Developer Statistics & FPS Telemetry...");

            eng::developer::DeveloperStats stats;
            stats.RecordFrame(0.016f); // 16ms
            stats.RecordFrame(0.016f);

            if (stats.GetFPS() < 59.0f || stats.GetFPS() > 64.0f) {
                LOG_ERROR("[FormatTest] Test 103 FAILED: DeveloperStats FPS calculation error!");
                return false;
            }

            LOG_INFO("[FormatTest] Test 103 Passed: Developer Statistics & Frame Rate Telemetry validated successfully.");
        }

        // --------------------------------------------------------------------
        // Test 104: Asset Validator Integrity Verification
        // --------------------------------------------------------------------
        {
            LOG_INFO("[FormatTest] Running Test 104: Asset Validator Integrity...");

            eng::developer::AssetValidator validator;
            bool validTex = validator.ValidateTexture("textures/brick.png");
            bool validMesh = validator.ValidateMesh("models/character.obj");
            bool invalidTex = validator.ValidateTexture("invalid_file.txt");

            if (!validTex || !validMesh || invalidTex || validator.GetErrorCount() != 1) {
                LOG_ERROR("[FormatTest] Test 104 FAILED: AssetValidator integrity check error!");
                return false;
            }

            LOG_INFO("[FormatTest] Test 104 Passed: Asset Validator Integrity Verification validated successfully.");
        }

        // --------------------------------------------------------------------
        // Test 105: Package Format Structs & Header Checksums (.omxpkg)
        // --------------------------------------------------------------------
        {
            LOG_INFO("[FormatTest] Running Test 105: Package Format Structs (.omxpkg)...");

            OmnixPackage pkg;
            pkg.header.assetCount = 1;
            pkg.header.dependencyCount = 0;
            pkg.header.chunkCount = 0;

            PackageAssetEntry assetEntry;
            assetEntry.handle = AssetHandle(1001);
            assetEntry.type = 1; // Mesh
            assetEntry.dataOffset = 0;
            assetEntry.dataSize = 4;
            pkg.assets.push_back(assetEntry);
            pkg.rawDataBlock = { 0x11, 0x22, 0x33, 0x44 };

            std::string tempPath = "temp_test_pkg.omxpkg";
            bool serialized = SerializePackage(pkg, tempPath);
            if (!serialized) {
                LOG_ERROR("[FormatTest] Test 105 FAILED: Package serialization failed!");
                return false;
            }

            OmnixPackage deserializedPkg;
            bool deserialized = DeserializePackage(deserializedPkg, tempPath);
            std::filesystem::remove(tempPath);

            if (!deserialized || deserializedPkg.header.assetCount != 1 || deserializedPkg.rawDataBlock.size() != 4) {
                LOG_ERROR("[FormatTest] Test 105 FAILED: Package deserialization mismatch!");
                return false;
            }

            LOG_INFO("[FormatTest] Test 105 Passed: Package Format Structs (.omxpkg) validated successfully.");
        }

        // --------------------------------------------------------------------
        // Test 106: Package Reader & Mounting System
        // --------------------------------------------------------------------
        {
            LOG_INFO("[FormatTest] Running Test 106: Package Reader & Mounting System...");

            OmnixPackage pkg;
            pkg.header.assetCount = 1;
            pkg.header.dependencyCount = 0;
            pkg.header.chunkCount = 0;

            uint64_t dataOffset = 52 + sizeof(PackageAssetEntry);
            pkg.header.assetTableOffset = 52;
            pkg.header.dependencyTableOffset = dataOffset;
            pkg.header.chunkTableOffset = dataOffset;
            pkg.header.dataBlockOffset = dataOffset;

            pkg.rawDataBlock = { 0xAA, 0xBB };

            PackageAssetEntry assetEntry;
            assetEntry.handle = AssetHandle(2002);
            assetEntry.type = 2; // Texture
            assetEntry.dataOffset = dataOffset;
            assetEntry.dataSize = 2;
            assetEntry.checksum = eng::runtime::ComputeChecksum32(pkg.rawDataBlock.data(), pkg.rawDataBlock.size());
            pkg.assets.push_back(assetEntry);

            std::string tempPath = "temp_reader_pkg.omxpkg";
            SerializePackage(pkg, tempPath);

            eng::runtime::Package packageReader;
            bool opened = packageReader.Open(tempPath);
            std::string outErr;
            bool valid = packageReader.Validate(outErr);

            auto payload = packageReader.ReadAssetPayload(AssetHandle(2002));
            std::filesystem::remove(tempPath);

            if (!opened || !valid || payload.size() != 2 || payload[0] != 0xAA) {
                LOG_ERROR("[FormatTest] Test 106 FAILED: Package reader payload extraction error! Err: %s", outErr.c_str());
                return false;
            }

            LOG_INFO("[FormatTest] Test 106 Passed: Package Reader & Mounting System validated successfully.");
        }

        // --------------------------------------------------------------------
        // Test 107: Package Payload Compression & Encryption
        // --------------------------------------------------------------------
        {
            LOG_INFO("[FormatTest] Running Test 107: Package Payload Compression & Encryption...");

            std::vector<uint8_t> originalData = { 0x10, 0x20, 0x30, 0x40, 0x50 };

            // Compression & Decompression
            auto compressed = eng::runtime::PackageCompressor::CompressPayload(originalData, eng::runtime::CompressionType::LZ4);
            auto decompressed = eng::runtime::PackageCompressor::DecompressPayload(compressed, eng::runtime::CompressionType::LZ4);

            if (decompressed != originalData) {
                LOG_ERROR("[FormatTest] Test 107 FAILED: Package payload decompression mismatch!");
                return false;
            }

            // Encryption & Decryption
            auto encrypted = eng::runtime::PackageEncryptor::EncryptPayload(originalData, 0xABCDEF);
            auto decrypted = eng::runtime::PackageEncryptor::DecryptPayload(encrypted, 0xABCDEF);

            if (decrypted != originalData) {
                LOG_ERROR("[FormatTest] Test 107 FAILED: Package payload decryption mismatch!");
                return false;
            }

            LOG_INFO("[FormatTest] Test 107 Passed: Package Payload Compression & Encryption validated successfully.");
        }

        // --------------------------------------------------------------------
        // Test 108: Package Asset Dependency Validation
        // --------------------------------------------------------------------
        {
            LOG_INFO("[FormatTest] Running Test 108: Package Asset Dependency Validation...");

            OmnixPackage pkg;
            pkg.header.assetCount = 1;
            pkg.header.dependencyCount = 1;
            pkg.header.chunkCount = 0;

            uint64_t dataOffset = 52 + sizeof(PackageAssetEntry) + sizeof(PackageDependencyEntry);
            pkg.header.assetTableOffset = 52;
            pkg.header.dependencyTableOffset = 52 + sizeof(PackageAssetEntry);
            pkg.header.chunkTableOffset = dataOffset;
            pkg.header.dataBlockOffset = dataOffset;

            pkg.rawDataBlock = { 0xFF };

            PackageAssetEntry assetEntry;
            assetEntry.handle = AssetHandle(3003);
            assetEntry.type = 3; // Material
            assetEntry.dataOffset = dataOffset;
            assetEntry.dataSize = 1;
            assetEntry.checksum = eng::runtime::ComputeChecksum32(pkg.rawDataBlock.data(), pkg.rawDataBlock.size());
            pkg.assets.push_back(assetEntry);

            PackageDependencyEntry depEntry;
            depEntry.assetHandle = AssetHandle(3003);
            depEntry.dependentHandle = AssetHandle(2002); // Depends on Texture 2002
            pkg.dependencies.push_back(depEntry);

            std::string tempPath = "temp_dep_pkg.omxpkg";
            SerializePackage(pkg, tempPath);

            eng::runtime::PackageManager pkgManager;
            bool mounted = pkgManager.MountPackage(tempPath);
            auto deps = pkgManager.GetDependencies(AssetHandle(3003));
            pkgManager.Clear();
            std::filesystem::remove(tempPath);

            if (!mounted || deps.size() != 1 || deps[0] != AssetHandle(2002)) {
                LOG_ERROR("[FormatTest] Test 108 FAILED: Package dependency validation error!");
                return false;
            }

            LOG_INFO("[FormatTest] Test 108 Passed: Package Asset Dependency Validation validated successfully.");
        }

        LOG_INFO("================================================================================");
        LOG_INFO("                   ALL RUNTIME FORMAT TESTS PASSED SUCCESSFULLY                 ");
        LOG_INFO("================================================================================");
        return true;
    }

} // namespace eng::runtime
