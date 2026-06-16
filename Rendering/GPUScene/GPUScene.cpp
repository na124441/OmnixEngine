#include "Core/pch.h"
#include "GPUScene.h"
#include "GPUInstance.h"
#include "Rendering/Visibility/Frustum.h"
#include "Core/Vulkan/VkUtils.h"
#include "Core/Engine/Log.h"
#include "Core/Engine/VmaHelpers.h"
#include "Core/Engine/ResourceTracker.h"
#include "Rendering/Scene/RenderSceneExtractor.h"
#include "RenderingEngine/Renderer/scene/Mesh.h"
#include <cstring>
#include <algorithm>

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

void GPUSceneFrameResources::LocalLightBuffer::Upload(const void* data, size_t dataSize)
{
    if (!resources || dataSize == 0) return;

    if (size < dataSize) {
        if (buffer != VK_NULL_HANDLE) {
            vmaDestroyBuffer(resources->allocator, buffer, allocation);
            ::eng::ResourceTracker::decBuffer();
        }

        VkBufferCreateInfo bufferInfo{};
        bufferInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
        bufferInfo.size = dataSize;
        bufferInfo.usage = VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT;
        bufferInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

        VmaAllocationCreateInfo allocInfo{};
        allocInfo.usage = VMA_MEMORY_USAGE_GPU_ONLY;

        VK_CHECK(vmaCreateBuffer(resources->allocator, &bufferInfo, &allocInfo, &buffer, &allocation, nullptr));
        ::eng::ResourceTracker::incBuffer();
        size = dataSize;
        capacity = static_cast<uint32_t>(dataSize);
    }

    resources->ensureStagingBuffer(dataSize);
    void* stagingDst = nullptr;
    VK_CHECK(vmaMapMemory(resources->allocator, resources->transfer.stagingAlloc, &stagingDst));
    std::memcpy(stagingDst, data, dataSize);
    vmaUnmapMemory(resources->allocator, resources->transfer.stagingAlloc);

    VkCommandBuffer cmd = resources->beginSingleTimeCommands();
    resources->copyStagingToDevice(cmd, buffer, 0, dataSize);
    resources->endSingleTimeCommands(cmd);
}

void GPUScene::Initialize(EngineResources& resources)
{
    LOG_INFO("GPUScene: Initializing...");
    createDescriptorSetLayout(resources);

    // Create Local Lights Descriptor Set Layout
    {
        VkDescriptorSetLayoutBinding bindings[5]{};
        // Binding 0: Local Lights storage buffer
        bindings[0].binding            = 0;
        bindings[0].descriptorType     = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
        bindings[0].descriptorCount    = 1;
        bindings[0].stageFlags         = VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT;
        bindings[0].pImmutableSamplers = nullptr;

        // Binding 1: Cluster Bounds storage buffer
        bindings[1].binding            = 1;
        bindings[1].descriptorType     = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
        bindings[1].descriptorCount    = 1;
        bindings[1].stageFlags         = VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT;
        bindings[1].pImmutableSamplers = nullptr;

        // Binding 2: Cluster Ranges storage buffer
        bindings[2].binding            = 2;
        bindings[2].descriptorType     = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
        bindings[2].descriptorCount    = 1;
        bindings[2].stageFlags         = VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT;
        bindings[2].pImmutableSamplers = nullptr;

        // Binding 3: Cluster Light Indices storage buffer
        bindings[3].binding            = 3;
        bindings[3].descriptorType     = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
        bindings[3].descriptorCount    = 1;
        bindings[3].stageFlags         = VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT;
        bindings[3].pImmutableSamplers = nullptr;

        // Binding 4: Cluster Settings uniform buffer
        bindings[4].binding            = 4;
        bindings[4].descriptorType     = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
        bindings[4].descriptorCount    = 1;
        bindings[4].stageFlags         = VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT;
        bindings[4].pImmutableSamplers = nullptr;

        VkDescriptorSetLayoutCreateInfo layoutInfo{};
        layoutInfo.sType        = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
        layoutInfo.bindingCount = 5;
        layoutInfo.pBindings    = bindings;

        VK_CHECK(vkCreateDescriptorSetLayout(resources.device, &layoutInfo, nullptr, &m_LocalLightsDescriptorSetLayout));
    }

    // Create Light Culling Compute Descriptor Set Layout
    {
        VkDescriptorSetLayoutBinding bindings[5]{};
        // Binding 0: Cluster Settings uniform buffer
        bindings[0].binding            = 0;
        bindings[0].descriptorType     = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
        bindings[0].descriptorCount    = 1;
        bindings[0].stageFlags         = VK_SHADER_STAGE_COMPUTE_BIT;
        bindings[0].pImmutableSamplers = nullptr;

        // Binding 1: Lights storage buffer
        bindings[1].binding            = 1;
        bindings[1].descriptorType     = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
        bindings[1].descriptorCount    = 1;
        bindings[1].stageFlags         = VK_SHADER_STAGE_COMPUTE_BIT;
        bindings[1].pImmutableSamplers = nullptr;

        // Binding 2: Cluster Bounds storage buffer
        bindings[2].binding            = 2;
        bindings[2].descriptorType     = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
        bindings[2].descriptorCount    = 1;
        bindings[2].stageFlags         = VK_SHADER_STAGE_COMPUTE_BIT;
        bindings[2].pImmutableSamplers = nullptr;

        // Binding 3: Cluster Ranges storage buffer
        bindings[3].binding            = 3;
        bindings[3].descriptorType     = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
        bindings[3].descriptorCount    = 1;
        bindings[3].stageFlags         = VK_SHADER_STAGE_COMPUTE_BIT;
        bindings[3].pImmutableSamplers = nullptr;

        // Binding 4: Cluster Light Indices storage buffer
        bindings[4].binding            = 4;
        bindings[4].descriptorType     = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
        bindings[4].descriptorCount    = 1;
        bindings[4].stageFlags         = VK_SHADER_STAGE_COMPUTE_BIT;
        bindings[4].pImmutableSamplers = nullptr;

        VkDescriptorSetLayoutCreateInfo layoutInfo{};
        layoutInfo.sType        = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
        layoutInfo.bindingCount = 5;
        layoutInfo.pBindings    = bindings;

        VK_CHECK(vkCreateDescriptorSetLayout(resources.device, &layoutInfo, nullptr, &m_LightCullingDescriptorSetLayout));
    }

    m_Frames.resize(resources.MAX_FRAMES_IN_FLIGHT);
    for (uint32_t i = 0; i < resources.MAX_FRAMES_IN_FLIGHT; ++i) {
        createFrameResources(resources, m_Frames[i]);
    }
    LOG_INFO("GPUScene: Initialization complete.");
}

void GPUScene::Shutdown(EngineResources& resources)
{
    LOG_INFO("GPUScene: Shutting down...");
    for (uint32_t i = 0; i < resources.MAX_FRAMES_IN_FLIGHT; ++i) {
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
    LOG_INFO("GPUScene: Shutdown complete.");
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
)
{
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

    static bool loggedPlanes = false;
    if (!loggedPlanes) {
        loggedPlanes = true;
        LOG_INFO("GPUScene: Initial Frustum Planes Extracted:");
        const char* planeNames[] = { "Left", "Right", "Bottom", "Top", "Near", "Far" };
        for (int i = 0; i < 6; ++i) {
            LOG_INFO("  Plane " + std::string(planeNames[i]) + ": " +
                     "x=" + std::to_string(frustum.planes[i].x) + ", " +
                     "y=" + std::to_string(frustum.planes[i].y) + ", " +
                     "z=" + std::to_string(frustum.planes[i].z) + ", " +
                     "w=" + std::to_string(frustum.planes[i].w));
        }
    }

    // -------------------------------------------------------------------------
    // 2. Light Storage Buffer Upload
    // -------------------------------------------------------------------------
    LightData lightUboData{};
    // directional light
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

        // ShadowGPUSettings
        lightUboData.shadowSettings.shadowParams.x = dirLight.shadowStrength;
        lightUboData.shadowSettings.shadowParams.y = dirLight.shadowBias;
        lightUboData.shadowSettings.shadowParams.z = dirLight.shadowSlopeBias;
        lightUboData.shadowSettings.shadowParams.w = 1.0f; // softness default to 1.0f

        lightUboData.shadowSettings.shadowFlags.x = static_cast<uint32_t>(dirLight.pcfKernelSize);
        lightUboData.shadowSettings.shadowFlags.y = 0;
        lightUboData.shadowSettings.shadowFlags.z = 0;
        lightUboData.shadowSettings.shadowFlags.w = 0;
    } else {
        lightUboData.directionalDirectionIntensity = glm::vec4(0.0f, -1.0f, 0.0f, 0.0f);
        lightUboData.directionalColor = glm::vec4(1.0f, 1.0f, 1.0f, 1.0f);
        lightUboData.directionalLightProjView = glm::mat4(1.0f);
        lightUboData.shadowBias = 0.003f;
        lightUboData.shadowNormalBias = 0.0f;
        lightUboData.shadowSlopeBias = 0.01f;
        lightUboData.shadowStrength = 1.0f;
        lightUboData.shadowLightCast = 0;
        lightUboData.pcfKernelSize = 3;
        lightUboData.shadowResolution = 2048;

        // ShadowGPUSettings fallback
        lightUboData.shadowSettings.shadowParams = glm::vec4(1.0f, 0.003f, 0.01f, 1.0f);
        lightUboData.shadowSettings.shadowFlags = glm::uvec4(3, 0, 0, 0);
    }
    // ambient
    lightUboData.ambientColorIntensity = glm::vec4(renderScene.skyLight.color, renderScene.skyLight.intensity);
    // point lights
    lightUboData.pointLightCount = std::min(static_cast<uint32_t>(renderScene.pointLights.size()), 16u);
    for (uint32_t i = 0; i < lightUboData.pointLightCount; ++i) {
        const auto& pt = renderScene.pointLights[i];
        lightUboData.pointPositionsRadius[i] = glm::vec4(pt.position, pt.radius);
        lightUboData.pointColorsIntensity[i] = glm::vec4(pt.color, pt.intensity);
    }
    // spot lights
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

    // Register default material at index 0
    MaterialGPUData defaultMatData{};
    if (defaultMaterial) {
        defaultMatData = defaultMaterial->uboData;
    }
    materialsData.push_back(defaultMatData);
    if (defaultMaterial) {
        materialToIndex[defaultMaterial] = 0;
    }

    // Scan render queue to register used materials
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

    // Resize material buffer if needed
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

    // Material buffer upload via safe staging path (device-local copy)
    VkDeviceSize matDataSize = materialCount * sizeof(MaterialGPUData);
    resources.ensureStagingBuffer(matDataSize);
    void* stagingDst = nullptr;
    VK_CHECK(vmaMapMemory(resources.allocator, resources.transfer.stagingAlloc, &stagingDst));
    std::memcpy(stagingDst, materialsData.data(), matDataSize);
    vmaUnmapMemory(resources.allocator, resources.transfer.stagingAlloc);

    VkCommandBuffer cmd = resources.beginSingleTimeCommands();
    resources.copyStagingToDevice(cmd, frameRes.materialBuffer, 0, matDataSize);
    resources.endSingleTimeCommands(cmd);

    // Register unique meshes to assign meshIndex
    std::unordered_map<const Mesh*, uint32_t> meshToIndex;
    std::vector<const Mesh*> uniqueMeshes;
    for (const auto& item : renderQueueItems) {
        if (item.mesh && meshToIndex.find(item.mesh) == meshToIndex.end()) {
            meshToIndex[item.mesh] = static_cast<uint32_t>(uniqueMeshes.size());
            uniqueMeshes.push_back(item.mesh);
        }
    }

    // -------------------------------------------------------------------------
    // 4. Build Instance & ObjectID Arrays
    // -------------------------------------------------------------------------
    std::vector<GPUInstance> instancesData;
    std::vector<uint32_t> objectIDsData;

    instancesData.reserve(renderQueueItems.size());
    objectIDsData.reserve(renderQueueItems.size());

    for (const auto& item : renderQueueItems) {
        uint32_t matIdx = 0;
        if (item.material) {
            auto it = materialToIndex.find(item.material);
            if (it != materialToIndex.end()) {
                matIdx = it->second;
            }
        }

        uint32_t meshIdx = 0;
        if (item.mesh) {
            auto it = meshToIndex.find(item.mesh);
            if (it != meshToIndex.end()) {
                meshIdx = it->second;
            }
        }

        GPUInstance inst{};
        inst.worldMatrix = item.transform;
        inst.previousWorldMatrix = item.previousTransform;
        inst.boundsCenterRadius = ComputeWorldBoundsSphere(item.transform, item.mesh ? item.mesh->bounds : MeshBounds{});
        inst.meshIndex = meshIdx;
        inst.materialIndex = matIdx;
        inst.objectID = item.entityID;
        inst.flags = RenderFlags_Opaque;

        instancesData.push_back(inst);
        objectIDsData.push_back(item.entityID);
    }

    // Default to at least 1 element to avoid 0-sized allocation if queue is empty
    if (instancesData.empty()) {
        instancesData.push_back(GPUInstance{});
        objectIDsData.push_back(0);
    }

    uint32_t instanceCount = static_cast<uint32_t>(instancesData.size());

    // Resize instance buffer if needed
    uint32_t prevInstanceCapacity = frameRes.instanceCapacity;
    resizeBufferIfNeeded(
        resources,
        frameRes.instanceBuffer,
        frameRes.instanceAlloc,
        frameRes.instanceBufferSize,
        frameRes.instanceCapacity,
        instanceCount,
        sizeof(GPUInstance),
        VK_BUFFER_USAGE_STORAGE_BUFFER_BIT
    );
    if (frameRes.instanceCapacity != prevInstanceCapacity) {
        needsDescriptorUpdate = true;
    }

    // Persistently mapped path for Instance Buffer
    void* instanceDst = nullptr;
    VK_CHECK(vmaMapMemory(resources.allocator, frameRes.instanceAlloc, &instanceDst));
    std::memcpy(instanceDst, instancesData.data(), instanceCount * sizeof(GPUInstance));
    vmaUnmapMemory(resources.allocator, frameRes.instanceAlloc);

    m_GPUInstances = instancesData;

    // Resize ObjectID buffer if needed
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

    // Persistently mapped path for ObjectID Buffer
    void* objectIdDst = nullptr;
    VK_CHECK(vmaMapMemory(resources.allocator, frameRes.objectIdAlloc, &objectIdDst));
    std::memcpy(objectIdDst, objectIDsData.data(), instanceCount * sizeof(uint32_t));
    vmaUnmapMemory(resources.allocator, frameRes.objectIdAlloc);

    // -------------------------------------------------------------------------
    // 4a. Populate and Upload Mesh Draw Metadata
    // -------------------------------------------------------------------------
    std::vector<GPUMeshDrawData> meshDrawData;
    meshDrawData.reserve(uniqueMeshes.size());
    for (const Mesh* mesh : uniqueMeshes) {
        GPUMeshDrawData draw{};
        draw.indexCount = mesh->indexCount;
        draw.firstIndex = mesh->firstIndex;
        draw.vertexOffset = mesh->vertexOffset;
        draw.materialSlotOffset = mesh->materialSlotOffset;
        meshDrawData.push_back(draw);
    }
    if (meshDrawData.empty()) {
        meshDrawData.push_back(GPUMeshDrawData{});
    }

    uint32_t meshCount = static_cast<uint32_t>(meshDrawData.size());
    uint32_t prevMeshDrawDataCapacity = frameRes.meshDrawDataCapacity;
    resizeBufferIfNeeded(
        resources,
        frameRes.meshDrawDataBuffer,
        frameRes.meshDrawDataAlloc,
        frameRes.meshDrawDataBufferSize,
        frameRes.meshDrawDataCapacity,
        meshCount,
        sizeof(GPUMeshDrawData),
        VK_BUFFER_USAGE_STORAGE_BUFFER_BIT
    );
    if (frameRes.meshDrawDataCapacity != prevMeshDrawDataCapacity) {
        needsDescriptorUpdate = true;
    }

    void* meshDrawDataDst = nullptr;
    VK_CHECK(vmaMapMemory(resources.allocator, frameRes.meshDrawDataAlloc, &meshDrawDataDst));
    std::memcpy(meshDrawDataDst, meshDrawData.data(), meshCount * sizeof(GPUMeshDrawData));
    vmaUnmapMemory(resources.allocator, frameRes.meshDrawDataAlloc);

    m_GPUMeshDrawData = meshDrawData;

    // -------------------------------------------------------------------------
    // 4b. Local Lights Extraction and Upload
    // -------------------------------------------------------------------------
    std::vector<Omnix::Radiance::LocalLightGPU> localLights;
    if (activeScene) {
        RenderSceneExtractor::ExtractLocalLights(*activeScene, localLights);
    }
    frameRes.localLightCount = static_cast<uint32_t>(localLights.size());

    VkBuffer oldLocalBuffer = frameRes.localLightBuffer.buffer;
    frameRes.localLightBuffer.Upload(
        localLights.data(),
        localLights.size() * sizeof(Omnix::Radiance::LocalLightGPU)
    );
    if (frameRes.localLightBuffer.buffer != oldLocalBuffer) {
        needsDescriptorUpdate = true;
    }
    LOG_INFO("Local Lights uploaded: " + std::to_string(localLights.size()));

    // -------------------------------------------------------------------------
    // 4c. Clustered Lighting Calculations & Upload
    // -------------------------------------------------------------------------
    Omnix::Radiance::ClusterSettings settings{};
    uint32_t viewportWidth = static_cast<uint32_t>(radianceUBO.viewportSize.x);
    uint32_t viewportHeight = static_cast<uint32_t>(radianceUBO.viewportSize.y);
    if (viewportWidth == 0) viewportWidth = 1280;
    if (viewportHeight == 0) viewportHeight = 720;

    uint32_t tileCountX = (viewportWidth + settings.tileSizeX - 1) / settings.tileSizeX;
    uint32_t tileCountY = (viewportHeight + settings.tileSizeY - 1) / settings.tileSizeY;
    uint32_t depthSliceCount = settings.depthSliceCount;
    uint32_t maxLightsPerCluster = settings.maxLightsPerCluster;
    uint32_t clusterCount = tileCountX * tileCountY * depthSliceCount;

    // Resize cluster buffers if needed
    VkBuffer oldBoundsBuffer = frameRes.clusterBoundsBuffer;
    resizeBufferIfNeeded(
        resources,
        frameRes.clusterBoundsBuffer,
        frameRes.clusterBoundsAlloc,
        frameRes.clusterBoundsBufferSize,
        frameRes.clusterBoundsCapacity,
        clusterCount,
        sizeof(Omnix::Radiance::ClusterBoundsGPU),
        VK_BUFFER_USAGE_STORAGE_BUFFER_BIT
    );
    if (frameRes.clusterBoundsBuffer != oldBoundsBuffer) {
        needsDescriptorUpdate = true;
    }

    VkBuffer oldRangeBuffer = frameRes.clusterRangeBuffer;
    resizeBufferIfNeeded(
        resources,
        frameRes.clusterRangeBuffer,
        frameRes.clusterRangeAlloc,
        frameRes.clusterRangeBufferSize,
        frameRes.clusterRangeCapacity,
        clusterCount,
        sizeof(Omnix::Radiance::ClusterRangeGPU),
        VK_BUFFER_USAGE_STORAGE_BUFFER_BIT
    );
    if (frameRes.clusterRangeBuffer != oldRangeBuffer) {
        needsDescriptorUpdate = true;
    }

    VkBuffer oldIndexBuffer = frameRes.clusterLightIndexBuffer;
    resizeBufferIfNeeded(
        resources,
        frameRes.clusterLightIndexBuffer,
        frameRes.clusterLightIndexAlloc,
        frameRes.clusterLightIndexBufferSize,
        frameRes.clusterLightIndexCapacity,
        clusterCount * maxLightsPerCluster,
        sizeof(uint32_t),
        VK_BUFFER_USAGE_STORAGE_BUFFER_BIT
    );
    if (frameRes.clusterLightIndexBuffer != oldIndexBuffer) {
        needsDescriptorUpdate = true;
    }

    // Build cluster bounds on CPU
    float nearPlane = renderScene.camera.nearPlane;
    float farPlane = renderScene.camera.farPlane;
    if (nearPlane <= 0.001f) nearPlane = 0.1f;
    if (farPlane <= nearPlane) farPlane = 1000.0f;

    struct LocalBoundsBuilder {
        static std::vector<Omnix::Radiance::ClusterBoundsGPU> Build(
            uint32_t tileCountX,
            uint32_t tileCountY,
            uint32_t depthSlices,
            float nearP,
            float farP)
        {
            std::vector<Omnix::Radiance::ClusterBoundsGPU> clusters;
            clusters.resize(tileCountX * tileCountY * depthSlices);

            for (uint32_t z = 0; z < depthSlices; z++)
            {
                float z0 = nearP + (farP - nearP) * (float(z) / float(depthSlices));
                float z1 = nearP + (farP - nearP) * (float(z + 1) / float(depthSlices));

                for (uint32_t y = 0; y < tileCountY; y++)
                {
                    for (uint32_t x = 0; x < tileCountX; x++)
                    {
                        uint32_t index =
                            x +
                            y * tileCountX +
                            z * tileCountX * tileCountY;

                        clusters[index].minPoint = glm::vec4(
                            float(x),
                            float(y),
                            z0,
                            0.0f
                        );

                        clusters[index].maxPoint = glm::vec4(
                            float(x + 1),
                            float(y + 1),
                            z1,
                            0.0f
                        );
                    }
                }
            }

            return clusters;
        }
    };

    std::vector<Omnix::Radiance::ClusterBoundsGPU> clusterBounds = LocalBoundsBuilder::Build(
        tileCountX,
        tileCountY,
        depthSliceCount,
        nearPlane,
        farPlane
    );

    // Upload Cluster Bounds
    void* boundsDst = nullptr;
    VK_CHECK(vmaMapMemory(resources.allocator, frameRes.clusterBoundsAlloc, &boundsDst));
    std::memcpy(boundsDst, clusterBounds.data(), clusterBounds.size() * sizeof(Omnix::Radiance::ClusterBoundsGPU));
    vmaUnmapMemory(resources.allocator, frameRes.clusterBoundsAlloc);

    // CPU light-to-cluster assignment and CPU buffer copies are removed.
    // The light_culling.comp compute shader executes on the GPU and populates
    // clusterRangeBuffer and clusterLightIndexBuffer dynamically per-frame.

    // Upload Cluster Settings
    Omnix::Radiance::ClusterSettingsGPU settingsGPU{};
    settingsGPU.tileCountX = tileCountX;
    settingsGPU.tileCountY = tileCountY;
    settingsGPU.depthSliceCount = depthSliceCount;
    settingsGPU.maxLightsPerCluster = maxLightsPerCluster;
    settingsGPU.clusterCount = clusterCount;
    settingsGPU.lightCount = static_cast<uint32_t>(localLights.size());
    settingsGPU.nearPlane = nearPlane;
    settingsGPU.farPlane = farPlane;

    void* settingsDst = nullptr;
    VK_CHECK(vmaMapMemory(resources.allocator, frameRes.clusterSettingsAlloc, &settingsDst));
    std::memcpy(settingsDst, &settingsGPU, sizeof(Omnix::Radiance::ClusterSettingsGPU));
    vmaUnmapMemory(resources.allocator, frameRes.clusterSettingsAlloc);

    // -------------------------------------------------------------------------
    // 5. Update Descriptor Set if any buffer was resized
    // -------------------------------------------------------------------------
    if (needsDescriptorUpdate) {
        writeDescriptorSet(resources, frameRes);
    }
}

void GPUScene::createDescriptorSetLayout(EngineResources& resources)
{
    VkDescriptorSetLayoutBinding bindings[7]{};

    // Binding 0: Camera Uniform Buffer
    bindings[0].binding            = 0;
    bindings[0].descriptorType     = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
    bindings[0].descriptorCount    = 1;
    bindings[0].stageFlags         = VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT;
    bindings[0].pImmutableSamplers = nullptr;

    // Binding 1: Instance Storage Buffer
    bindings[1].binding            = 1;
    bindings[1].descriptorType     = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
    bindings[1].descriptorCount    = 1;
    bindings[1].stageFlags         = VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT;
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

    VkDescriptorSetLayoutCreateInfo layoutInfo{};
    layoutInfo.sType        = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
    layoutInfo.bindingCount = 7;
    layoutInfo.pBindings    = bindings;

    VK_CHECK(vkCreateDescriptorSetLayout(resources.device, &layoutInfo, nullptr, &m_DescriptorSetLayout));
}

void GPUScene::createFrameResources(EngineResources& resources, GPUSceneFrameResources& frameRes)
{
    // Allocate descriptor pool
    VkDescriptorPoolSize poolSizes[2]{};
    poolSizes[0].type            = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
    poolSizes[0].descriptorCount = 4; // cameraBuffer (set 0), clusterSettingsBuffer (set 3), compute settings (compute set 0), frustumBuffer (set 0)
    poolSizes[1].type            = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
    poolSizes[1].descriptorCount = 20; // set 0: 5 storage buffers. set 3: 4 storage buffers. compute set 0: 4 storage buffers.

    VkDescriptorPoolCreateInfo poolInfo{};
    poolInfo.sType         = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
    poolInfo.poolSizeCount = 2;
    poolInfo.pPoolSizes    = poolSizes;
    poolInfo.maxSets       = 3; // set 0, set 3, and compute set 0
    poolInfo.flags         = VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT;

    VK_CHECK(vkCreateDescriptorPool(resources.device, &poolInfo, nullptr, &frameRes.descriptorPool));

    // Allocate descriptor set 0
    VkDescriptorSetAllocateInfo setAlloc{};
    setAlloc.sType              = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
    setAlloc.descriptorPool     = frameRes.descriptorPool;
    setAlloc.descriptorSetCount = 1;
    setAlloc.pSetLayouts        = &m_DescriptorSetLayout;

    VK_CHECK(vkAllocateDescriptorSets(resources.device, &setAlloc, &frameRes.descriptorSet));

    // Allocate descriptor set 3 (local lights and clusters)
    VkDescriptorSetAllocateInfo localSetAlloc{};
    localSetAlloc.sType              = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
    localSetAlloc.descriptorPool     = frameRes.descriptorPool;
    localSetAlloc.descriptorSetCount = 1;
    localSetAlloc.pSetLayouts        = &m_LocalLightsDescriptorSetLayout;

    VK_CHECK(vkAllocateDescriptorSets(resources.device, &localSetAlloc, &frameRes.localLightsDescriptorSet));

    // Allocate descriptor set for light culling compute shader
    VkDescriptorSetAllocateInfo cullingSetAlloc{};
    cullingSetAlloc.sType              = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
    cullingSetAlloc.descriptorPool     = frameRes.descriptorPool;
    cullingSetAlloc.descriptorSetCount = 1;
    cullingSetAlloc.pSetLayouts        = &m_LightCullingDescriptorSetLayout;

    VK_CHECK(vkAllocateDescriptorSets(resources.device, &cullingSetAlloc, &frameRes.lightCullingDescriptorSet));

    // Allocate Camera Uniform Buffer
    VkBufferCreateInfo bufInfo{};
    bufInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
    bufInfo.size  = sizeof(Omnix::Radiance::RadianceFrameUBO);
    bufInfo.usage = VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT;
    bufInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

    VmaAllocationCreateInfo vmaAllocInfo{};
    vmaAllocInfo.usage = VMA_MEMORY_USAGE_CPU_TO_GPU;

    VK_CHECK(vmaCreateBuffer(resources.allocator, &bufInfo, &vmaAllocInfo, &frameRes.cameraBuffer, &frameRes.cameraAlloc, nullptr));
    ::eng::ResourceTracker::incBuffer();

    // Allocate Frustum Uniform Buffer
    bufInfo.size  = sizeof(GPUFrustum);
    bufInfo.usage = VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT;
    VK_CHECK(vmaCreateBuffer(resources.allocator, &bufInfo, &vmaAllocInfo, &frameRes.frustumBuffer, &frameRes.frustumAlloc, nullptr));
    ::eng::ResourceTracker::incBuffer();

    // Allocate Light Storage Buffer
    bufInfo.size  = sizeof(LightData);
    bufInfo.usage = VK_BUFFER_USAGE_STORAGE_BUFFER_BIT;
    VK_CHECK(vmaCreateBuffer(resources.allocator, &bufInfo, &vmaAllocInfo, &frameRes.lightBuffer, &frameRes.lightAlloc, nullptr));
    ::eng::ResourceTracker::incBuffer();

    // Initialize dynamic buffers to a default capacity
    frameRes.instanceCapacity = 128;
    frameRes.instanceBufferSize = frameRes.instanceCapacity * sizeof(GPUInstance);
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
    frameRes.meshDrawDataBufferSize = frameRes.meshDrawDataCapacity * sizeof(GPUMeshDrawData);
    bufInfo.size = frameRes.meshDrawDataBufferSize;
    bufInfo.usage = VK_BUFFER_USAGE_STORAGE_BUFFER_BIT;
    VK_CHECK(vmaCreateBuffer(resources.allocator, &bufInfo, &vmaAllocInfo, &frameRes.meshDrawDataBuffer, &frameRes.meshDrawDataAlloc, nullptr));
    ::eng::ResourceTracker::incBuffer();

    // Material buffer (Device-local staging upload)
    frameRes.materialCapacity = 128;
    frameRes.materialBufferSize = frameRes.materialCapacity * sizeof(MaterialGPUData);
    bufInfo.size = frameRes.materialBufferSize;
    bufInfo.usage = VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT;
    
    VmaAllocationCreateInfo gpuAllocInfo{};
    gpuAllocInfo.usage = VMA_MEMORY_USAGE_GPU_ONLY;
    VK_CHECK(vmaCreateBuffer(resources.allocator, &bufInfo, &gpuAllocInfo, &frameRes.materialBuffer, &frameRes.materialAlloc, nullptr));
    ::eng::ResourceTracker::incBuffer();

    // Initialize localLightBuffer dynamic storage buffer to a default capacity
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

    // Initialize cluster bounds, ranges, indices, settings buffers to default size 1
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

    // Write descriptor set initially
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
)
{
    if (buffer != VK_NULL_HANDLE && currentCapacity >= neededCapacity) {
        return;
    }

    uint32_t newCapacity = std::max(currentCapacity * 2, 128u);
    while (newCapacity < neededCapacity) {
        newCapacity *= 2;
    }

    VkDeviceSize newSize = newCapacity * elementSize;

    if (buffer != VK_NULL_HANDLE) {
        vmaDestroyBuffer(resources.allocator, buffer, allocation);
        ::eng::ResourceTracker::decBuffer();
        buffer = VK_NULL_HANDLE;
    }

    VkBufferCreateInfo bufInfo{};
    bufInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
    bufInfo.size  = newSize;
    bufInfo.usage = usage;
    bufInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

    VmaAllocationCreateInfo allocInfo{};
    if (usage & VK_BUFFER_USAGE_TRANSFER_DST_BIT) {
        allocInfo.usage = VMA_MEMORY_USAGE_GPU_ONLY;
    } else {
        allocInfo.usage = VMA_MEMORY_USAGE_CPU_TO_GPU;
    }

    VkResult res = vmaCreateBuffer(resources.allocator, &bufInfo, &allocInfo, &buffer, &allocation, nullptr);
    VK_CHECK(res);
    ::eng::ResourceTracker::incBuffer();

    currentCapacity = newCapacity;
    currentSize = newSize;

    LOG_INFO("GPUScene: Resized buffer (usage " + std::to_string(usage) + ") to capacity " + std::to_string(newCapacity) + " (" + std::to_string(newSize) + " bytes)");
}

void GPUScene::writeDescriptorSet(EngineResources& resources, GPUSceneFrameResources& frameRes)
{
    std::vector<VkWriteDescriptorSet> writes;
    writes.reserve(7);

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

    // Write local lights descriptor set if buffer exists
    VkDescriptorBufferInfo localLightInfo{};
    VkDescriptorBufferInfo clusterBoundsInfo{};
    VkDescriptorBufferInfo clusterRangeInfo{};
    VkDescriptorBufferInfo clusterLightIndexInfo{};
    VkDescriptorBufferInfo clusterSettingsInfo{};

    std::vector<VkWriteDescriptorSet> localSetWrites;

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

    writes.insert(writes.end(), localSetWrites.begin(), localSetWrites.end());

    // Write light culling compute descriptor set if it exists
    if (frameRes.lightCullingDescriptorSet != VK_NULL_HANDLE) {
        static VkDescriptorBufferInfo csInfo{};
        csInfo.buffer = frameRes.clusterSettingsBuffer;
        csInfo.offset = 0;
        csInfo.range  = sizeof(Omnix::Radiance::ClusterSettingsGPU);

        static VkDescriptorBufferInfo lInfo{};
        lInfo.buffer = frameRes.localLightBuffer.buffer;
        lInfo.offset = 0;
        lInfo.range  = frameRes.localLightBuffer.size;

        static VkDescriptorBufferInfo cbInfo{};
        cbInfo.buffer = frameRes.clusterBoundsBuffer;
        cbInfo.offset = 0;
        cbInfo.range  = frameRes.clusterBoundsBufferSize;

        static VkDescriptorBufferInfo crInfo{};
        crInfo.buffer = frameRes.clusterRangeBuffer;
        crInfo.offset = 0;
        crInfo.range  = frameRes.clusterRangeBufferSize;

        static VkDescriptorBufferInfo cliInfo{};
        cliInfo.buffer = frameRes.clusterLightIndexBuffer;
        cliInfo.offset = 0;
        cliInfo.range  = frameRes.clusterLightIndexBufferSize;

        VkWriteDescriptorSet cullingWrites[5]{};

        cullingWrites[0].sType           = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        cullingWrites[0].dstSet          = frameRes.lightCullingDescriptorSet;
        cullingWrites[0].dstBinding      = 0;
        cullingWrites[0].dstArrayElement = 0;
        cullingWrites[0].descriptorCount = 1;
        cullingWrites[0].descriptorType  = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
        cullingWrites[0].pBufferInfo     = &csInfo;

        cullingWrites[1].sType           = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        cullingWrites[1].dstSet          = frameRes.lightCullingDescriptorSet;
        cullingWrites[1].dstBinding      = 1;
        cullingWrites[1].dstArrayElement = 0;
        cullingWrites[1].descriptorCount = 1;
        cullingWrites[1].descriptorType  = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
        cullingWrites[1].pBufferInfo     = &lInfo;

        cullingWrites[2].sType           = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        cullingWrites[2].dstSet          = frameRes.lightCullingDescriptorSet;
        cullingWrites[2].dstBinding      = 2;
        cullingWrites[2].dstArrayElement = 0;
        cullingWrites[2].descriptorCount = 1;
        cullingWrites[2].descriptorType  = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
        cullingWrites[2].pBufferInfo     = &cbInfo;

        cullingWrites[3].sType           = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        cullingWrites[3].dstSet          = frameRes.lightCullingDescriptorSet;
        cullingWrites[3].dstBinding      = 3;
        cullingWrites[3].dstArrayElement = 0;
        cullingWrites[3].descriptorCount = 1;
        cullingWrites[3].descriptorType  = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
        cullingWrites[3].pBufferInfo     = &crInfo;

        cullingWrites[4].sType           = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        cullingWrites[4].dstSet          = frameRes.lightCullingDescriptorSet;
        cullingWrites[4].dstBinding      = 4;
        cullingWrites[4].dstArrayElement = 0;
        cullingWrites[4].descriptorCount = 1;
        cullingWrites[4].descriptorType  = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
        cullingWrites[4].pBufferInfo     = &cliInfo;

        for (int i = 0; i < 5; ++i) {
            writes.push_back(cullingWrites[i]);
        }
    }

    vkUpdateDescriptorSets(resources.device, static_cast<uint32_t>(writes.size()), writes.data(), 0, nullptr);
}

} // namespace eng::renderer
