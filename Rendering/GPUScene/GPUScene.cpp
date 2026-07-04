#include "Core/pch.h"
#include "GPUScene.h"
#include "Core/Engine/Log.h"
#include "Core/Vulkan/VkUtils.h"
#include "Core/Engine/VmaHelpers.h"
#include "Rendering/Visibility/Frustum.h"
#include <cstring>
#include <algorithm>
#include <unordered_set>
#include "Rendering/Geometry/Assets/RVGRegistry.h"
#include "Rendering/Geometry/Streaming/RVGPageStreamingManager.h"

namespace eng::renderer {

static glm::vec3 TransformPoint(const glm::mat4& m, const glm::vec3& p)
{
    return glm::vec3(m * glm::vec4(p, 1.0f));
}

static float GetMaxScaleFromMatrix(const glm::mat4& m)
{
    float sx = glm::length(glm::vec3(m[0]));
    float sy = glm::length(glm::vec3(m[1]));
    float sz = glm::length(glm::vec3(m[2]));
    return glm::max(sx, glm::max(sy, sz));
}

static glm::vec4 ComputeWorldBoundsSphere(
    const glm::mat4& world,
    const MeshBounds& bounds)
{
    glm::vec3 worldCenter = TransformPoint(world, bounds.localCenter);
    float maxScale = GetMaxScaleFromMatrix(world);
    float worldRadius = bounds.localRadius * maxScale;
    return glm::vec4(worldCenter, glm::max(worldRadius, 0.0001f));
}


GPUScene::GPUScene() {
    m_UploadBytesThisFrame = 0;
    m_StaleHandleErrors = 0;
    m_GrowthEvents = 0;
}

GPUScene::~GPUScene() {
}

void GPUScene::Initialize(EngineResources& resources) {
    std::lock_guard<std::mutex> lock(m_SceneMutex);

    m_InstanceSlots.clear();
    m_FreeInstanceSlots.clear();
    m_EntityToInstance.clear();
    m_MaterialOverrides.clear();
    m_MeshSlots.clear();
    m_MeshToIndex.clear();
    m_GPUInstances.clear();
    m_GPUMeshDrawData.clear();

    m_UploadBytesThisFrame = 0;
    m_StaleHandleErrors = 0;
    m_GrowthEvents = 0;

    createDescriptorSetLayout(resources);

    m_Frames.resize(EngineResources::MAX_FRAMES_IN_FLIGHT);
    for (uint32_t i = 0; i < EngineResources::MAX_FRAMES_IN_FLIGHT; ++i) {
        createFrameResources(resources, m_Frames[i]);
    }

    LOG_INFO("[GPUScene] Initialized persistent scene instances manager.");
}

void GPUScene::Shutdown(EngineResources& resources) {
    std::lock_guard<std::mutex> lock(m_SceneMutex);

    for (uint32_t i = 0; i < m_Frames.size(); ++i) {
        destroyFrameResources(resources, m_Frames[i]);
    }
    m_Frames.clear();

    if (m_DescriptorSetLayout != VK_NULL_HANDLE) {
        vkDestroyDescriptorSetLayout(resources.device, m_DescriptorSetLayout, nullptr);
        m_DescriptorSetLayout = VK_NULL_HANDLE;
    }
    if (m_LocalLightsDescriptorSetLayout != VK_NULL_HANDLE) {
        vkDestroyDescriptorSetLayout(resources.device, m_LocalLightsDescriptorSetLayout, nullptr);
        m_LocalLightsDescriptorSetLayout = VK_NULL_HANDLE;
    }
    if (m_LightCullingDescriptorSetLayout != VK_NULL_HANDLE) {
        vkDestroyDescriptorSetLayout(resources.device, m_LightCullingDescriptorSetLayout, nullptr);
        m_LightCullingDescriptorSetLayout = VK_NULL_HANDLE;
    }

    m_InstanceSlots.clear();
    m_FreeInstanceSlots.clear();
    m_EntityToInstance.clear();
    m_MaterialOverrides.clear();
    m_MeshSlots.clear();
    m_MeshToIndex.clear();
    LOG_INFO("[GPUScene] Shutdown complete.");
}

GPUSceneInstanceHandle GPUScene::CreateInstance(const GPUGeometryInstance& initialData) {
    std::lock_guard<std::mutex> lock(m_SceneMutex);

    uint32_t index = 0;
    uint32_t gen = 0;

    if (!m_FreeInstanceSlots.empty()) {
        index = m_FreeInstanceSlots.back();
        m_FreeInstanceSlots.pop_back();
        gen = m_InstanceSlots[index].generation;
        m_InstanceSlots[index].instance = initialData;
        m_InstanceSlots[index].active = true;
        m_InstanceSlots[index].dirtyFrames = EngineResources::MAX_FRAMES_IN_FLIGHT;
        m_InstanceSlots[index].overrideOffset = 0xFFFFFFFF;
        m_InstanceSlots[index].overrideCount = 0;
    } else {
        index = static_cast<uint32_t>(m_InstanceSlots.size());
        InstanceSlot slot{};
        slot.instance = initialData;
        slot.generation = 0;
        slot.active = true;
        slot.dirtyFrames = EngineResources::MAX_FRAMES_IN_FLIGHT;
        slot.overrideOffset = 0xFFFFFFFF;
        slot.overrideCount = 0;
        m_InstanceSlots.push_back(slot);
        gen = 0;
    }

    if (initialData.objectID != 0) {
        m_InstanceSlots[index].entityID = initialData.objectID;
        m_EntityToInstance[initialData.objectID] = GPUSceneInstanceHandle(index, gen);
    }

    return GPUSceneInstanceHandle(index, gen);
}

void GPUScene::UpdateInstance(GPUSceneInstanceHandle handle, const GPUGeometryInstance& data) {
    std::lock_guard<std::mutex> lock(m_SceneMutex);

    if (handle.index >= m_InstanceSlots.size()) {
        m_StaleHandleErrors++;
        return;
    }

    auto& slot = m_InstanceSlots[handle.index];
    if (!slot.active || slot.generation != handle.generation) {
        m_StaleHandleErrors++;
        return;
    }

    // Preserve previous transform
    slot.instance.previousModel = slot.instance.model;
    slot.instance.model = data.model;
    slot.instance.worldBoundsSphere = data.worldBoundsSphere;
    slot.instance.geometryID = data.geometryID;
    slot.instance.objectID = data.objectID;
    
    // Merge overrides offset if overrides exist
    if (slot.overrideOffset != 0xFFFFFFFF) {
        slot.instance.materialTableOffset = slot.overrideOffset;
    } else {
        slot.instance.materialTableOffset = data.materialTableOffset;
    }

    slot.instance.flags = data.flags;

    slot.dirtyFrames = EngineResources::MAX_FRAMES_IN_FLIGHT;
}

void GPUScene::DestroyInstance(GPUSceneInstanceHandle handle) {
    std::lock_guard<std::mutex> lock(m_SceneMutex);

    if (handle.index >= m_InstanceSlots.size()) return;

    auto& slot = m_InstanceSlots[handle.index];
    if (!slot.active || slot.generation != handle.generation) return;

    slot.active = false;
    slot.instance = GPUGeometryInstance{}; // Zero it out
    slot.instance.flags = 0; // Clear flags (invisible)
    slot.dirtyFrames = EngineResources::MAX_FRAMES_IN_FLIGHT;
    slot.generation++;

    if (slot.entityID != 0) {
        m_EntityToInstance.erase(slot.entityID);
        slot.entityID = 0;
    }

    m_FreeInstanceSlots.push_back(handle.index);
}

bool GPUScene::IsInstanceValid(GPUSceneInstanceHandle handle) const {
    std::lock_guard<std::mutex> lock(m_SceneMutex);
    if (handle.index >= m_InstanceSlots.size()) return false;
    const auto& slot = m_InstanceSlots[handle.index];
    return slot.active && slot.generation == handle.generation;
}

void GPUScene::RegisterEntityInstance(uint32_t entityID, GPUSceneInstanceHandle handle) {
    std::lock_guard<std::mutex> lock(m_SceneMutex);
    m_EntityToInstance[entityID] = handle;
    if (handle.index < m_InstanceSlots.size()) {
        m_InstanceSlots[handle.index].entityID = entityID;
    }
}

GPUSceneInstanceHandle GPUScene::GetEntityInstance(uint32_t entityID) const {
    std::lock_guard<std::mutex> lock(m_SceneMutex);
    auto it = m_EntityToInstance.find(entityID);
    if (it != m_EntityToInstance.end()) {
        return it->second;
    }
    return GPUSceneInstanceHandle();
}

void GPUScene::UnregisterEntityInstance(uint32_t entityID) {
    std::lock_guard<std::mutex> lock(m_SceneMutex);
    m_EntityToInstance.erase(entityID);
}

void GPUScene::SetInstanceMaterialOverrides(GPUSceneInstanceHandle handle, const std::vector<uint32_t>& materialIDs) {
    std::lock_guard<std::mutex> lock(m_SceneMutex);

    if (handle.index >= m_InstanceSlots.size()) return;
    auto& slot = m_InstanceSlots[handle.index];
    if (!slot.active || slot.generation != handle.generation) return;

    // Append to materials override vector
    slot.overrideOffset = static_cast<uint32_t>(m_MaterialOverrides.size());
    slot.overrideCount = static_cast<uint32_t>(materialIDs.size());
    for (uint32_t id : materialIDs) {
        m_MaterialOverrides.push_back(id);
    }

    slot.instance.materialTableOffset = slot.overrideOffset;
    slot.dirtyFrames = EngineResources::MAX_FRAMES_IN_FLIGHT;
}

void GPUScene::ClearInstanceMaterialOverrides(GPUSceneInstanceHandle handle) {
    std::lock_guard<std::mutex> lock(m_SceneMutex);

    if (handle.index >= m_InstanceSlots.size()) return;
    auto& slot = m_InstanceSlots[handle.index];
    if (!slot.active || slot.generation != handle.generation) return;

    slot.overrideOffset = 0xFFFFFFFF;
    slot.overrideCount = 0;
    slot.instance.materialTableOffset = 0xFFFFFFFF;
    slot.dirtyFrames = EngineResources::MAX_FRAMES_IN_FLIGHT;
}

GPUSceneDiagnostics GPUScene::GetDiagnostics() const {
    std::lock_guard<std::mutex> lock(m_SceneMutex);

    GPUSceneDiagnostics diag{};
    diag.instanceCount = static_cast<uint32_t>(m_InstanceSlots.size());
    diag.activeSlots = static_cast<uint32_t>(m_InstanceSlots.size() - m_FreeInstanceSlots.size());
    diag.freeSlots = static_cast<uint32_t>(m_FreeInstanceSlots.size());
    diag.gpuMeshRecordCount = static_cast<uint32_t>(m_MeshSlots.size());
    diag.materialOverrideCount = static_cast<uint32_t>(m_MaterialOverrides.size());
    diag.uploadBytesThisFrame = m_UploadBytesThisFrame;
    diag.staleHandleErrors = m_StaleHandleErrors;

    return diag;
}

void GPUScene::UpdateFrame(
    EngineResources& resources,
    uint32_t frameIndex,
    const RenderScene& renderScene,
    const std::vector<RenderItem>& renderQueueItems,
    const std::unordered_map<uint64_t, Material*>& ecsMaterialCache,
    const Material* defaultMaterial,
    const Omnix::Radiance::RadianceFrameUBO& radianceUBO,
    uint32_t shadingMode,
    const ::Scene* activeScene
) {
    GPUSceneFrameResources& frameRes = m_Frames[frameIndex];
    bool needsDescriptorUpdate = false;

    // -------------------------------------------------------------------------
    // 1. Camera Uniform Buffer Upload
    // -------------------------------------------------------------------------
    void* cameraDst = nullptr;
    VK_CHECK(vmaMapMemory(resources.allocator, frameRes.cameraAlloc, &cameraDst));
    std::memcpy(cameraDst, &radianceUBO, sizeof(radianceUBO));
    vmaUnmapMemory(resources.allocator, frameRes.cameraAlloc);

    // -------------------------------------------------------------------------
    // 1b. Frustum Uniform Buffer Upload
    // -------------------------------------------------------------------------
    float aspect = renderScene.camera.aspectRatio;
    if (aspect <= 0.001f) {
        if (resources.swapChainExtent.height > 0) {
            aspect = static_cast<float>(resources.swapChainExtent.width) / static_cast<float>(resources.swapChainExtent.height);
        } else {
            aspect = 1.777f;
        }
    }
    glm::mat4 proj = glm::perspective(glm::radians(renderScene.camera.fov), aspect, renderScene.camera.nearPlane, renderScene.camera.farPlane);
    glm::mat4 view = renderScene.camera.viewMatrix;
    glm::mat4 vp = proj * view;
    GPUFrustum frustum = ExtractFrustumPlanes(vp);

    void* frustumDst = nullptr;
    VK_CHECK(vmaMapMemory(resources.allocator, frameRes.frustumAlloc, &frustumDst));
    std::memcpy(frustumDst, &frustum, sizeof(GPUFrustum));
    vmaUnmapMemory(resources.allocator, frameRes.frustumAlloc);

    // -------------------------------------------------------------------------
    // 2. Light Storage Buffer Upload
    // -------------------------------------------------------------------------
    LightData lightUboData{};
    if (!renderScene.directionalLights.empty()) {
        const auto& dirLight = renderScene.directionalLights[0];
        lightUboData.directionalDirectionIntensity = glm::vec4(dirLight.direction, dirLight.intensity);
        lightUboData.directionalColor = glm::vec4(dirLight.color, 1.0f);
        lightUboData.directionalLightProjView = dirLight.lightSpaceMatrix;
        lightUboData.shadowBias = dirLight.shadowBias;
        lightUboData.shadowNormalBias = dirLight.shadowNormalBias;
        lightUboData.shadowSlopeBias = dirLight.shadowSlopeBias;
        lightUboData.shadowStrength = dirLight.shadowStrength;
        lightUboData.shadowLightCast = dirLight.castShadows > 0.0f ? 1 : 0;
        lightUboData.pcfKernelSize = dirLight.pcfKernelSize;
        lightUboData.shadowResolution = dirLight.shadowResolution;
        lightUboData.shadowSettings.shadowParams = glm::vec4(1.0f, 0.003f, 0.01f, 1.0f);
        lightUboData.shadowSettings.shadowFlags = glm::uvec4(3, 0, 0, 0);
    }
    lightUboData.ambientColorIntensity = glm::vec4(renderScene.skyLight.color, renderScene.skyLight.intensity);
    lightUboData.pointLightCount = std::min(static_cast<uint32_t>(renderScene.pointLights.size()), 16u);
    for (uint32_t i = 0; i < lightUboData.pointLightCount; ++i) {
        const auto& pt = renderScene.pointLights[i];
        lightUboData.pointPositionsRadius[i] = glm::vec4(pt.position, pt.radius);
        lightUboData.pointColorsIntensity[i] = glm::vec4(pt.color, pt.intensity);
    }
    lightUboData.spotLightCount = std::min(static_cast<uint32_t>(renderScene.spotLights.size()), 16u);
    for (uint32_t i = 0; i < lightUboData.spotLightCount; ++i) {
        const auto& sl = renderScene.spotLights[i];
        lightUboData.spotPositionsRange[i] = glm::vec4(sl.position, sl.range);
        lightUboData.spotDirectionsIntensity[i] = glm::vec4(sl.direction, sl.intensity);
        lightUboData.spotColors[i] = glm::vec4(sl.color, std::cos(glm::radians(sl.innerConeAngle)));
        lightUboData.spotAngles[i] = glm::vec4(std::cos(glm::radians(sl.outerConeAngle)), 0.0f, 0.0f, 0.0f);
    }
    lightUboData.shadingMode = shadingMode;

    void* lightDst = nullptr;
    VK_CHECK(vmaMapMemory(resources.allocator, frameRes.lightAlloc, &lightDst));
    std::memcpy(lightDst, &lightUboData, sizeof(lightUboData));
    vmaUnmapMemory(resources.allocator, frameRes.lightAlloc);

    // -------------------------------------------------------------------------
    // 3. Populate Unique Materials & Build Material Array
    // -------------------------------------------------------------------------
    std::unordered_map<const Material*, uint32_t> materialToIndex;
    std::vector<MaterialGPUData> materialsData;

    MaterialGPUData defaultMatData{};
    if (defaultMaterial) {
        defaultMatData = defaultMaterial->uboData;
    }
    materialsData.push_back(defaultMatData);
    if (defaultMaterial) {
        materialToIndex[defaultMaterial] = 0;
    }

    for (const auto& item : renderQueueItems) {
        const Material* mat = item.material;
        if (mat) {
            if (item.mesh) {
                const_cast<Material*>(mat)->updateNormalMapCompatibility(*item.mesh);
            }
            if (materialToIndex.find(mat) == materialToIndex.end()) {
                MaterialGPUData matData = mat->uboData;
                materialToIndex[mat] = static_cast<uint32_t>(materialsData.size());
                materialsData.push_back(matData);
            }
        }
    }

    uint32_t materialCount = static_cast<uint32_t>(materialsData.size());
    uint32_t prevMaterialCapacity = frameRes.materialCapacity;
    resizeBufferIfNeeded(
        resources,
        frameRes.materialBuffer,
        frameRes.materialAlloc,
        frameRes.materialBufferSize,
        frameRes.materialCapacity,
        materialCount,
        sizeof(MaterialGPUData),
        VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT
    );
    if (frameRes.materialCapacity != prevMaterialCapacity) {
        needsDescriptorUpdate = true;
    }

    VkDeviceSize matDataSize = materialCount * sizeof(MaterialGPUData);
    resources.ensureStagingBuffer(matDataSize);
    void* stagingDst = nullptr;
    VK_CHECK(vmaMapMemory(resources.allocator, resources.transfer.stagingAlloc, &stagingDst));
    std::memcpy(stagingDst, materialsData.data(), matDataSize);
    vmaUnmapMemory(resources.allocator, resources.transfer.stagingAlloc);

    VkCommandBuffer copyCmd = resources.beginSingleTimeCommands();
    resources.copyStagingToDevice(copyCmd, frameRes.materialBuffer, 0, matDataSize);
    resources.endSingleTimeCommands(copyCmd);

    // -------------------------------------------------------------------------
    // 4. Synchronize CPU RenderQueue to Persistent Instance/Mesh Slots (Self-Managing System)
    // -------------------------------------------------------------------------
    std::lock_guard<std::mutex> lock(m_SceneMutex);

    m_UploadBytesThisFrame = 0;

    // Scan unique meshes
    std::vector<const Mesh*> uniqueMeshes;
    m_MeshToIndex.clear();
    for (const auto& item : renderQueueItems) {
        if (item.mesh && m_MeshToIndex.find(item.mesh) == m_MeshToIndex.end()) {
            m_MeshToIndex[item.mesh] = static_cast<uint32_t>(uniqueMeshes.size());
            uniqueMeshes.push_back(item.mesh);
        }
    }

    // Populate CPU Mesh record table
    m_MeshSlots.clear();
    for (const Mesh* mesh : uniqueMeshes) {
        MeshSlot slot{};
        slot.meshPtr = mesh;
        slot.record.firstIndex = mesh->firstIndex;
        slot.record.vertexOffset = mesh->vertexOffset;
        slot.record.indexCount = mesh->indexCount;
        slot.record.vertexCount = static_cast<uint32_t>(mesh->vertexSize / sizeof(PbrVertex));
        slot.record.localBoundsSphere = glm::vec4(mesh->bounds.localCenter, mesh->bounds.localRadius);
        slot.record.materialSlotOffset = mesh->materialSlotOffset;
        slot.record.submeshOffset = 0;
        slot.record.submeshCount = 1;
        slot.record.flags = 1; // Active
        slot.active = true;
        m_MeshSlots.push_back(slot);
    }

    // Synchronize instances from CPU renderQueueItems
    std::unordered_set<uint32_t> entityIDsThisFrame;
    for (const auto& item : renderQueueItems) {
        if (!item.mesh) continue;

        uint32_t entityID = item.entityID;
        entityIDsThisFrame.insert(entityID);

        uint32_t meshIdx = m_MeshToIndex[item.mesh];
        uint32_t matIdx = 0;
        if (item.material) {
            auto it = materialToIndex.find(item.material);
            if (it != materialToIndex.end()) {
                matIdx = it->second;
            }
        }

        // Build current data
        GPUGeometryInstance data{};
        data.model = item.transform;
        data.previousModel = item.previousTransform;
        data.worldBoundsSphere = ComputeWorldBoundsSphere(item.transform, item.mesh->bounds);
        data.geometryID = item.mesh->isVirtualGeometry ? item.mesh->rvgAssetIndex : meshIdx;
        data.materialTableOffset = matIdx;
        data.objectID = entityID;
        data.flags = GPUInstanceFlags_Visible | GPUInstanceFlags_CastShadow;
        if (item.mesh->isVirtualGeometry) {
            data.flags |= GPUInstanceFlags_VirtualGeometry;
        }

        // Negative scale check
        float det = glm::determinant(glm::mat3(item.transform));
        if (det < 0.0f) {
            data.flags |= GPUInstanceFlags_NegativeScale;
        }

        // Check if handle already exists for this entityID
        auto it = m_EntityToInstance.find(entityID);
        if (it != m_EntityToInstance.end()) {
            GPUSceneInstanceHandle handle = it->second;
            auto& slot = m_InstanceSlots[handle.index];
            
            // Check if any fields changed (model, material, mesh, or bounds)
            if (slot.instance.model != data.model ||
                slot.instance.geometryID != data.geometryID ||
                slot.instance.materialTableOffset != matIdx) 
            {
                slot.instance.previousModel = slot.instance.model;
                slot.instance.model = data.model;
                slot.instance.worldBoundsSphere = data.worldBoundsSphere;
                slot.instance.geometryID = data.geometryID;
                slot.instance.objectID = data.objectID;
                slot.instance.materialTableOffset = (slot.overrideOffset != 0xFFFFFFFF) ? slot.overrideOffset : data.materialTableOffset;
                slot.instance.flags = data.flags;
                slot.dirtyFrames = EngineResources::MAX_FRAMES_IN_FLIGHT;
            }
        } else {
            // New instance slot needed
            uint32_t index = 0;
            uint32_t generation = 0;
            if (!m_FreeInstanceSlots.empty()) {
                index = m_FreeInstanceSlots.back();
                m_FreeInstanceSlots.pop_back();
                generation = m_InstanceSlots[index].generation;
                m_InstanceSlots[index].instance = data;
                m_InstanceSlots[index].active = true;
                m_InstanceSlots[index].dirtyFrames = EngineResources::MAX_FRAMES_IN_FLIGHT;
                m_InstanceSlots[index].overrideOffset = 0xFFFFFFFF;
                m_InstanceSlots[index].overrideCount = 0;
            } else {
                index = static_cast<uint32_t>(m_InstanceSlots.size());
                InstanceSlot newSlot{};
                newSlot.instance = data;
                newSlot.generation = 0;
                newSlot.active = true;
                newSlot.dirtyFrames = EngineResources::MAX_FRAMES_IN_FLIGHT;
                newSlot.overrideOffset = 0xFFFFFFFF;
                newSlot.overrideCount = 0;
                m_InstanceSlots.push_back(newSlot);
            }
            if (entityID != 0) {
                m_InstanceSlots[index].entityID = entityID;
                m_EntityToInstance[entityID] = GPUSceneInstanceHandle(index, generation);
            }
        }
    }

    // Deactivate / Destroy instances that disappeared
    for (size_t i = 0; i < m_InstanceSlots.size(); ++i) {
        auto& slot = m_InstanceSlots[i];
        if (slot.active && slot.entityID != 0) {
            if (entityIDsThisFrame.find(slot.entityID) == entityIDsThisFrame.end()) {
                const uint32_t removedEntityID = slot.entityID;
                slot.active = false;
                slot.instance = GPUGeometryInstance{};
                slot.instance.flags = 0;
                slot.dirtyFrames = EngineResources::MAX_FRAMES_IN_FLIGHT;
                slot.generation++;
                slot.entityID = 0;
                m_EntityToInstance.erase(removedEntityID);
                m_FreeInstanceSlots.push_back(static_cast<uint32_t>(i));
            }
        }
    }

    // Default 1 dummy slot if no active slots exist to prevent empty buffers
    if (m_InstanceSlots.empty()) {
        InstanceSlot slot{};
        slot.instance = GPUGeometryInstance{};
        slot.active = true;
        slot.dirtyFrames = EngineResources::MAX_FRAMES_IN_FLIGHT;
        m_InstanceSlots.push_back(slot);
    }

    uint32_t instanceCount = static_cast<uint32_t>(m_InstanceSlots.size());

    // -------------------------------------------------------------------------
    // 5. Upload Instance & ObjectID Buffer Changes (Dirty Updates Range Consolidation)
    // -------------------------------------------------------------------------
    uint32_t prevInstanceCapacity = frameRes.instanceCapacity;
    resizeBufferIfNeeded(
        resources,
        frameRes.instanceBuffer,
        frameRes.instanceAlloc,
        frameRes.instanceBufferSize,
        frameRes.instanceCapacity,
        instanceCount,
        sizeof(GPUGeometryInstance),
        VK_BUFFER_USAGE_STORAGE_BUFFER_BIT
    );
    if (frameRes.instanceCapacity != prevInstanceCapacity) {
        needsDescriptorUpdate = true;
        m_GrowthEvents++;
        // On growth, force mark ALL slots dirty so the new buffer is fully written
        for (auto& slot : m_InstanceSlots) {
            slot.dirtyFrames = EngineResources::MAX_FRAMES_IN_FLIGHT;
        }
    }

    uint32_t prevObjectIdCapacity = frameRes.objectIdCapacity;
    resizeBufferIfNeeded(
        resources,
        frameRes.objectIdBuffer,
        frameRes.objectIdAlloc,
        frameRes.objectIdBufferSize,
        frameRes.objectIdCapacity,
        instanceCount,
        sizeof(uint32_t),
        VK_BUFFER_USAGE_STORAGE_BUFFER_BIT
    );
    if (frameRes.objectIdCapacity != prevObjectIdCapacity) {
        needsDescriptorUpdate = true;
    }

    // Gather dirty slots and combine contiguous ranges
    std::vector<uint32_t> dirtyIndices;
    dirtyIndices.reserve(m_InstanceSlots.size());
    for (uint32_t i = 0; i < m_InstanceSlots.size(); ++i) {
        if (m_InstanceSlots[i].dirtyFrames > 0) {
            dirtyIndices.push_back(i);
        }
    }

    if (!dirtyIndices.empty()) {
        std::sort(dirtyIndices.begin(), dirtyIndices.end());
        
        // Group contiguous indices
        struct Range {
            uint32_t start;
            uint32_t count;
        };
        std::vector<Range> dirtyRanges;
        dirtyRanges.push_back({dirtyIndices[0], 1});

        for (size_t i = 1; i < dirtyIndices.size(); ++i) {
            uint32_t idx = dirtyIndices[i];
            auto& lastRange = dirtyRanges.back();
            if (idx == lastRange.start + lastRange.count) {
                lastRange.count++;
            } else {
                dirtyRanges.push_back({idx, 1});
            }
        }

        // Map and copy dirty ranges
        void* instanceDst = nullptr;
        VK_CHECK(vmaMapMemory(resources.allocator, frameRes.instanceAlloc, &instanceDst));
        void* objectIdDst = nullptr;
        VK_CHECK(vmaMapMemory(resources.allocator, frameRes.objectIdAlloc, &objectIdDst));

        char* instBytes = static_cast<char*>(instanceDst);
        char* objBytes = static_cast<char*>(objectIdDst);

        for (const auto& range : dirtyRanges) {
            VkDeviceSize instOffset = range.start * sizeof(GPUGeometryInstance);
            VkDeviceSize instSize = range.count * sizeof(GPUGeometryInstance);
            
            // Build direct array of instances in range
            std::vector<GPUGeometryInstance> instTemp;
            std::vector<uint32_t> objTemp;
            instTemp.reserve(range.count);
            objTemp.reserve(range.count);

            for (uint32_t k = 0; k < range.count; ++k) {
                instTemp.push_back(m_InstanceSlots[range.start + k].instance);
                objTemp.push_back(m_InstanceSlots[range.start + k].instance.objectID);
            }

            std::memcpy(instBytes + instOffset, instTemp.data(), instSize);
            std::memcpy(objBytes + range.start * sizeof(uint32_t), objTemp.data(), range.count * sizeof(uint32_t));

            m_UploadBytesThisFrame += instSize + range.count * sizeof(uint32_t);
        }

        vmaUnmapMemory(resources.allocator, frameRes.instanceAlloc);
        vmaUnmapMemory(resources.allocator, frameRes.objectIdAlloc);

        // Decrement dirty counters
        for (uint32_t idx : dirtyIndices) {
            if (m_InstanceSlots[idx].dirtyFrames > 0) {
                m_InstanceSlots[idx].dirtyFrames--;
            }
        }
    }

    // -------------------------------------------------------------------------
    // 6. Upload GPU Mesh Records Table
    // -------------------------------------------------------------------------
    std::vector<GPUMeshRecord> meshRecords;
    meshRecords.reserve(m_MeshSlots.size());
    for (const auto& slot : m_MeshSlots) {
        meshRecords.push_back(slot.record);
    }
    if (meshRecords.empty()) {
        meshRecords.push_back(GPUMeshRecord{});
    }

    uint32_t meshCount = static_cast<uint32_t>(meshRecords.size());
    uint32_t prevMeshDrawDataCapacity = frameRes.meshDrawDataCapacity;
    resizeBufferIfNeeded(
        resources,
        frameRes.meshDrawDataBuffer,
        frameRes.meshDrawDataAlloc,
        frameRes.meshDrawDataBufferSize,
        frameRes.meshDrawDataCapacity,
        meshCount,
        sizeof(GPUMeshRecord),
        VK_BUFFER_USAGE_STORAGE_BUFFER_BIT
    );
    if (frameRes.meshDrawDataCapacity != prevMeshDrawDataCapacity) {
        needsDescriptorUpdate = true;
    }

    void* meshDrawDataDst = nullptr;
    VK_CHECK(vmaMapMemory(resources.allocator, frameRes.meshDrawDataAlloc, &meshDrawDataDst));
    std::memcpy(meshDrawDataDst, meshRecords.data(), meshCount * sizeof(GPUMeshRecord));
    vmaUnmapMemory(resources.allocator, frameRes.meshDrawDataAlloc);

    // -------------------------------------------------------------------------
    // 7. Upload Material Override Buffer
    // -------------------------------------------------------------------------
    uint32_t overrideCount = static_cast<uint32_t>(m_MaterialOverrides.size());
    if (overrideCount == 0) {
        overrideCount = 1; // Default to at least 1 slot
    }
    uint32_t prevOverrideCapacity = frameRes.materialOverrideCapacity;
    resizeBufferIfNeeded(
        resources,
        frameRes.materialOverrideBuffer,
        frameRes.materialOverrideAlloc,
        frameRes.materialOverrideBufferSize,
        frameRes.materialOverrideCapacity,
        overrideCount,
        sizeof(uint32_t),
        VK_BUFFER_USAGE_STORAGE_BUFFER_BIT
    );
    if (frameRes.materialOverrideCapacity != prevOverrideCapacity) {
        needsDescriptorUpdate = true;
    }

    void* overrideDst = nullptr;
    VK_CHECK(vmaMapMemory(resources.allocator, frameRes.materialOverrideAlloc, &overrideDst));
    if (!m_MaterialOverrides.empty()) {
        std::memcpy(overrideDst, m_MaterialOverrides.data(), m_MaterialOverrides.size() * sizeof(uint32_t));
    } else {
        uint32_t zero = 0;
        std::memcpy(overrideDst, &zero, sizeof(uint32_t));
    }
    vmaUnmapMemory(resources.allocator, frameRes.materialOverrideAlloc);

    // Update descriptors if buffers grew or reallocated
    if (needsDescriptorUpdate) {
        writeDescriptorSet(resources, frameRes);
    }

    // -------------------------------------------------------------------------
    // 8. Populate Compat Cache vectors for G1/G2 path to use
    // -------------------------------------------------------------------------
    m_GPUInstances.clear();
    m_GPUInstances.reserve(m_InstanceSlots.size());
    for (const auto& slot : m_InstanceSlots) {
        if (slot.active) {
            m_GPUInstances.push_back(slot.instance);
        }
    }
    if (m_GPUInstances.empty()) {
        m_GPUInstances.push_back(GPUInstance{});
    }

    m_GPUMeshDrawData.clear();
    m_GPUMeshDrawData.reserve(m_MeshSlots.size());
    for (const auto& slot : m_MeshSlots) {
        if (slot.active) {
            // Draw data mapped directly
            GPUMeshDrawData draw{};
            draw.firstIndex = slot.record.firstIndex;
            draw.vertexOffset = slot.record.vertexOffset;
            draw.indexCount = slot.record.indexCount;
            draw.materialSlotOffset = slot.record.materialSlotOffset;
            m_GPUMeshDrawData.push_back(draw);
        }
    }
    if (m_GPUMeshDrawData.empty()) {
        m_GPUMeshDrawData.push_back(GPUMeshDrawData{});
    }
}

void GPUScene::createFrameResources(EngineResources& resources, GPUSceneFrameResources& frameRes)
{
    VkDescriptorPoolSize poolSizes[2]{};
    poolSizes[0].type            = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
    poolSizes[0].descriptorCount = 4;
    poolSizes[1].type            = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
    poolSizes[1].descriptorCount = 20;

    VkDescriptorPoolCreateInfo poolInfo{};
    poolInfo.sType         = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
    poolInfo.poolSizeCount = 2;
    poolInfo.pPoolSizes    = poolSizes;
    poolInfo.maxSets       = 3;
    poolInfo.flags         = VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT;

    VK_CHECK(vkCreateDescriptorPool(resources.device, &poolInfo, nullptr, &frameRes.descriptorPool));

    VkDescriptorSetAllocateInfo setAlloc{};
    setAlloc.sType              = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
    setAlloc.descriptorPool     = frameRes.descriptorPool;
    setAlloc.descriptorSetCount = 1;
    setAlloc.pSetLayouts        = &m_DescriptorSetLayout;

    VK_CHECK(vkAllocateDescriptorSets(resources.device, &setAlloc, &frameRes.descriptorSet));

    VkDescriptorSetAllocateInfo localSetAlloc{};
    localSetAlloc.sType              = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
    localSetAlloc.descriptorPool     = frameRes.descriptorPool;
    localSetAlloc.descriptorSetCount = 1;
    localSetAlloc.pSetLayouts        = &m_LocalLightsDescriptorSetLayout;

    VK_CHECK(vkAllocateDescriptorSets(resources.device, &localSetAlloc, &frameRes.localLightsDescriptorSet));

    VkDescriptorSetAllocateInfo cullingSetAlloc{};
    cullingSetAlloc.sType              = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
    cullingSetAlloc.descriptorPool     = frameRes.descriptorPool;
    cullingSetAlloc.descriptorSetCount = 1;
    cullingSetAlloc.pSetLayouts        = &m_LightCullingDescriptorSetLayout;

    VK_CHECK(vkAllocateDescriptorSets(resources.device, &cullingSetAlloc, &frameRes.lightCullingDescriptorSet));

    VkBufferCreateInfo bufInfo{};
    bufInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
    bufInfo.size  = sizeof(Omnix::Radiance::RadianceFrameUBO);
    bufInfo.usage = VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT;
    bufInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

    VmaAllocationCreateInfo vmaAllocInfo{};
    vmaAllocInfo.usage = VMA_MEMORY_USAGE_CPU_TO_GPU;

    VK_CHECK(vmaCreateBuffer(resources.allocator, &bufInfo, &vmaAllocInfo, &frameRes.cameraBuffer, &frameRes.cameraAlloc, nullptr));
    ::eng::ResourceTracker::incBuffer();

    bufInfo.size  = sizeof(GPUFrustum);
    bufInfo.usage = VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT;
    VK_CHECK(vmaCreateBuffer(resources.allocator, &bufInfo, &vmaAllocInfo, &frameRes.frustumBuffer, &frameRes.frustumAlloc, nullptr));
    ::eng::ResourceTracker::incBuffer();

    bufInfo.size  = sizeof(LightData);
    bufInfo.usage = VK_BUFFER_USAGE_STORAGE_BUFFER_BIT;
    VK_CHECK(vmaCreateBuffer(resources.allocator, &bufInfo, &vmaAllocInfo, &frameRes.lightBuffer, &frameRes.lightAlloc, nullptr));
    ::eng::ResourceTracker::incBuffer();

    frameRes.instanceCapacity = 128;
    frameRes.instanceBufferSize = frameRes.instanceCapacity * sizeof(GPUGeometryInstance);
    bufInfo.size = frameRes.instanceBufferSize;
    bufInfo.usage = VK_BUFFER_USAGE_STORAGE_BUFFER_BIT;
    VK_CHECK(vmaCreateBuffer(resources.allocator, &bufInfo, &vmaAllocInfo, &frameRes.instanceBuffer, &frameRes.instanceAlloc, nullptr));
    ::eng::ResourceTracker::incBuffer();

    frameRes.objectIdCapacity = 128;
    frameRes.objectIdBufferSize = frameRes.objectIdCapacity * sizeof(uint32_t);
    bufInfo.size = frameRes.objectIdBufferSize;
    bufInfo.usage = VK_BUFFER_USAGE_STORAGE_BUFFER_BIT;
    VK_CHECK(vmaCreateBuffer(resources.allocator, &bufInfo, &vmaAllocInfo, &frameRes.objectIdBuffer, &frameRes.objectIdAlloc, nullptr));
    ::eng::ResourceTracker::incBuffer();

    frameRes.meshDrawDataCapacity = 128;
    frameRes.meshDrawDataBufferSize = frameRes.meshDrawDataCapacity * sizeof(GPUMeshRecord);
    bufInfo.size = frameRes.meshDrawDataBufferSize;
    bufInfo.usage = VK_BUFFER_USAGE_STORAGE_BUFFER_BIT;
    VK_CHECK(vmaCreateBuffer(resources.allocator, &bufInfo, &vmaAllocInfo, &frameRes.meshDrawDataBuffer, &frameRes.meshDrawDataAlloc, nullptr));
    ::eng::ResourceTracker::incBuffer();

    // G3: GPU Material Override Buffer
    frameRes.materialOverrideCapacity = 256;
    frameRes.materialOverrideBufferSize = frameRes.materialOverrideCapacity * sizeof(uint32_t);
    bufInfo.size = frameRes.materialOverrideBufferSize;
    bufInfo.usage = VK_BUFFER_USAGE_STORAGE_BUFFER_BIT;
    VK_CHECK(vmaCreateBuffer(resources.allocator, &bufInfo, &vmaAllocInfo, &frameRes.materialOverrideBuffer, &frameRes.materialOverrideAlloc, nullptr));
    ::eng::ResourceTracker::incBuffer();

    frameRes.materialCapacity = 128;
    frameRes.materialBufferSize = frameRes.materialCapacity * sizeof(MaterialGPUData);
    bufInfo.size = frameRes.materialBufferSize;
    bufInfo.usage = VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT;
    
    VmaAllocationCreateInfo gpuAllocInfo{};
    gpuAllocInfo.usage = VMA_MEMORY_USAGE_GPU_ONLY;
    VK_CHECK(vmaCreateBuffer(resources.allocator, &bufInfo, &gpuAllocInfo, &frameRes.materialBuffer, &frameRes.materialAlloc, nullptr));
    ::eng::ResourceTracker::incBuffer();

    frameRes.localLightBuffer.resources = &resources;
    frameRes.localLightBuffer.size = 1 * sizeof(Omnix::Radiance::LocalLightGPU);
    frameRes.localLightBuffer.capacity = 1;

    VkBufferCreateInfo localLightBufInfo{};
    localLightBufInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
    localLightBufInfo.size = frameRes.localLightBuffer.size;
    localLightBufInfo.usage = VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT;
    localLightBufInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

    VmaAllocationCreateInfo localLightAllocInfo{};
    localLightAllocInfo.usage = VMA_MEMORY_USAGE_GPU_ONLY;

    VK_CHECK(vmaCreateBuffer(resources.allocator, &localLightBufInfo, &localLightAllocInfo, &frameRes.localLightBuffer.buffer, &frameRes.localLightBuffer.allocation, nullptr));
    ::eng::ResourceTracker::incBuffer();

    frameRes.clusterBoundsCapacity = 1;
    frameRes.clusterBoundsBufferSize = frameRes.clusterBoundsCapacity * sizeof(Omnix::Radiance::ClusterBoundsGPU);
    bufInfo.size = frameRes.clusterBoundsBufferSize;
    bufInfo.usage = VK_BUFFER_USAGE_STORAGE_BUFFER_BIT;
    VK_CHECK(vmaCreateBuffer(resources.allocator, &bufInfo, &vmaAllocInfo, &frameRes.clusterBoundsBuffer, &frameRes.clusterBoundsAlloc, nullptr));
    ::eng::ResourceTracker::incBuffer();

    frameRes.clusterRangeCapacity = 1;
    frameRes.clusterRangeBufferSize = frameRes.clusterRangeCapacity * sizeof(Omnix::Radiance::ClusterRangeGPU);
    bufInfo.size = frameRes.clusterRangeBufferSize;
    bufInfo.usage = VK_BUFFER_USAGE_STORAGE_BUFFER_BIT;
    VK_CHECK(vmaCreateBuffer(resources.allocator, &bufInfo, &vmaAllocInfo, &frameRes.clusterRangeBuffer, &frameRes.clusterRangeAlloc, nullptr));
    ::eng::ResourceTracker::incBuffer();

    frameRes.clusterLightIndexCapacity = 1;
    frameRes.clusterLightIndexBufferSize = frameRes.clusterLightIndexCapacity * sizeof(uint32_t);
    bufInfo.size = frameRes.clusterLightIndexBufferSize;
    bufInfo.usage = VK_BUFFER_USAGE_STORAGE_BUFFER_BIT;
    VK_CHECK(vmaCreateBuffer(resources.allocator, &bufInfo, &vmaAllocInfo, &frameRes.clusterLightIndexBuffer, &frameRes.clusterLightIndexAlloc, nullptr));
    ::eng::ResourceTracker::incBuffer();

    bufInfo.size = sizeof(Omnix::Radiance::ClusterSettingsGPU);
    bufInfo.usage = VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT;
    VK_CHECK(vmaCreateBuffer(resources.allocator, &bufInfo, &vmaAllocInfo, &frameRes.clusterSettingsBuffer, &frameRes.clusterSettingsAlloc, nullptr));
    ::eng::ResourceTracker::incBuffer();

    writeDescriptorSet(resources, frameRes);
}

void GPUScene::destroyFrameResources(EngineResources& resources, GPUSceneFrameResources& frameRes)
{
    if (frameRes.cameraBuffer != VK_NULL_HANDLE) {
        vmaDestroyBuffer(resources.allocator, frameRes.cameraBuffer, frameRes.cameraAlloc);
        ::eng::ResourceTracker::decBuffer();
        frameRes.cameraBuffer = VK_NULL_HANDLE;
    }
    if (frameRes.frustumBuffer != VK_NULL_HANDLE) {
        vmaDestroyBuffer(resources.allocator, frameRes.frustumBuffer, frameRes.frustumAlloc);
        ::eng::ResourceTracker::decBuffer();
        frameRes.frustumBuffer = VK_NULL_HANDLE;
    }
    if (frameRes.lightBuffer != VK_NULL_HANDLE) {
        vmaDestroyBuffer(resources.allocator, frameRes.lightBuffer, frameRes.lightAlloc);
        ::eng::ResourceTracker::decBuffer();
        frameRes.lightBuffer = VK_NULL_HANDLE;
    }
    if (frameRes.instanceBuffer != VK_NULL_HANDLE) {
        vmaDestroyBuffer(resources.allocator, frameRes.instanceBuffer, frameRes.instanceAlloc);
        ::eng::ResourceTracker::decBuffer();
        frameRes.instanceBuffer = VK_NULL_HANDLE;
    }
    if (frameRes.materialBuffer != VK_NULL_HANDLE) {
        vmaDestroyBuffer(resources.allocator, frameRes.materialBuffer, frameRes.materialAlloc);
        ::eng::ResourceTracker::decBuffer();
        frameRes.materialBuffer = VK_NULL_HANDLE;
    }
    if (frameRes.objectIdBuffer != VK_NULL_HANDLE) {
        vmaDestroyBuffer(resources.allocator, frameRes.objectIdBuffer, frameRes.objectIdAlloc);
        ::eng::ResourceTracker::decBuffer();
        frameRes.objectIdBuffer = VK_NULL_HANDLE;
    }
    if (frameRes.meshDrawDataBuffer != VK_NULL_HANDLE) {
        vmaDestroyBuffer(resources.allocator, frameRes.meshDrawDataBuffer, frameRes.meshDrawDataAlloc);
        ::eng::ResourceTracker::decBuffer();
        frameRes.meshDrawDataBuffer = VK_NULL_HANDLE;
    }
    // G3: GPU Material Override Buffer
    if (frameRes.materialOverrideBuffer != VK_NULL_HANDLE) {
        vmaDestroyBuffer(resources.allocator, frameRes.materialOverrideBuffer, frameRes.materialOverrideAlloc);
        ::eng::ResourceTracker::decBuffer();
        frameRes.materialOverrideBuffer = VK_NULL_HANDLE;
    }
    if (frameRes.localLightBuffer.buffer != VK_NULL_HANDLE) {
        vmaDestroyBuffer(resources.allocator, frameRes.localLightBuffer.buffer, frameRes.localLightBuffer.allocation);
        ::eng::ResourceTracker::decBuffer();
        frameRes.localLightBuffer.buffer = VK_NULL_HANDLE;
    }
    if (frameRes.clusterBoundsBuffer != VK_NULL_HANDLE) {
        vmaDestroyBuffer(resources.allocator, frameRes.clusterBoundsBuffer, frameRes.clusterBoundsAlloc);
        ::eng::ResourceTracker::decBuffer();
        frameRes.clusterBoundsBuffer = VK_NULL_HANDLE;
    }
    if (frameRes.clusterRangeBuffer != VK_NULL_HANDLE) {
        vmaDestroyBuffer(resources.allocator, frameRes.clusterRangeBuffer, frameRes.clusterRangeAlloc);
        ::eng::ResourceTracker::decBuffer();
        frameRes.clusterRangeBuffer = VK_NULL_HANDLE;
    }
    if (frameRes.clusterLightIndexBuffer != VK_NULL_HANDLE) {
        vmaDestroyBuffer(resources.allocator, frameRes.clusterLightIndexBuffer, frameRes.clusterLightIndexAlloc);
        ::eng::ResourceTracker::decBuffer();
        frameRes.clusterLightIndexBuffer = VK_NULL_HANDLE;
    }
    if (frameRes.clusterSettingsBuffer != VK_NULL_HANDLE) {
        vmaDestroyBuffer(resources.allocator, frameRes.clusterSettingsBuffer, frameRes.clusterSettingsAlloc);
        ::eng::ResourceTracker::decBuffer();
        frameRes.clusterSettingsBuffer = VK_NULL_HANDLE;
    }
    if (frameRes.descriptorPool != VK_NULL_HANDLE) {
        vkDestroyDescriptorPool(resources.device, frameRes.descriptorPool, nullptr);
        frameRes.descriptorPool = VK_NULL_HANDLE;
    }
}

void GPUScene::resizeBufferIfNeeded(
    EngineResources& resources,
    VkBuffer& buffer,
    VmaAllocation& allocation,
    VkDeviceSize& currentSize,
    uint32_t& currentCapacity,
    uint32_t neededCapacity,
    VkDeviceSize elementSize,
    VkBufferUsageFlags usage
) {
    if (neededCapacity <= currentCapacity) return;

    uint32_t newCapacity = currentCapacity * 2;
    while (newCapacity < neededCapacity) {
        newCapacity *= 2;
    }

    if (buffer != VK_NULL_HANDLE) {
        vmaDestroyBuffer(resources.allocator, buffer, allocation);
        ::eng::ResourceTracker::decBuffer();
    }

    currentCapacity = newCapacity;
    currentSize = newCapacity * elementSize;

    VkBufferCreateInfo bufInfo{};
    bufInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
    bufInfo.size  = currentSize;
    bufInfo.usage = usage;
    bufInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

    VmaAllocationCreateInfo vmaAllocInfo{};
    vmaAllocInfo.usage = VMA_MEMORY_USAGE_CPU_TO_GPU;

    VK_CHECK(vmaCreateBuffer(resources.allocator, &bufInfo, &vmaAllocInfo, &buffer, &allocation, nullptr));
    ::eng::ResourceTracker::incBuffer();

    LOG_INFO("[GPUScene] Resized buffer (usage " + std::to_string(usage) + ") to capacity " + std::to_string(newCapacity) + " (" + std::to_string(currentSize) + " bytes)");
}

void GPUScene::createDescriptorSetLayout(EngineResources& resources)
{
    VkDescriptorSetLayoutBinding bindings[8]{};

    // Binding 0: Camera Uniform Buffer
    bindings[0].binding            = 0;
    bindings[0].descriptorType     = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
    bindings[0].descriptorCount    = 1;
    bindings[0].stageFlags         = VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT | VK_SHADER_STAGE_COMPUTE_BIT;
    bindings[0].pImmutableSamplers = nullptr;

    // Binding 1: Instance Storage Buffer
    bindings[1].binding            = 1;
    bindings[1].descriptorType     = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
    bindings[1].descriptorCount    = 1;
    bindings[1].stageFlags         = VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT | VK_SHADER_STAGE_COMPUTE_BIT;
    bindings[1].pImmutableSamplers = nullptr;

    // Binding 2: Material Storage Buffer
    bindings[2].binding            = 2;
    bindings[2].descriptorType     = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
    bindings[2].descriptorCount    = 1;
    bindings[2].stageFlags         = VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT;
    bindings[2].pImmutableSamplers = nullptr;

    // Binding 3: Light Storage Buffer
    bindings[3].binding            = 3;
    bindings[3].descriptorType     = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
    bindings[3].descriptorCount    = 1;
    bindings[3].stageFlags         = VK_SHADER_STAGE_FRAGMENT_BIT;
    bindings[3].pImmutableSamplers = nullptr;

    // Binding 4: ObjectID Storage Buffer
    bindings[4].binding            = 4;
    bindings[4].descriptorType     = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
    bindings[4].descriptorCount    = 1;
    bindings[4].stageFlags         = VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT;
    bindings[4].pImmutableSamplers = nullptr;

    // Binding 5: Frustum Uniform Buffer
    bindings[5].binding            = 5;
    bindings[5].descriptorType     = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
    bindings[5].descriptorCount    = 1;
    bindings[5].stageFlags         = VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT | VK_SHADER_STAGE_COMPUTE_BIT;
    bindings[5].pImmutableSamplers = nullptr;

    // Binding 6: MeshDrawData Storage Buffer
    bindings[6].binding            = 6;
    bindings[6].descriptorType     = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
    bindings[6].descriptorCount    = 1;
    bindings[6].stageFlags         = VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT;
    bindings[6].pImmutableSamplers = nullptr;

    // G3 Binding 7: Material Override Storage Buffer
    bindings[7].binding            = 7;
    bindings[7].descriptorType     = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
    bindings[7].descriptorCount    = 1;
    bindings[7].stageFlags         = VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT;
    bindings[7].pImmutableSamplers = nullptr;

    // G8 Binding 8: RVG Asset Table Buffer
    VkDescriptorSetLayoutBinding assetBinding{};
    assetBinding.binding            = 8;
    assetBinding.descriptorType     = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
    assetBinding.descriptorCount    = 1;
    assetBinding.stageFlags         = VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT;
    assetBinding.pImmutableSamplers = nullptr;

    // G8 Binding 9: RVG Node Buffer
    VkDescriptorSetLayoutBinding nodeBinding{};
    nodeBinding.binding            = 9;
    nodeBinding.descriptorType     = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
    nodeBinding.descriptorCount    = 1;
    nodeBinding.stageFlags         = VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT;
    nodeBinding.pImmutableSamplers = nullptr;

    // G8 Binding 10: RVG Cluster Buffer
    VkDescriptorSetLayoutBinding clusterBinding{};
    clusterBinding.binding            = 10;
    clusterBinding.descriptorType     = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
    clusterBinding.descriptorCount    = 1;
    clusterBinding.stageFlags         = VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT | VK_SHADER_STAGE_COMPUTE_BIT;
    clusterBinding.pImmutableSamplers = nullptr;

    // G9 Binding 11: Virtual Page Table
    VkDescriptorSetLayoutBinding virtualPageTableBinding{};
    virtualPageTableBinding.binding            = 11;
    virtualPageTableBinding.descriptorType     = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
    virtualPageTableBinding.descriptorCount    = 1;
    virtualPageTableBinding.stageFlags         = VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT | VK_SHADER_STAGE_COMPUTE_BIT;
    virtualPageTableBinding.pImmutableSamplers = nullptr;

    // G9 Binding 12: Streaming Request Buffer
    VkDescriptorSetLayoutBinding streamingRequestBinding{};
    streamingRequestBinding.binding            = 12;
    streamingRequestBinding.descriptorType     = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
    streamingRequestBinding.descriptorCount    = 1;
    streamingRequestBinding.stageFlags         = VK_SHADER_STAGE_COMPUTE_BIT;
    streamingRequestBinding.pImmutableSamplers = nullptr;

    std::vector<VkDescriptorSetLayoutBinding> allBindings;
    for (int b = 0; b < 8; ++b) allBindings.push_back(bindings[b]);
    allBindings.push_back(assetBinding);
    allBindings.push_back(nodeBinding);
    allBindings.push_back(clusterBinding);
    allBindings.push_back(virtualPageTableBinding);
    allBindings.push_back(streamingRequestBinding);

    VkDescriptorSetLayoutCreateInfo layoutInfo{};
    layoutInfo.sType        = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
    layoutInfo.bindingCount = static_cast<uint32_t>(allBindings.size());
    layoutInfo.pBindings    = allBindings.data();

    VK_CHECK(vkCreateDescriptorSetLayout(resources.device, &layoutInfo, nullptr, &m_DescriptorSetLayout));

    // Local lights layout bindings (Clustered shading)
    VkDescriptorSetLayoutBinding localBindings[5]{};
    localBindings[0].binding            = 0;
    localBindings[0].descriptorType     = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
    localBindings[0].descriptorCount    = 1;
    localBindings[0].stageFlags         = VK_SHADER_STAGE_FRAGMENT_BIT;
    localBindings[0].pImmutableSamplers = nullptr;

    localBindings[1].binding            = 1;
    localBindings[1].descriptorType     = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
    localBindings[1].descriptorCount    = 1;
    localBindings[1].stageFlags         = VK_SHADER_STAGE_FRAGMENT_BIT;
    localBindings[1].pImmutableSamplers = nullptr;

    localBindings[2].binding            = 2;
    localBindings[2].descriptorType     = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
    localBindings[2].descriptorCount    = 1;
    localBindings[2].stageFlags         = VK_SHADER_STAGE_FRAGMENT_BIT;
    localBindings[2].pImmutableSamplers = nullptr;

    localBindings[3].binding            = 3;
    localBindings[3].descriptorType     = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
    localBindings[3].descriptorCount    = 1;
    localBindings[3].stageFlags         = VK_SHADER_STAGE_FRAGMENT_BIT;
    localBindings[3].pImmutableSamplers = nullptr;

    localBindings[4].binding            = 4;
    localBindings[4].descriptorType     = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
    localBindings[4].descriptorCount    = 1;
    localBindings[4].stageFlags         = VK_SHADER_STAGE_FRAGMENT_BIT;
    localBindings[4].pImmutableSamplers = nullptr;

    VkDescriptorSetLayoutCreateInfo localLayoutInfo{};
    localLayoutInfo.sType        = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
    localLayoutInfo.bindingCount = 5;
    localLayoutInfo.pBindings    = localBindings;

    VK_CHECK(vkCreateDescriptorSetLayout(resources.device, &localLayoutInfo, nullptr, &m_LocalLightsDescriptorSetLayout));

    // Light culling compute layout bindings
    VkDescriptorSetLayoutBinding cullingBindings[6]{};
    cullingBindings[0].binding            = 0;
    cullingBindings[0].descriptorType     = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
    cullingBindings[0].descriptorCount    = 1;
    cullingBindings[0].stageFlags         = VK_SHADER_STAGE_COMPUTE_BIT;
    cullingBindings[0].pImmutableSamplers = nullptr;

    cullingBindings[1].binding            = 1;
    cullingBindings[1].descriptorType     = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
    cullingBindings[1].descriptorCount    = 1;
    cullingBindings[1].stageFlags         = VK_SHADER_STAGE_COMPUTE_BIT;
    cullingBindings[1].pImmutableSamplers = nullptr;

    cullingBindings[2].binding            = 2;
    cullingBindings[2].descriptorType     = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
    cullingBindings[2].descriptorCount    = 1;
    cullingBindings[2].stageFlags         = VK_SHADER_STAGE_COMPUTE_BIT;
    cullingBindings[2].pImmutableSamplers = nullptr;

    cullingBindings[3].binding            = 3;
    cullingBindings[3].descriptorType     = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
    cullingBindings[3].descriptorCount    = 1;
    cullingBindings[3].stageFlags         = VK_SHADER_STAGE_COMPUTE_BIT;
    cullingBindings[3].pImmutableSamplers = nullptr;

    cullingBindings[4].binding            = 4;
    cullingBindings[4].descriptorType     = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
    cullingBindings[4].descriptorCount    = 1;
    cullingBindings[4].stageFlags         = VK_SHADER_STAGE_COMPUTE_BIT;
    cullingBindings[4].pImmutableSamplers = nullptr;

    cullingBindings[5].binding            = 5;
    cullingBindings[5].descriptorType     = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
    cullingBindings[5].descriptorCount    = 1;
    cullingBindings[5].stageFlags         = VK_SHADER_STAGE_COMPUTE_BIT;
    cullingBindings[5].pImmutableSamplers = nullptr;

    VkDescriptorSetLayoutCreateInfo cullingLayoutInfo{};
    cullingLayoutInfo.sType        = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
    cullingLayoutInfo.bindingCount = 6;
    cullingLayoutInfo.pBindings    = cullingBindings;

    VK_CHECK(vkCreateDescriptorSetLayout(resources.device, &cullingLayoutInfo, nullptr, &m_LightCullingDescriptorSetLayout));
}

void GPUScene::writeDescriptorSet(EngineResources& resources, GPUSceneFrameResources& frameRes)
{
    std::vector<VkWriteDescriptorSet> writes;
    writes.reserve(8);

    // Binding 0: Camera Uniform Buffer
    VkDescriptorBufferInfo cameraInfo{};
    cameraInfo.buffer = frameRes.cameraBuffer;
    cameraInfo.offset = 0;
    cameraInfo.range  = sizeof(Omnix::Radiance::RadianceFrameUBO);

    VkWriteDescriptorSet cameraWrite{};
    cameraWrite.sType           = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    cameraWrite.dstSet          = frameRes.descriptorSet;
    cameraWrite.dstBinding      = 0;
    cameraWrite.dstArrayElement = 0;
    cameraWrite.descriptorCount = 1;
    cameraWrite.descriptorType  = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
    cameraWrite.pBufferInfo     = &cameraInfo;
    writes.push_back(cameraWrite);

    // Binding 1: Instance Storage Buffer
    VkDescriptorBufferInfo instanceInfo{};
    instanceInfo.buffer = frameRes.instanceBuffer;
    instanceInfo.offset = 0;
    instanceInfo.range  = frameRes.instanceBufferSize;

    VkWriteDescriptorSet instanceWrite{};
    instanceWrite.sType           = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    instanceWrite.dstSet          = frameRes.descriptorSet;
    instanceWrite.dstBinding      = 1;
    instanceWrite.dstArrayElement = 0;
    instanceWrite.descriptorCount = 1;
    instanceWrite.descriptorType  = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
    instanceWrite.pBufferInfo     = &instanceInfo;
    writes.push_back(instanceWrite);

    // Binding 2: Material Storage Buffer
    VkDescriptorBufferInfo materialInfo{};
    materialInfo.buffer = frameRes.materialBuffer;
    materialInfo.offset = 0;
    materialInfo.range  = frameRes.materialBufferSize;

    VkWriteDescriptorSet materialWrite{};
    materialWrite.sType           = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    materialWrite.dstSet          = frameRes.descriptorSet;
    materialWrite.dstBinding      = 2;
    materialWrite.dstArrayElement = 0;
    materialWrite.descriptorCount = 1;
    materialWrite.descriptorType  = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
    materialWrite.pBufferInfo     = &materialInfo;
    writes.push_back(materialWrite);

    // Binding 3: Light Storage Buffer
    VkDescriptorBufferInfo lightInfo{};
    lightInfo.buffer = frameRes.lightBuffer;
    lightInfo.offset = 0;
    lightInfo.range  = sizeof(LightData);

    VkWriteDescriptorSet lightWrite{};
    lightWrite.sType           = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    lightWrite.dstSet          = frameRes.descriptorSet;
    lightWrite.dstBinding      = 3;
    lightWrite.dstArrayElement = 0;
    lightWrite.descriptorCount = 1;
    lightWrite.descriptorType  = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
    lightWrite.pBufferInfo     = &lightInfo;
    writes.push_back(lightWrite);

    // Binding 4: ObjectID Storage Buffer
    VkDescriptorBufferInfo objectIdInfo{};
    objectIdInfo.buffer = frameRes.objectIdBuffer;
    objectIdInfo.offset = 0;
    objectIdInfo.range  = frameRes.objectIdBufferSize;

    VkWriteDescriptorSet objectIdWrite{};
    objectIdWrite.sType           = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    objectIdWrite.dstSet          = frameRes.descriptorSet;
    objectIdWrite.dstBinding      = 4;
    objectIdWrite.dstArrayElement = 0;
    objectIdWrite.descriptorCount = 1;
    objectIdWrite.descriptorType  = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
    objectIdWrite.pBufferInfo     = &objectIdInfo;
    writes.push_back(objectIdWrite);

    // Binding 5: Frustum Uniform Buffer
    VkDescriptorBufferInfo frustumInfo{};
    frustumInfo.buffer = frameRes.frustumBuffer;
    frustumInfo.offset = 0;
    frustumInfo.range  = sizeof(GPUFrustum);

    VkWriteDescriptorSet frustumWrite{};
    frustumWrite.sType           = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    frustumWrite.dstSet          = frameRes.descriptorSet;
    frustumWrite.dstBinding      = 5;
    frustumWrite.dstArrayElement = 0;
    frustumWrite.descriptorCount = 1;
    frustumWrite.descriptorType  = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
    frustumWrite.pBufferInfo     = &frustumInfo;
    writes.push_back(frustumWrite);

    // Binding 6: MeshDrawData Storage Buffer
    VkDescriptorBufferInfo meshDrawDataInfo{};
    meshDrawDataInfo.buffer = frameRes.meshDrawDataBuffer;
    meshDrawDataInfo.offset = 0;
    meshDrawDataInfo.range  = frameRes.meshDrawDataBufferSize;

    VkWriteDescriptorSet meshDrawDataWrite{};
    meshDrawDataWrite.sType           = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    meshDrawDataWrite.dstSet          = frameRes.descriptorSet;
    meshDrawDataWrite.dstBinding      = 6;
    meshDrawDataWrite.dstArrayElement = 0;
    meshDrawDataWrite.descriptorCount = 1;
    meshDrawDataWrite.descriptorType  = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
    meshDrawDataWrite.pBufferInfo     = &meshDrawDataInfo;
    writes.push_back(meshDrawDataWrite);

    // Binding 7: Material Override Storage Buffer
    VkDescriptorBufferInfo materialOverrideInfo{};
    materialOverrideInfo.buffer = frameRes.materialOverrideBuffer;
    materialOverrideInfo.offset = 0;
    materialOverrideInfo.range  = frameRes.materialOverrideBufferSize;

    VkWriteDescriptorSet materialOverrideWrite{};
    materialOverrideWrite.sType           = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    materialOverrideWrite.dstSet          = frameRes.descriptorSet;
    materialOverrideWrite.dstBinding      = 7;
    materialOverrideWrite.dstArrayElement = 0;
    materialOverrideWrite.descriptorCount = 1;
    materialOverrideWrite.descriptorType  = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
    materialOverrideWrite.pBufferInfo     = &materialOverrideInfo;
    writes.push_back(materialOverrideWrite);

    // Bindings 8, 9, 10: RVG buffers or fallback to camera UBO if not loaded yet
    VkBuffer rvgAssetBuf = RVGRegistry::Get().GetAssetTableBuffer();
    VkBuffer rvgNodeBuf = RVGRegistry::Get().GetNodesBuffer();
    VkBuffer rvgClusterBuf = RVGRegistry::Get().GetClustersBuffer();
    
    if (rvgAssetBuf == VK_NULL_HANDLE || rvgNodeBuf == VK_NULL_HANDLE || rvgClusterBuf == VK_NULL_HANDLE) {
        rvgAssetBuf = frameRes.instanceBuffer;
        rvgNodeBuf = frameRes.instanceBuffer;
        rvgClusterBuf = frameRes.instanceBuffer;
    }

    VkDescriptorBufferInfo rvgAssetInfo{};
    rvgAssetInfo.buffer = rvgAssetBuf;
    rvgAssetInfo.offset = 0;
    rvgAssetInfo.range  = VK_WHOLE_SIZE;

    VkDescriptorBufferInfo rvgNodeInfo{};
    rvgNodeInfo.buffer = rvgNodeBuf;
    rvgNodeInfo.offset = 0;
    rvgNodeInfo.range  = VK_WHOLE_SIZE;

    VkDescriptorBufferInfo rvgClusterInfo{};
    rvgClusterInfo.buffer = rvgClusterBuf;
    rvgClusterInfo.offset = 0;
    rvgClusterInfo.range  = VK_WHOLE_SIZE;

    VkWriteDescriptorSet rvgAssetWrite{};
    rvgAssetWrite.sType           = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    rvgAssetWrite.dstSet          = frameRes.descriptorSet;
    rvgAssetWrite.dstBinding      = 8;
    rvgAssetWrite.dstArrayElement = 0;
    rvgAssetWrite.descriptorCount = 1;
    rvgAssetWrite.descriptorType  = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
    rvgAssetWrite.pBufferInfo     = &rvgAssetInfo;
    writes.push_back(rvgAssetWrite);

    VkWriteDescriptorSet rvgNodeWrite{};
    rvgNodeWrite.sType           = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    rvgNodeWrite.dstSet          = frameRes.descriptorSet;
    rvgNodeWrite.dstBinding      = 9;
    rvgNodeWrite.dstArrayElement = 0;
    rvgNodeWrite.descriptorCount = 1;
    rvgNodeWrite.descriptorType  = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
    rvgNodeWrite.pBufferInfo     = &rvgNodeInfo;
    writes.push_back(rvgNodeWrite);

    VkWriteDescriptorSet rvgClusterWrite{};
    rvgClusterWrite.sType           = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    rvgClusterWrite.dstSet          = frameRes.descriptorSet;
    rvgClusterWrite.dstBinding      = 10;
    rvgClusterWrite.dstArrayElement = 0;
    rvgClusterWrite.descriptorCount = 1;
    rvgClusterWrite.descriptorType  = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
    rvgClusterWrite.pBufferInfo     = &rvgClusterInfo;
    writes.push_back(rvgClusterWrite);

    // Determine frameIndex from frameRes reference
    uint32_t frameIndex = 0;
    for (uint32_t i = 0; i < m_Frames.size(); ++i) {
        if (&m_Frames[i] == &frameRes) {
            frameIndex = i;
            break;
        }
    }

    // G9 Bindings 11 and 12: Virtual Page Table and Streaming Request Buffer
    VkBuffer virtualPageTableBuf = RVGPageStreamingManager::Get().GetVirtualPageTableBuffer(frameIndex);
    VkBuffer streamingRequestBuf = RVGPageStreamingManager::Get().GetStreamingRequestBuffer(frameIndex);

    if (virtualPageTableBuf == VK_NULL_HANDLE || streamingRequestBuf == VK_NULL_HANDLE) {
        virtualPageTableBuf = frameRes.instanceBuffer;
        streamingRequestBuf = frameRes.instanceBuffer;
    }

    VkDescriptorBufferInfo virtualPageTableInfo{};
    virtualPageTableInfo.buffer = virtualPageTableBuf;
    virtualPageTableInfo.offset = 0;
    virtualPageTableInfo.range  = VK_WHOLE_SIZE;

    VkDescriptorBufferInfo streamingRequestInfo{};
    streamingRequestInfo.buffer = streamingRequestBuf;
    streamingRequestInfo.offset = 0;
    streamingRequestInfo.range  = VK_WHOLE_SIZE;

    VkWriteDescriptorSet virtualPageTableWrite{};
    virtualPageTableWrite.sType           = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    virtualPageTableWrite.dstSet          = frameRes.descriptorSet;
    virtualPageTableWrite.dstBinding      = 11;
    virtualPageTableWrite.dstArrayElement = 0;
    virtualPageTableWrite.descriptorCount = 1;
    virtualPageTableWrite.descriptorType  = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
    virtualPageTableWrite.pBufferInfo     = &virtualPageTableInfo;
    writes.push_back(virtualPageTableWrite);

    VkWriteDescriptorSet streamingRequestWrite{};
    streamingRequestWrite.sType           = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    streamingRequestWrite.dstSet          = frameRes.descriptorSet;
    streamingRequestWrite.dstBinding      = 12;
    streamingRequestWrite.dstArrayElement = 0;
    streamingRequestWrite.descriptorCount = 1;
    streamingRequestWrite.descriptorType  = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
    streamingRequestWrite.pBufferInfo     = &streamingRequestInfo;
    writes.push_back(streamingRequestWrite);

    vkUpdateDescriptorSets(resources.device, static_cast<uint32_t>(writes.size()), writes.data(), 0, nullptr);

    // Write local lights descriptor set
    std::vector<VkWriteDescriptorSet> localSetWrites;
    VkDescriptorBufferInfo localLightInfo{};
    VkDescriptorBufferInfo clusterBoundsInfo{};
    VkDescriptorBufferInfo clusterRangeInfo{};
    VkDescriptorBufferInfo clusterLightIndexInfo{};
    VkDescriptorBufferInfo clusterSettingsInfo{};

    if (frameRes.localLightBuffer.buffer != VK_NULL_HANDLE) {
        localLightInfo.buffer = frameRes.localLightBuffer.buffer;
        localLightInfo.offset = 0;
        localLightInfo.range  = frameRes.localLightBuffer.size;

        VkWriteDescriptorSet localLightWrite{};
        localLightWrite.sType           = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        localLightWrite.dstSet          = frameRes.localLightsDescriptorSet;
        localLightWrite.dstBinding      = 0;
        localLightWrite.dstArrayElement = 0;
        localLightWrite.descriptorCount = 1;
        localLightWrite.descriptorType  = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
        localLightWrite.pBufferInfo     = &localLightInfo;
        localSetWrites.push_back(localLightWrite);
    }

    if (frameRes.clusterBoundsBuffer != VK_NULL_HANDLE) {
        clusterBoundsInfo.buffer = frameRes.clusterBoundsBuffer;
        clusterBoundsInfo.offset = 0;
        clusterBoundsInfo.range  = frameRes.clusterBoundsBufferSize;

        VkWriteDescriptorSet clusterBoundsWrite{};
        clusterBoundsWrite.sType           = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        clusterBoundsWrite.dstSet          = frameRes.localLightsDescriptorSet;
        clusterBoundsWrite.dstBinding      = 1;
        clusterBoundsWrite.dstArrayElement = 0;
        clusterBoundsWrite.descriptorCount = 1;
        clusterBoundsWrite.descriptorType  = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
        clusterBoundsWrite.pBufferInfo     = &clusterBoundsInfo;
        localSetWrites.push_back(clusterBoundsWrite);
    }

    if (frameRes.clusterRangeBuffer != VK_NULL_HANDLE) {
        clusterRangeInfo.buffer = frameRes.clusterRangeBuffer;
        clusterRangeInfo.offset = 0;
        clusterRangeInfo.range  = frameRes.clusterRangeBufferSize;

        VkWriteDescriptorSet clusterRangeWrite{};
        clusterRangeWrite.sType           = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        clusterRangeWrite.dstSet          = frameRes.localLightsDescriptorSet;
        clusterRangeWrite.dstBinding      = 2;
        clusterRangeWrite.dstArrayElement = 0;
        clusterRangeWrite.descriptorCount = 1;
        clusterRangeWrite.descriptorType  = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
        clusterRangeWrite.pBufferInfo     = &clusterRangeInfo;
        localSetWrites.push_back(clusterRangeWrite);
    }

    if (frameRes.clusterLightIndexBuffer != VK_NULL_HANDLE) {
        clusterLightIndexInfo.buffer = frameRes.clusterLightIndexBuffer;
        clusterLightIndexInfo.offset = 0;
        clusterLightIndexInfo.range  = frameRes.clusterLightIndexBufferSize;

        VkWriteDescriptorSet clusterLightIndexWrite{};
        clusterLightIndexWrite.sType           = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        clusterLightIndexWrite.dstSet          = frameRes.localLightsDescriptorSet;
        clusterLightIndexWrite.dstBinding      = 3;
        clusterLightIndexWrite.dstArrayElement = 0;
        clusterLightIndexWrite.descriptorCount = 1;
        clusterLightIndexWrite.descriptorType  = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
        clusterLightIndexWrite.pBufferInfo     = &clusterLightIndexInfo;
        localSetWrites.push_back(clusterLightIndexWrite);
    }

    if (frameRes.clusterSettingsBuffer != VK_NULL_HANDLE) {
        clusterSettingsInfo.buffer = frameRes.clusterSettingsBuffer;
        clusterSettingsInfo.offset = 0;
        clusterSettingsInfo.range  = sizeof(Omnix::Radiance::ClusterSettingsGPU);

        VkWriteDescriptorSet clusterSettingsWrite{};
        clusterSettingsWrite.sType           = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        clusterSettingsWrite.dstSet          = frameRes.localLightsDescriptorSet;
        clusterSettingsWrite.dstBinding      = 4;
        clusterSettingsWrite.dstArrayElement = 0;
        clusterSettingsWrite.descriptorCount = 1;
        clusterSettingsWrite.descriptorType  = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
        clusterSettingsWrite.pBufferInfo     = &clusterSettingsInfo;
        localSetWrites.push_back(clusterSettingsWrite);
    }

    if (!localSetWrites.empty()) {
        vkUpdateDescriptorSets(resources.device, static_cast<uint32_t>(localSetWrites.size()), localSetWrites.data(), 0, nullptr);
    }

    // Write light culling compute shader descriptors
    std::vector<VkWriteDescriptorSet> cullingSetWrites;
    if (frameRes.clusterSettingsBuffer != VK_NULL_HANDLE) {
        VkDescriptorBufferInfo csInfo{};
        csInfo.buffer = frameRes.clusterSettingsBuffer;
        csInfo.offset = 0;
        csInfo.range  = sizeof(Omnix::Radiance::ClusterSettingsGPU);

        VkWriteDescriptorSet w{};
        w.sType           = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        w.dstSet          = frameRes.lightCullingDescriptorSet;
        w.dstBinding      = 0;
        w.dstArrayElement = 0;
        w.descriptorCount = 1;
        w.descriptorType  = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
        w.pBufferInfo     = &csInfo;
        cullingSetWrites.push_back(w);
    }
    if (frameRes.localLightBuffer.buffer != VK_NULL_HANDLE) {
        VkDescriptorBufferInfo lbInfo{};
        lbInfo.buffer = frameRes.localLightBuffer.buffer;
        lbInfo.offset = 0;
        lbInfo.range  = frameRes.localLightBuffer.size;

        VkWriteDescriptorSet w{};
        w.sType           = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        w.dstSet          = frameRes.lightCullingDescriptorSet;
        w.dstBinding      = 1;
        w.dstArrayElement = 0;
        w.descriptorCount = 1;
        w.descriptorType  = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
        w.pBufferInfo     = &lbInfo;
        cullingSetWrites.push_back(w);
    }
    if (frameRes.clusterBoundsBuffer != VK_NULL_HANDLE) {
        VkDescriptorBufferInfo cbInfo{};
        cbInfo.buffer = frameRes.clusterBoundsBuffer;
        cbInfo.offset = 0;
        cbInfo.range  = frameRes.clusterBoundsBufferSize;

        VkWriteDescriptorSet w{};
        w.sType           = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        w.dstSet          = frameRes.lightCullingDescriptorSet;
        w.dstBinding      = 2;
        w.dstArrayElement = 0;
        w.descriptorCount = 1;
        w.descriptorType  = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
        w.pBufferInfo     = &cbInfo;
        cullingSetWrites.push_back(w);
    }
    if (frameRes.clusterRangeBuffer != VK_NULL_HANDLE) {
        VkDescriptorBufferInfo crInfo{};
        crInfo.buffer = frameRes.clusterRangeBuffer;
        crInfo.offset = 0;
        crInfo.range  = frameRes.clusterRangeBufferSize;

        VkWriteDescriptorSet w{};
        w.sType           = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        w.dstSet          = frameRes.lightCullingDescriptorSet;
        w.dstBinding      = 3;
        w.dstArrayElement = 0;
        w.descriptorCount = 1;
        w.descriptorType  = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
        w.pBufferInfo     = &crInfo;
        cullingSetWrites.push_back(w);
    }
    if (frameRes.clusterLightIndexBuffer != VK_NULL_HANDLE) {
        VkDescriptorBufferInfo cliInfo{};
        cliInfo.buffer = frameRes.clusterLightIndexBuffer;
        cliInfo.offset = 0;
        cliInfo.range  = frameRes.clusterLightIndexBufferSize;

        VkWriteDescriptorSet w{};
        w.sType           = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        w.dstSet          = frameRes.lightCullingDescriptorSet;
        w.dstBinding      = 4;
        w.dstArrayElement = 0;
        w.descriptorCount = 1;
        w.descriptorType  = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
        w.pBufferInfo     = &cliInfo;
        cullingSetWrites.push_back(w);
    }
    if (!cullingSetWrites.empty()) {
        vkUpdateDescriptorSets(resources.device, static_cast<uint32_t>(cullingSetWrites.size()), cullingSetWrites.data(), 0, nullptr);
    }
}

void GPUSceneFrameResources::LocalLightBuffer::Upload(const void* data, size_t dataSize) {
    if (buffer == VK_NULL_HANDLE || dataSize == 0) return;
    
    // Resize localLightBuffer if capacity is exceeded
    if (dataSize > size) {
        if (buffer != VK_NULL_HANDLE) {
            vmaDestroyBuffer(resources->allocator, buffer, allocation);
            ::eng::ResourceTracker::decBuffer();
        }

        size = dataSize;
        VkBufferCreateInfo bufInfo{};
        bufInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
        bufInfo.size = size;
        bufInfo.usage = VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT;
        bufInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

        VmaAllocationCreateInfo allocInfo{};
        allocInfo.usage = VMA_MEMORY_USAGE_GPU_ONLY;

        VK_CHECK(vmaCreateBuffer(resources->allocator, &bufInfo, &allocInfo, &buffer, &allocation, nullptr));
        ::eng::ResourceTracker::incBuffer();
    }

    resources->ensureStagingBuffer(dataSize);
    void* stagePtr = nullptr;
    VK_CHECK(vmaMapMemory(resources->allocator, resources->transfer.stagingAlloc, &stagePtr));
    std::memcpy(stagePtr, data, dataSize);
    vmaUnmapMemory(resources->allocator, resources->transfer.stagingAlloc);

    VkCommandBuffer cmd = resources->beginSingleTimeCommands();
    resources->copyStagingToDevice(cmd, buffer, 0, dataSize);
    resources->endSingleTimeCommands(cmd);
}

} // namespace eng::renderer
