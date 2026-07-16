#include "Core/pch.h"
#include "FrustumCullPass.h"
#include "Rendering/GPUScene/GPUScene.h"
#include "Core/Vulkan/VkUtils.h"
#include "Core/Engine/Log.h"
#include "Core/Engine/VmaHelpers.h"
#include "Core/Engine/ResourceTracker.h"
#include <cstring>
#include <algorithm>
#include <array>

namespace eng::renderer {

struct CullPushConstants {
    uint32_t instanceCount;
};

void FrustumCullPass::Initialize(EngineResources& resources)
{
    LOG_INFO("FrustumCullPass: Initializing...");

    // 1. Create Descriptor Set Layout
    VkDescriptorSetLayoutBinding bindings[4]{};
    
    // Binding 0: InstanceBuffer (Storage)
    bindings[0].binding            = 0;
    bindings[0].descriptorType     = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
    bindings[0].descriptorCount    = 1;
    bindings[0].stageFlags         = VK_SHADER_STAGE_COMPUTE_BIT | VK_SHADER_STAGE_VERTEX_BIT;
    bindings[0].pImmutableSamplers = nullptr;

    // Binding 1: FrustumBuffer (Uniform)
    bindings[1].binding            = 1;
    bindings[1].descriptorType     = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
    bindings[1].descriptorCount    = 1;
    bindings[1].stageFlags         = VK_SHADER_STAGE_COMPUTE_BIT | VK_SHADER_STAGE_VERTEX_BIT;
    bindings[1].pImmutableSamplers = nullptr;

    // Binding 2: VisibleInstanceBuffer (Storage)
    bindings[2].binding            = 2;
    bindings[2].descriptorType     = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
    bindings[2].descriptorCount    = 1;
    bindings[2].stageFlags         = VK_SHADER_STAGE_COMPUTE_BIT | VK_SHADER_STAGE_VERTEX_BIT;
    bindings[2].pImmutableSamplers = nullptr;

    // Binding 3: VisibleCountBuffer (Storage)
    bindings[3].binding            = 3;
    bindings[3].descriptorType     = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
    bindings[3].descriptorCount    = 1;
    bindings[3].stageFlags         = VK_SHADER_STAGE_COMPUTE_BIT | VK_SHADER_STAGE_VERTEX_BIT;
    bindings[3].pImmutableSamplers = nullptr;

    VkDescriptorSetLayoutCreateInfo layoutInfo{};
    layoutInfo.sType        = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
    layoutInfo.bindingCount = 4;
    layoutInfo.pBindings    = bindings;

    VK_CHECK(vkCreateDescriptorSetLayout(resources.device, &layoutInfo, nullptr, &m_DescriptorSetLayout));

    // 2. Create Pipeline Layout
    VkPushConstantRange pushRange{};
    pushRange.stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;
    pushRange.offset = 0;
    pushRange.size = sizeof(CullPushConstants);

    VkPipelineLayoutCreateInfo pipelineLayoutInfo{};
    pipelineLayoutInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
    pipelineLayoutInfo.setLayoutCount = 1;
    pipelineLayoutInfo.pSetLayouts = &m_DescriptorSetLayout;
    pipelineLayoutInfo.pushConstantRangeCount = 1;
    pipelineLayoutInfo.pPushConstantRanges = &pushRange;

    VK_CHECK(vkCreatePipelineLayout(resources.device, &pipelineLayoutInfo, nullptr, &m_PipelineLayout));

    // 3. Create Compute Pipeline
    createPipeline(resources);

    // 4. Create Frame Resources
    m_Frames.resize(resources.MAX_FRAMES_IN_FLIGHT);
    for (uint32_t i = 0; i < resources.MAX_FRAMES_IN_FLIGHT; ++i) {
        createFrameResources(resources, m_Frames[i]);
    }

    LOG_INFO("FrustumCullPass: Initialization complete.");
}

void FrustumCullPass::Shutdown(EngineResources& resources)
{
    LOG_INFO("FrustumCullPass: Shutting down...");

    for (uint32_t i = 0; i < resources.MAX_FRAMES_IN_FLIGHT; ++i) {
        destroyFrameResources(resources, m_Frames[i]);
    }
    m_Frames.clear();

    if (m_Pipeline != VK_NULL_HANDLE) {
        vkDestroyPipeline(resources.device, m_Pipeline, nullptr);
        m_Pipeline = VK_NULL_HANDLE;
    }

    if (m_PipelineLayout != VK_NULL_HANDLE) {
        vkDestroyPipelineLayout(resources.device, m_PipelineLayout, nullptr);
        m_PipelineLayout = VK_NULL_HANDLE;
    }

    if (m_DescriptorSetLayout != VK_NULL_HANDLE) {
        vkDestroyDescriptorSetLayout(resources.device, m_DescriptorSetLayout, nullptr);
        m_DescriptorSetLayout = VK_NULL_HANDLE;
    }

    LOG_INFO("FrustumCullPass: Shutdown complete.");
}

void FrustumCullPass::createPipeline(EngineResources& resources)
{
    VkShaderModule computeShader = resources.loadShaderModule("shaders/frustum_cull.spv");
    if (computeShader == VK_NULL_HANDLE) {
        LOG_ERROR("FrustumCullPass: Failed to load compute shader shaders/frustum_cull.spv");
        return;
    }

    VkComputePipelineCreateInfo computePipelineInfo{};
    computePipelineInfo.sType = VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO;
    computePipelineInfo.layout = m_PipelineLayout;
    computePipelineInfo.stage.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    computePipelineInfo.stage.stage = VK_SHADER_STAGE_COMPUTE_BIT;
    computePipelineInfo.stage.module = computeShader;
    computePipelineInfo.stage.pName = "main";

    VK_CHECK(vkCreateComputePipelines(resources.device, VK_NULL_HANDLE, 1, &computePipelineInfo, nullptr, &m_Pipeline));

    vkDestroyShaderModule(resources.device, computeShader, nullptr);
    LOG_INFO("FrustumCullPass: Compute pipeline created successfully.");
}

void FrustumCullPass::createFrameResources(EngineResources& resources, FrustumCullFrameResources& frameRes)
{
    // 1. Create Descriptor Pool
    VkDescriptorPoolSize poolSizes[2]{};
    poolSizes[0].type            = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
    poolSizes[0].descriptorCount = 3; // InstanceBuffer, VisibleInstanceBuffer, VisibleCountBuffer
    poolSizes[1].type            = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
    poolSizes[1].descriptorCount = 1; // FrustumBuffer

    VkDescriptorPoolCreateInfo poolInfo{};
    poolInfo.sType         = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
    poolInfo.poolSizeCount = 2;
    poolInfo.pPoolSizes    = poolSizes;
    poolInfo.maxSets       = 1;
    poolInfo.flags         = VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT;

    VK_CHECK(vkCreateDescriptorPool(resources.device, &poolInfo, nullptr, &frameRes.descriptorPool));

    // 2. Allocate Descriptor Set
    VkDescriptorSetAllocateInfo setAlloc{};
    setAlloc.sType              = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
    setAlloc.descriptorPool     = frameRes.descriptorPool;
    setAlloc.descriptorSetCount = 1;
    setAlloc.pSetLayouts        = &m_DescriptorSetLayout;

    VK_CHECK(vkAllocateDescriptorSets(resources.device, &setAlloc, &frameRes.descriptorSet));

    // 3. Create Buffers
    // Visible Count Buffer (Host visible to read back easily)
    VkBufferCreateInfo bufInfo{};
    bufInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
    bufInfo.size  = sizeof(uint32_t);
    bufInfo.usage = VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_TRANSFER_SRC_BIT;
    bufInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

    VmaAllocationCreateInfo vmaAllocInfo{};
    vmaAllocInfo.usage = VMA_MEMORY_USAGE_CPU_TO_GPU;

    VK_CHECK(vmaCreateBuffer(resources.allocator, &bufInfo, &vmaAllocInfo, &frameRes.visibleCountBuffer, &frameRes.visibleCountAlloc, nullptr));
    ::eng::ResourceTracker::incBuffer();

    // Initialize visibleCount to 0
    uint32_t zero = 0;
    void* mapped = nullptr;
    VK_CHECK(vmaMapMemory(resources.allocator, frameRes.visibleCountAlloc, &mapped));
    std::memcpy(mapped, &zero, sizeof(uint32_t));
    vmaUnmapMemory(resources.allocator, frameRes.visibleCountAlloc);

    // Visible Instance Buffer (Initially default to 128 elements, GPU only)
    frameRes.visibleInstanceCapacity = 128;
    frameRes.visibleInstanceBufferSize = frameRes.visibleInstanceCapacity * sizeof(uint32_t);
    
    bufInfo.size = frameRes.visibleInstanceBufferSize;
    bufInfo.usage = VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT;
    
    VmaAllocationCreateInfo gpuAllocInfo{};
    gpuAllocInfo.usage = VMA_MEMORY_USAGE_CPU_TO_GPU;

    VK_CHECK(vmaCreateBuffer(resources.allocator, &bufInfo, &gpuAllocInfo, &frameRes.visibleInstanceBuffer, &frameRes.visibleInstanceAlloc, nullptr));
    ::eng::ResourceTracker::incBuffer();
}

void FrustumCullPass::destroyFrameResources(EngineResources& resources, FrustumCullFrameResources& frameRes)
{
    if (frameRes.visibleInstanceBuffer != VK_NULL_HANDLE) {
        vmaDestroyBuffer(resources.allocator, frameRes.visibleInstanceBuffer, frameRes.visibleInstanceAlloc);
        ::eng::ResourceTracker::decBuffer();
        frameRes.visibleInstanceBuffer = VK_NULL_HANDLE;
    }

    if (frameRes.visibleCountBuffer != VK_NULL_HANDLE) {
        vmaDestroyBuffer(resources.allocator, frameRes.visibleCountBuffer, frameRes.visibleCountAlloc);
        ::eng::ResourceTracker::decBuffer();
        frameRes.visibleCountBuffer = VK_NULL_HANDLE;
    }

    if (frameRes.descriptorPool != VK_NULL_HANDLE) {
        vkDestroyDescriptorPool(resources.device, frameRes.descriptorPool, nullptr);
        frameRes.descriptorPool = VK_NULL_HANDLE;
    }
}

void FrustumCullPass::resizeVisibleInstanceBufferIfNeeded(
    EngineResources& resources,
    FrustumCullFrameResources& frameRes,
    uint32_t neededCapacity
)
{
    if (frameRes.visibleInstanceBuffer != VK_NULL_HANDLE && frameRes.visibleInstanceCapacity >= neededCapacity) {
        return;
    }

    uint32_t newCapacity = std::max(frameRes.visibleInstanceCapacity * 2, 128u);
    while (newCapacity < neededCapacity) {
        newCapacity *= 2;
    }

    VkDeviceSize newSize = newCapacity * sizeof(uint32_t);

    if (frameRes.visibleInstanceBuffer != VK_NULL_HANDLE) {
        vkDeviceWaitIdle(resources.device);
        vmaDestroyBuffer(resources.allocator, frameRes.visibleInstanceBuffer, frameRes.visibleInstanceAlloc);
        ::eng::ResourceTracker::decBuffer();
        frameRes.visibleInstanceBuffer = VK_NULL_HANDLE;
    }

    VkBufferCreateInfo bufInfo{};
    bufInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
    bufInfo.size  = newSize;
    bufInfo.usage = VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT;
    bufInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

    VmaAllocationCreateInfo allocInfo{};
    allocInfo.usage = VMA_MEMORY_USAGE_CPU_TO_GPU;

    VkResult res = vmaCreateBuffer(resources.allocator, &bufInfo, &allocInfo, &frameRes.visibleInstanceBuffer, &frameRes.visibleInstanceAlloc, nullptr);
    VK_CHECK(res);
    ::eng::ResourceTracker::incBuffer();

    frameRes.visibleInstanceCapacity = newCapacity;
    frameRes.visibleInstanceBufferSize = newSize;

    LOG_INFO("FrustumCullPass: Resized visibleInstanceBuffer to capacity " + std::to_string(newCapacity) + " (" + std::to_string(newSize) + " bytes)");
}

void FrustumCullPass::writeDescriptorSet(
    EngineResources& resources,
    FrustumCullFrameResources& frameRes,
    VkBuffer instanceBuffer,
    VkDeviceSize instanceBufferSize,
    VkBuffer frustumBuffer
)
{
    std::array<VkWriteDescriptorSet, 4> writes{};

    // Binding 0: InstanceBuffer (Storage)
    VkDescriptorBufferInfo instanceInfo{};
    instanceInfo.buffer = instanceBuffer;
    instanceInfo.offset = 0;
    instanceInfo.range  = instanceBufferSize;

    writes[0].sType           = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    writes[0].dstSet          = frameRes.descriptorSet;
    writes[0].dstBinding      = 0;
    writes[0].dstArrayElement = 0;
    writes[0].descriptorCount = 1;
    writes[0].descriptorType  = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
    writes[0].pBufferInfo     = &instanceInfo;

    // Binding 1: FrustumBuffer (Uniform)
    VkDescriptorBufferInfo frustumInfo{};
    frustumInfo.buffer = frustumBuffer;
    frustumInfo.offset = 0;
    frustumInfo.range  = VK_WHOLE_SIZE; // Matches GPUFrustum size

    writes[1].sType           = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    writes[1].dstSet          = frameRes.descriptorSet;
    writes[1].dstBinding      = 1;
    writes[1].dstArrayElement = 0;
    writes[1].descriptorCount = 1;
    writes[1].descriptorType  = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
    writes[1].pBufferInfo     = &frustumInfo;

    // Binding 2: VisibleInstanceBuffer (Storage)
    VkDescriptorBufferInfo visibleInstanceInfo{};
    visibleInstanceInfo.buffer = frameRes.visibleInstanceBuffer;
    visibleInstanceInfo.offset = 0;
    visibleInstanceInfo.range  = frameRes.visibleInstanceBufferSize;

    writes[2].sType           = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    writes[2].dstSet          = frameRes.descriptorSet;
    writes[2].dstBinding      = 2;
    writes[2].dstArrayElement = 0;
    writes[2].descriptorCount = 1;
    writes[2].descriptorType  = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
    writes[2].pBufferInfo     = &visibleInstanceInfo;

    // Binding 3: VisibleCountBuffer (Storage)
    VkDescriptorBufferInfo visibleCountInfo{};
    visibleCountInfo.buffer = frameRes.visibleCountBuffer;
    visibleCountInfo.offset = 0;
    visibleCountInfo.range  = sizeof(uint32_t);

    writes[3].sType           = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    writes[3].dstSet          = frameRes.descriptorSet;
    writes[3].dstBinding      = 3;
    writes[3].dstArrayElement = 0;
    writes[3].descriptorCount = 1;
    writes[3].descriptorType  = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
    writes[3].pBufferInfo     = &visibleCountInfo;

    vkUpdateDescriptorSets(resources.device, static_cast<uint32_t>(writes.size()), writes.data(), 0, nullptr);
}

void FrustumCullPass::Execute(
    VkCommandBuffer cmd,
    EngineResources& resources,
    uint32_t frameIndex,
    GPUScene& gpuScene,
    uint32_t instanceCount
)
{
    if (m_Pipeline == VK_NULL_HANDLE) {
        return;
    }

    FrustumCullFrameResources& frameRes = m_Frames[frameIndex];
    const auto& sceneRes = gpuScene.GetFrameResources(frameIndex);

    // If there are no instances, we can just clear visibleCount and return
    if (instanceCount == 0 || sceneRes.instanceBuffer == VK_NULL_HANDLE || sceneRes.frustumBuffer == VK_NULL_HANDLE) {
        vkCmdFillBuffer(cmd, frameRes.visibleCountBuffer, 0, sizeof(uint32_t), 0);
        return;
    }

    // 1. Dynamic Buffers Resizing
    resizeVisibleInstanceBufferIfNeeded(resources, frameRes, instanceCount);

    // 2. Descriptor set update
    writeDescriptorSet(resources, frameRes, sceneRes.instanceBuffer, sceneRes.instanceBufferSize, sceneRes.frustumBuffer);

    // 3. Clear atomic count to 0 via Vulkan command
    vkCmdFillBuffer(cmd, frameRes.visibleCountBuffer, 0, sizeof(uint32_t), 0);

    // 4. Barrier: Clear -> Compute Shader write
    VkBufferMemoryBarrier clearBarrier{};
    clearBarrier.sType               = VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER;
    clearBarrier.srcAccessMask       = VK_ACCESS_TRANSFER_WRITE_BIT;
    clearBarrier.dstAccessMask       = VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_SHADER_WRITE_BIT;
    clearBarrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    clearBarrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    clearBarrier.buffer              = frameRes.visibleCountBuffer;
    clearBarrier.offset              = 0;
    clearBarrier.size                = VK_WHOLE_SIZE;

    vkCmdPipelineBarrier(
        cmd,
        VK_PIPELINE_STAGE_TRANSFER_BIT,
        VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
        0,
        0, nullptr,
        1, &clearBarrier,
        0, nullptr
    );

    // 5. Bind pipeline and dispatch
    vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_COMPUTE, m_Pipeline);
    vkCmdBindDescriptorSets(
        cmd,
        VK_PIPELINE_BIND_POINT_COMPUTE,
        m_PipelineLayout,
        0, 1, &frameRes.descriptorSet,
        0, nullptr
    );

    CullPushConstants pc{};
    pc.instanceCount = instanceCount;

    vkCmdPushConstants(
        cmd,
        m_PipelineLayout,
        VK_SHADER_STAGE_COMPUTE_BIT,
        0,
        sizeof(CullPushConstants),
        &pc
    );

    uint32_t groupCount = (instanceCount + 63) / 64;
    vkCmdDispatch(cmd, groupCount, 1, 1);

    // 6. Barrier: Compute Shader write -> Host Readback or future passes
    std::array<VkBufferMemoryBarrier, 2> barriers{};
    
    barriers[0].sType               = VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER;
    barriers[0].srcAccessMask       = VK_ACCESS_SHADER_WRITE_BIT;
    barriers[0].dstAccessMask       = VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_TRANSFER_READ_BIT;
    barriers[0].srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    barriers[0].dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    barriers[0].buffer              = frameRes.visibleInstanceBuffer;
    barriers[0].offset              = 0;
    barriers[0].size                = VK_WHOLE_SIZE;

    barriers[1].sType               = VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER;
    barriers[1].srcAccessMask       = VK_ACCESS_SHADER_WRITE_BIT;
    barriers[1].dstAccessMask       = VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_TRANSFER_READ_BIT | VK_ACCESS_HOST_READ_BIT;
    barriers[1].srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    barriers[1].dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    barriers[1].buffer              = frameRes.visibleCountBuffer;
    barriers[1].offset              = 0;
    barriers[1].size                = VK_WHOLE_SIZE;

    vkCmdPipelineBarrier(
        cmd,
        VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
        VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT | VK_PIPELINE_STAGE_TRANSFER_BIT | VK_PIPELINE_STAGE_HOST_BIT,
        0,
        0, nullptr,
        static_cast<uint32_t>(barriers.size()), barriers.data(),
        0, nullptr
    );
}

uint32_t FrustumCullPass::ReadBackVisibleCount(EngineResources& resources, uint32_t frameIndex)
{
    if (frameIndex >= m_Frames.size()) return 0;
    
    const FrustumCullFrameResources& frameRes = m_Frames[frameIndex];
    if (frameRes.visibleCountBuffer == VK_NULL_HANDLE) return 0;

    uint32_t count = 0;
    void* mapped = nullptr;
    VkResult res = vmaMapMemory(resources.allocator, frameRes.visibleCountAlloc, &mapped);
    if (res == VK_SUCCESS && mapped != nullptr) {
        std::memcpy(&count, mapped, sizeof(uint32_t));
        vmaUnmapMemory(resources.allocator, frameRes.visibleCountAlloc);
    } else {
        LOG_ERROR("FrustumCullPass: Failed to map visibleCountBuffer for host readback.");
    }
    return count;
}

std::vector<uint32_t> FrustumCullPass::ReadBackVisibleInstances(EngineResources& resources, uint32_t frameIndex, uint32_t count)
{
    std::vector<uint32_t> visibleIndices;
    if (count == 0 || frameIndex >= m_Frames.size()) return visibleIndices;

    const FrustumCullFrameResources& frameRes = m_Frames[frameIndex];
    if (frameRes.visibleInstanceBuffer == VK_NULL_HANDLE) return visibleIndices;

    visibleIndices.resize(count);
    void* mapped = nullptr;
    VkResult res = vmaMapMemory(resources.allocator, frameRes.visibleInstanceAlloc, &mapped);
    if (res == VK_SUCCESS && mapped != nullptr) {
        std::memcpy(visibleIndices.data(), mapped, count * sizeof(uint32_t));
        vmaUnmapMemory(resources.allocator, frameRes.visibleInstanceAlloc);
    } else {
        LOG_ERROR("FrustumCullPass: Failed to map visibleInstanceBuffer for host readback.");
        visibleIndices.clear();
    }
    return visibleIndices;
}

} // namespace eng::renderer
