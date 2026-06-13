#include "Core/pch.h"
#include "GPUScene.h"
#include "Core/Vulkan/VkUtils.h"
#include "Core/Engine/Log.h"
#include "Core/Engine/VmaHelpers.h"
#include "Core/Engine/ResourceTracker.h"
#include <cstring>
#include <algorithm>

namespace eng::renderer {

void GPUScene::Initialize(EngineResources& resources)
{
    LOG_INFO("GPUScene: Initializing...");
    createDescriptorSetLayout(resources);

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
    LOG_INFO("GPUScene: Shutdown complete.");
}

void GPUScene::UpdateFrame(
    EngineResources& resources,
    uint32_t frameIndex,
    const RenderScene& renderScene,
    const std::vector<RenderItem>& renderQueueItems,
    const std::unordered_map<uint64_t, Material*>& ecsMaterialCache,
    const Material* defaultMaterial,
    uint32_t shadingMode
)
{
    GPUSceneFrameResources& frameRes = m_Frames[frameIndex];
    bool needsDescriptorUpdate = false;

    // -------------------------------------------------------------------------
    // 1. Camera Uniform Buffer Upload
    // -------------------------------------------------------------------------
    float aspect = renderScene.camera.aspectRatio;
    CameraGPUData camGPU{};
    camGPU.view = renderScene.camera.viewMatrix;
    camGPU.proj = glm::perspective(glm::radians(renderScene.camera.fov), aspect, renderScene.camera.nearPlane, renderScene.camera.farPlane);
    camGPU.proj[1][1] *= -1.0f;
    camGPU.cameraPos = glm::vec4(renderScene.camera.position, renderScene.camera.fov);
    camGPU.cameraPlanes = glm::vec4(renderScene.camera.nearPlane, renderScene.camera.farPlane, renderScene.camera.exposure, static_cast<float>(renderScene.camera.selectedEntityID));

    void* cameraDst = nullptr;
    VK_CHECK(vmaMapMemory(resources.allocator, frameRes.cameraAlloc, &cameraDst));
    std::memcpy(cameraDst, &camGPU, sizeof(camGPU));
    vmaUnmapMemory(resources.allocator, frameRes.cameraAlloc);

    // -------------------------------------------------------------------------
    // 2. Light Storage Buffer Upload
    // -------------------------------------------------------------------------
    LightData lightUboData{};
    // directional light
    if (!renderScene.directionalLights.empty()) {
        const auto& dirLight = renderScene.directionalLights[0];
        lightUboData.directionalDirectionIntensity = glm::vec4(dirLight.direction, dirLight.intensity);
        lightUboData.directionalColor = glm::vec4(dirLight.color, 1.0f);
        lightUboData.lightSpaceMatrix = dirLight.lightSpaceMatrix;
        lightUboData.shadowBias = dirLight.shadowBias;
        lightUboData.shadowNormalBias = dirLight.shadowNormalBias;
        lightUboData.shadowStrength = dirLight.shadowStrength;
        lightUboData.shadowLightCast = dirLight.castShadows > 0.0f ? 1 : 0;
    } else {
        lightUboData.directionalDirectionIntensity = glm::vec4(0.0f, -1.0f, 0.0f, 0.0f);
        lightUboData.directionalColor = glm::vec4(1.0f, 1.0f, 1.0f, 1.0f);
        lightUboData.lightSpaceMatrix = glm::mat4(1.0f);
        lightUboData.shadowBias = 0.005f;
        lightUboData.shadowNormalBias = 0.015f;
        lightUboData.shadowStrength = 1.0f;
        lightUboData.shadowLightCast = 0;
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
        if (mat && materialToIndex.find(mat) == materialToIndex.end()) {
            MaterialGPUData matData{};
            if (mat) {
                matData = mat->uboData;
            }

            materialToIndex[mat] = static_cast<uint32_t>(materialsData.size());
            materialsData.push_back(matData);
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

    // -------------------------------------------------------------------------
    // 4. Build Instance & ObjectID Arrays
    // -------------------------------------------------------------------------
    std::vector<InstanceGPUData> instancesData;
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

        InstanceGPUData inst{};
        inst.worldMatrix = item.transform;
        inst.previousWorldMatrix = item.previousTransform;
        inst.minBounds_materialIndex = glm::vec4(item.minBounds, static_cast<float>(matIdx));
        inst.maxBounds_entityID = glm::vec4(item.maxBounds, static_cast<float>(item.entityID));

        instancesData.push_back(inst);
        objectIDsData.push_back(item.entityID);
    }

    // Default to at least 1 element to avoid 0-sized allocation if queue is empty
    if (instancesData.empty()) {
        instancesData.push_back(InstanceGPUData{});
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
        sizeof(InstanceGPUData),
        VK_BUFFER_USAGE_STORAGE_BUFFER_BIT
    );
    if (frameRes.instanceCapacity != prevInstanceCapacity) {
        needsDescriptorUpdate = true;
    }

    // Persistently mapped path for Instance Buffer
    void* instanceDst = nullptr;
    VK_CHECK(vmaMapMemory(resources.allocator, frameRes.instanceAlloc, &instanceDst));
    std::memcpy(instanceDst, instancesData.data(), instanceCount * sizeof(InstanceGPUData));
    vmaUnmapMemory(resources.allocator, frameRes.instanceAlloc);

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
    // 5. Update Descriptor Set if any buffer was resized
    // -------------------------------------------------------------------------
    if (needsDescriptorUpdate) {
        writeDescriptorSet(resources, frameRes);
    }
}

void GPUScene::createDescriptorSetLayout(EngineResources& resources)
{
    VkDescriptorSetLayoutBinding bindings[5]{};

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

    VkDescriptorSetLayoutCreateInfo layoutInfo{};
    layoutInfo.sType        = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
    layoutInfo.bindingCount = 5;
    layoutInfo.pBindings    = bindings;

    VK_CHECK(vkCreateDescriptorSetLayout(resources.device, &layoutInfo, nullptr, &m_DescriptorSetLayout));
}

void GPUScene::createFrameResources(EngineResources& resources, GPUSceneFrameResources& frameRes)
{
    // Allocate descriptor pool
    VkDescriptorPoolSize poolSizes[2]{};
    poolSizes[0].type            = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
    poolSizes[0].descriptorCount = 1;
    poolSizes[1].type            = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
    poolSizes[1].descriptorCount = 4;

    VkDescriptorPoolCreateInfo poolInfo{};
    poolInfo.sType         = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
    poolInfo.poolSizeCount = 2;
    poolInfo.pPoolSizes    = poolSizes;
    poolInfo.maxSets       = 1;
    poolInfo.flags         = VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT;

    VK_CHECK(vkCreateDescriptorPool(resources.device, &poolInfo, nullptr, &frameRes.descriptorPool));

    // Allocate descriptor set
    VkDescriptorSetAllocateInfo setAlloc{};
    setAlloc.sType              = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
    setAlloc.descriptorPool     = frameRes.descriptorPool;
    setAlloc.descriptorSetCount = 1;
    setAlloc.pSetLayouts        = &m_DescriptorSetLayout;

    VK_CHECK(vkAllocateDescriptorSets(resources.device, &setAlloc, &frameRes.descriptorSet));

    // Allocate Camera Uniform Buffer (256 bytes)
    VkBufferCreateInfo bufInfo{};
    bufInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
    bufInfo.size  = 256;
    bufInfo.usage = VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT;
    bufInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

    VmaAllocationCreateInfo vmaAllocInfo{};
    vmaAllocInfo.usage = VMA_MEMORY_USAGE_CPU_TO_GPU;

    VK_CHECK(vmaCreateBuffer(resources.allocator, &bufInfo, &vmaAllocInfo, &frameRes.cameraBuffer, &frameRes.cameraAlloc, nullptr));
    ::eng::ResourceTracker::incBuffer();

    // Allocate Light Storage Buffer
    bufInfo.size  = sizeof(LightData);
    bufInfo.usage = VK_BUFFER_USAGE_STORAGE_BUFFER_BIT;
    VK_CHECK(vmaCreateBuffer(resources.allocator, &bufInfo, &vmaAllocInfo, &frameRes.lightBuffer, &frameRes.lightAlloc, nullptr));
    ::eng::ResourceTracker::incBuffer();

    // Initialize dynamic buffers to a default capacity
    frameRes.instanceCapacity = 128;
    frameRes.instanceBufferSize = frameRes.instanceCapacity * sizeof(InstanceGPUData);
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

    // Material buffer (Device-local staging upload)
    frameRes.materialCapacity = 128;
    frameRes.materialBufferSize = frameRes.materialCapacity * sizeof(MaterialGPUData);
    bufInfo.size = frameRes.materialBufferSize;
    bufInfo.usage = VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT;
    
    VmaAllocationCreateInfo gpuAllocInfo{};
    gpuAllocInfo.usage = VMA_MEMORY_USAGE_GPU_ONLY;
    VK_CHECK(vmaCreateBuffer(resources.allocator, &bufInfo, &gpuAllocInfo, &frameRes.materialBuffer, &frameRes.materialAlloc, nullptr));
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
    writes.reserve(5);

    // Binding 0: Camera Uniform Buffer
    VkDescriptorBufferInfo cameraInfo{};
    cameraInfo.buffer = frameRes.cameraBuffer;
    cameraInfo.offset = 0;
    cameraInfo.range  = sizeof(CameraGPUData);

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

    vkUpdateDescriptorSets(resources.device, static_cast<uint32_t>(writes.size()), writes.data(), 0, nullptr);
}

} // namespace eng::renderer
