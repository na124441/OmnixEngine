#include "IndirectCommandBuildPass.h"
#include "Core/Engine/Log.h"
#include <array>
#include <algorithm>
#include <cstring>

namespace eng::renderer {

static_assert(sizeof(VkDrawIndexedIndirectCommand) == 20, "VkDrawIndexedIndirectCommand size must be exactly 20 bytes.");
static_assert(offsetof(VkDrawIndexedIndirectCommand, indexCount) == 0, "indexCount must be at offset 0.");
static_assert(offsetof(VkDrawIndexedIndirectCommand, instanceCount) == 4, "instanceCount must be at offset 4.");
static_assert(offsetof(VkDrawIndexedIndirectCommand, firstIndex) == 8, "firstIndex must be at offset 8.");
static_assert(offsetof(VkDrawIndexedIndirectCommand, vertexOffset) == 12, "vertexOffset must be at offset 12.");
static_assert(offsetof(VkDrawIndexedIndirectCommand, firstInstance) == 16, "firstInstance must be at offset 16.");

void IndirectCommandBuildPass::Initialize(EngineResources& resources)
{
    LOG_INFO("IndirectCommandBuildPass: Initializing...");

    // 1. Create Descriptor Set Layout
    VkDescriptorSetLayoutBinding bindings[6]{};

    // Binding 0: VisibleInstanceBuffer (Storage, Readonly)
    bindings[0].binding            = 0;
    bindings[0].descriptorType     = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
    bindings[0].descriptorCount    = 1;
    bindings[0].stageFlags         = VK_SHADER_STAGE_COMPUTE_BIT;
    bindings[0].pImmutableSamplers = nullptr;

    // Binding 1: VisibleCountBuffer (Storage, Readonly)
    bindings[1].binding            = 1;
    bindings[1].descriptorType     = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
    bindings[1].descriptorCount    = 1;
    bindings[1].stageFlags         = VK_SHADER_STAGE_COMPUTE_BIT;
    bindings[1].pImmutableSamplers = nullptr;

    // Binding 2: InstanceBuffer (Storage, Readonly)
    bindings[2].binding            = 2;
    bindings[2].descriptorType     = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
    bindings[2].descriptorCount    = 1;
    bindings[2].stageFlags         = VK_SHADER_STAGE_COMPUTE_BIT;
    bindings[2].pImmutableSamplers = nullptr;

    // Binding 3: MeshDrawDataBuffer (Storage, Readonly)
    bindings[3].binding            = 3;
    bindings[3].descriptorType     = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
    bindings[3].descriptorCount    = 1;
    bindings[3].stageFlags         = VK_SHADER_STAGE_COMPUTE_BIT;
    bindings[3].pImmutableSamplers = nullptr;

    // Binding 4: IndirectCommandBuffer (Storage)
    bindings[4].binding            = 4;
    bindings[4].descriptorType     = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
    bindings[4].descriptorCount    = 1;
    bindings[4].stageFlags         = VK_SHADER_STAGE_COMPUTE_BIT;
    bindings[4].pImmutableSamplers = nullptr;

    // Binding 5: IndirectCountBuffer (Storage)
    bindings[5].binding            = 5;
    bindings[5].descriptorType     = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
    bindings[5].descriptorCount    = 1;
    bindings[5].stageFlags         = VK_SHADER_STAGE_COMPUTE_BIT;
    bindings[5].pImmutableSamplers = nullptr;

    VkDescriptorSetLayoutCreateInfo layoutInfo{};
    layoutInfo.sType        = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
    layoutInfo.bindingCount = 6;
    layoutInfo.pBindings    = bindings;

    VK_CHECK(vkCreateDescriptorSetLayout(resources.device, &layoutInfo, nullptr, &m_DescriptorSetLayout));

    // 2. Create Pipeline Layout
    VkPipelineLayoutCreateInfo pipelineLayoutInfo{};
    pipelineLayoutInfo.sType          = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
    pipelineLayoutInfo.setLayoutCount = 1;
    pipelineLayoutInfo.pSetLayouts    = &m_DescriptorSetLayout;

    VK_CHECK(vkCreatePipelineLayout(resources.device, &pipelineLayoutInfo, nullptr, &m_PipelineLayout));

    // 3. Create Compute Pipeline
    createPipeline(resources);

    // 4. Create Frame Resources
    m_Frames.resize(resources.MAX_FRAMES_IN_FLIGHT);
    for (uint32_t i = 0; i < resources.MAX_FRAMES_IN_FLIGHT; ++i) {
        createFrameResources(resources, m_Frames[i]);
    }

    LOG_INFO("IndirectCommandBuildPass: Initialization completed successfully.");
}

void IndirectCommandBuildPass::Shutdown(EngineResources& resources)
{
    LOG_INFO("IndirectCommandBuildPass: Shutting down...");

    for (uint32_t i = 0; i < m_Frames.size(); ++i) {
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

    LOG_INFO("IndirectCommandBuildPass: Shutdown completed.");
}

void IndirectCommandBuildPass::createPipeline(EngineResources& resources)
{
    VkShaderModule computeShader = resources.loadShaderModule("shaders/build_indirect_commands.spv");
    if (computeShader == VK_NULL_HANDLE) {
        LOG_ERROR("IndirectCommandBuildPass: Failed to load compute shader shaders/build_indirect_commands.spv");
        return;
    }

    VkComputePipelineCreateInfo pipelineInfo{};
    pipelineInfo.sType  = VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO;
    pipelineInfo.layout = m_PipelineLayout;
    pipelineInfo.stage.sType  = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    pipelineInfo.stage.stage  = VK_SHADER_STAGE_COMPUTE_BIT;
    pipelineInfo.stage.module = computeShader;
    pipelineInfo.stage.pName  = "main";

    VK_CHECK(vkCreateComputePipelines(resources.device, VK_NULL_HANDLE, 1, &pipelineInfo, nullptr, &m_Pipeline));

    vkDestroyShaderModule(resources.device, computeShader, nullptr);
}

void IndirectCommandBuildPass::createFrameResources(EngineResources& resources, IndirectCommandFrameResources& frameRes)
{
    // 1. Allocate Descriptor Pool
    VkDescriptorPoolSize poolSizes[1]{};
    poolSizes[0].type            = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
    poolSizes[0].descriptorCount = 12; // 6 descriptors * 2 sets = 12

    VkDescriptorPoolCreateInfo poolInfo{};
    poolInfo.sType         = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
    poolInfo.poolSizeCount = 1;
    poolInfo.pPoolSizes    = poolSizes;
    poolInfo.maxSets       = 2; // 2 sets
    poolInfo.flags         = VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT;

    VK_CHECK(vkCreateDescriptorPool(resources.device, &poolInfo, nullptr, &frameRes.descriptorPool));

    // 2. Allocate Descriptor Sets
    std::vector<VkDescriptorSetLayout> layouts(2, m_DescriptorSetLayout);
    VkDescriptorSetAllocateInfo setAlloc{};
    setAlloc.sType              = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
    setAlloc.descriptorPool     = frameRes.descriptorPool;
    setAlloc.descriptorSetCount = 2;
    setAlloc.pSetLayouts        = layouts.data();

    std::vector<VkDescriptorSet> sets(2);
    VK_CHECK(vkAllocateDescriptorSets(resources.device, &setAlloc, sets.data()));
    frameRes.descriptorSet = sets[0];
    frameRes.descriptorSetFinal = sets[1];

    // 3. Create Indirect Count Buffer (4 bytes, host-visible for diagnostics)
    VkBufferCreateInfo bufInfo{};
    bufInfo.sType       = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
    bufInfo.size        = sizeof(uint32_t);
    bufInfo.usage       = VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_INDIRECT_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_SRC_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT;
    bufInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

    VmaAllocationCreateInfo vmaAllocInfo{};
    vmaAllocInfo.usage  = VMA_MEMORY_USAGE_CPU_TO_GPU;

    VK_CHECK(vmaCreateBuffer(resources.allocator, &bufInfo, &vmaAllocInfo, &frameRes.indirectCountBuffer, &frameRes.indirectCountAlloc, nullptr));
    ::eng::ResourceTracker::incBuffer();

    // Clear count buffer to 0
    uint32_t zero = 0;
    void* mapped = nullptr;
    VK_CHECK(vmaMapMemory(resources.allocator, frameRes.indirectCountAlloc, &mapped));
    std::memcpy(mapped, &zero, sizeof(uint32_t));
    vmaUnmapMemory(resources.allocator, frameRes.indirectCountAlloc);

    // 4. Initialize dynamic IndirectCommandBuffer
    frameRes.indirectCommandCapacity = 128;
    frameRes.indirectCommandBufferSize = frameRes.indirectCommandCapacity * sizeof(VkDrawIndexedIndirectCommand);
    bufInfo.size  = frameRes.indirectCommandBufferSize;
    bufInfo.usage = VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_INDIRECT_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT;

    VK_CHECK(vmaCreateBuffer(resources.allocator, &bufInfo, &vmaAllocInfo, &frameRes.indirectCommandBuffer, &frameRes.indirectCommandAlloc, nullptr));
    ::eng::ResourceTracker::incBuffer();
}

void IndirectCommandBuildPass::destroyFrameResources(EngineResources& resources, IndirectCommandFrameResources& frameRes)
{
    if (frameRes.indirectCommandBuffer != VK_NULL_HANDLE) {
        vmaDestroyBuffer(resources.allocator, frameRes.indirectCommandBuffer, frameRes.indirectCommandAlloc);
        ::eng::ResourceTracker::decBuffer();
        frameRes.indirectCommandBuffer = VK_NULL_HANDLE;
    }

    if (frameRes.indirectCountBuffer != VK_NULL_HANDLE) {
        vmaDestroyBuffer(resources.allocator, frameRes.indirectCountBuffer, frameRes.indirectCountAlloc);
        ::eng::ResourceTracker::decBuffer();
        frameRes.indirectCountBuffer = VK_NULL_HANDLE;
    }

    if (frameRes.descriptorPool != VK_NULL_HANDLE) {
        vkDestroyDescriptorPool(resources.device, frameRes.descriptorPool, nullptr);
        frameRes.descriptorPool = VK_NULL_HANDLE;
    }
    frameRes.descriptorSet = VK_NULL_HANDLE;
    frameRes.descriptorSetFinal = VK_NULL_HANDLE;
}

void IndirectCommandBuildPass::resizeIndirectCommandBufferIfNeeded(
    EngineResources& resources,
    IndirectCommandFrameResources& frameRes,
    uint32_t neededCapacity
)
{
    if (frameRes.indirectCommandBuffer != VK_NULL_HANDLE && frameRes.indirectCommandCapacity >= neededCapacity) {
        return;
    }

    uint32_t newCapacity = std::max(frameRes.indirectCommandCapacity * 2, 128u);
    while (newCapacity < neededCapacity) {
        newCapacity *= 2;
    }

    VkDeviceSize newSize = newCapacity * sizeof(VkDrawIndexedIndirectCommand);

    if (frameRes.indirectCommandBuffer != VK_NULL_HANDLE) {
        vmaDestroyBuffer(resources.allocator, frameRes.indirectCommandBuffer, frameRes.indirectCommandAlloc);
        ::eng::ResourceTracker::decBuffer();
        frameRes.indirectCommandBuffer = VK_NULL_HANDLE;
    }

    VkBufferCreateInfo bufInfo{};
    bufInfo.sType       = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
    bufInfo.size        = newSize;
    bufInfo.usage       = VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_INDIRECT_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT;
    bufInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

    VmaAllocationCreateInfo allocInfo{};
    allocInfo.usage = VMA_MEMORY_USAGE_CPU_TO_GPU;

    VkResult res = vmaCreateBuffer(resources.allocator, &bufInfo, &allocInfo, &frameRes.indirectCommandBuffer, &frameRes.indirectCommandAlloc, nullptr);
    VK_CHECK(res);
    ::eng::ResourceTracker::incBuffer();

    frameRes.indirectCommandCapacity = newCapacity;
    frameRes.indirectCommandBufferSize = newSize;

    LOG_INFO("IndirectCommandBuildPass: Resized indirectCommandBuffer to capacity " + std::to_string(newCapacity) + " (" + std::to_string(newSize) + " bytes)");
}

void IndirectCommandBuildPass::writeDescriptorSet(
    EngineResources& resources,
    IndirectCommandFrameResources& frameRes,
    VkDescriptorSet descriptorSet,
    VkBuffer visibleInstanceBuffer,
    VkDeviceSize visibleInstanceBufferSize,
    VkBuffer visibleCountBuffer,
    VkBuffer instanceBuffer,
    VkDeviceSize instanceBufferSize,
    VkBuffer meshDrawDataBuffer,
    VkDeviceSize meshDrawDataBufferSize
)
{
    std::array<VkWriteDescriptorSet, 6> writes{};

    // Binding 0: VisibleInstanceBuffer
    VkDescriptorBufferInfo visibleInstanceInfo{};
    visibleInstanceInfo.buffer = visibleInstanceBuffer;
    visibleInstanceInfo.offset = 0;
    visibleInstanceInfo.range  = visibleInstanceBufferSize;

    writes[0].sType           = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    writes[0].dstSet          = descriptorSet;
    writes[0].dstBinding      = 0;
    writes[0].dstArrayElement = 0;
    writes[0].descriptorCount = 1;
    writes[0].descriptorType  = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
    writes[0].pBufferInfo     = &visibleInstanceInfo;

    // Binding 1: VisibleCountBuffer
    VkDescriptorBufferInfo visibleCountInfo{};
    visibleCountInfo.buffer = visibleCountBuffer;
    visibleCountInfo.offset = 0;
    visibleCountInfo.range  = sizeof(uint32_t);

    writes[1].sType           = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    writes[1].dstSet          = descriptorSet;
    writes[1].dstBinding      = 1;
    writes[1].dstArrayElement = 0;
    writes[1].descriptorCount = 1;
    writes[1].descriptorType  = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
    writes[1].pBufferInfo     = &visibleCountInfo;

    // Binding 2: InstanceBuffer
    VkDescriptorBufferInfo instanceInfo{};
    instanceInfo.buffer = instanceBuffer;
    instanceInfo.offset = 0;
    instanceInfo.range  = instanceBufferSize;

    writes[2].sType           = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    writes[2].dstSet          = descriptorSet;
    writes[2].dstBinding      = 2;
    writes[2].dstArrayElement = 0;
    writes[2].descriptorCount = 1;
    writes[2].descriptorType  = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
    writes[2].pBufferInfo     = &instanceInfo;

    // Binding 3: MeshDrawDataBuffer
    VkDescriptorBufferInfo meshDrawDataInfo{};
    meshDrawDataInfo.buffer = meshDrawDataBuffer;
    meshDrawDataInfo.offset = 0;
    meshDrawDataInfo.range  = meshDrawDataBufferSize;

    writes[3].sType           = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    writes[3].dstSet          = descriptorSet;
    writes[3].dstBinding      = 3;
    writes[3].dstArrayElement = 0;
    writes[3].descriptorCount = 1;
    writes[3].descriptorType  = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
    writes[3].pBufferInfo     = &meshDrawDataInfo;

    // Binding 4: IndirectCommandBuffer
    VkDescriptorBufferInfo indirectCommandInfo{};
    indirectCommandInfo.buffer = frameRes.indirectCommandBuffer;
    indirectCommandInfo.offset = 0;
    indirectCommandInfo.range  = frameRes.indirectCommandBufferSize;

    writes[4].sType           = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    writes[4].dstSet          = descriptorSet;
    writes[4].dstBinding      = 4;
    writes[4].dstArrayElement = 0;
    writes[4].descriptorCount = 1;
    writes[4].descriptorType  = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
    writes[4].pBufferInfo     = &indirectCommandInfo;

    // Binding 5: IndirectCountBuffer
    VkDescriptorBufferInfo indirectCountInfo{};
    indirectCountInfo.buffer = frameRes.indirectCountBuffer;
    indirectCountInfo.offset = 0;
    indirectCountInfo.range  = sizeof(uint32_t);

    writes[5].sType           = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    writes[5].dstSet          = descriptorSet;
    writes[5].dstBinding      = 5;
    writes[5].dstArrayElement = 0;
    writes[5].descriptorCount = 1;
    writes[5].descriptorType  = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
    writes[5].pBufferInfo     = &indirectCountInfo;

    vkUpdateDescriptorSets(resources.device, static_cast<uint32_t>(writes.size()), writes.data(), 0, nullptr);
}

void IndirectCommandBuildPass::Execute(
    VkCommandBuffer cmd,
    EngineResources& resources,
    uint32_t frameIndex,
    GPUScene& gpuScene,
    VkBuffer visibleInstanceBuffer,
    VkDeviceSize visibleInstanceBufferSize,
    VkBuffer visibleCountBuffer,
    uint32_t maxInstanceCount,
    bool isFinal
)
{
    if (m_Pipeline == VK_NULL_HANDLE) {
        return;
    }

    IndirectCommandFrameResources& frameRes = m_Frames[frameIndex];
    const auto& sceneRes = gpuScene.GetFrameResources(frameIndex);

    // If there are no instances, clear indirect count buffer to zero and return
    if (maxInstanceCount == 0 || sceneRes.instanceBuffer == VK_NULL_HANDLE || sceneRes.meshDrawDataBuffer == VK_NULL_HANDLE || visibleInstanceBuffer == VK_NULL_HANDLE) {
        vkCmdFillBuffer(cmd, frameRes.indirectCountBuffer, 0, sizeof(uint32_t), 0);
        return;
    }

    // 0. WAR barrier: if isFinal is true, we must wait for DepthPrepass to finish reading
    if (isFinal) {
        std::array<VkBufferMemoryBarrier, 2> prevBarriers{};
        prevBarriers[0].sType = VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER;
        prevBarriers[0].srcAccessMask = VK_ACCESS_INDIRECT_COMMAND_READ_BIT;
        prevBarriers[0].dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
        prevBarriers[0].srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        prevBarriers[0].dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        prevBarriers[0].buffer = frameRes.indirectCommandBuffer;
        prevBarriers[0].offset = 0;
        prevBarriers[0].size = VK_WHOLE_SIZE;

        prevBarriers[1].sType = VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER;
        prevBarriers[1].srcAccessMask = VK_ACCESS_INDIRECT_COMMAND_READ_BIT;
        prevBarriers[1].dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
        prevBarriers[1].srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        prevBarriers[1].dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        prevBarriers[1].buffer = frameRes.indirectCountBuffer;
        prevBarriers[1].offset = 0;
        prevBarriers[1].size = VK_WHOLE_SIZE;

        vkCmdPipelineBarrier(
            cmd,
            VK_PIPELINE_STAGE_DRAW_INDIRECT_BIT,
            VK_PIPELINE_STAGE_TRANSFER_BIT,
            0,
            0, nullptr,
            2, prevBarriers.data(),
            0, nullptr
        );
    }

    // 1. Resize command buffer to support up to maxInstanceCount
    resizeIndirectCommandBufferIfNeeded(resources, frameRes, maxInstanceCount);

    // 2. Select appropriate descriptor set and write/update descriptors
    VkDescriptorSet activeSet = isFinal ? frameRes.descriptorSetFinal : frameRes.descriptorSet;
    writeDescriptorSet(
        resources,
        frameRes,
        activeSet,
        visibleInstanceBuffer,
        visibleInstanceBufferSize,
        visibleCountBuffer,
        sceneRes.instanceBuffer,
        sceneRes.instanceBufferSize,
        sceneRes.meshDrawDataBuffer,
        sceneRes.meshDrawDataBufferSize
    );

    // 3. Clear indirect draw count to 0 via Vulkan command
    vkCmdFillBuffer(cmd, frameRes.indirectCountBuffer, 0, sizeof(uint32_t), 0);

    // 4. Barrier: Clear -> Compute Shader write
    VkBufferMemoryBarrier clearBarrier{};
    clearBarrier.sType               = VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER;
    clearBarrier.srcAccessMask       = VK_ACCESS_TRANSFER_WRITE_BIT;
    clearBarrier.dstAccessMask       = VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_SHADER_WRITE_BIT;
    clearBarrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    clearBarrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    clearBarrier.buffer              = frameRes.indirectCountBuffer;
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

    // 5. Bind pipeline, bind descriptors, and dispatch compute
    vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_COMPUTE, m_Pipeline);
    vkCmdBindDescriptorSets(
        cmd,
        VK_PIPELINE_BIND_POINT_COMPUTE,
        m_PipelineLayout,
        0, 1, &activeSet,
        0, nullptr
    );

    uint32_t groupCount = (maxInstanceCount + 63) / 64;
    vkCmdDispatch(cmd, groupCount, 1, 1);

    // 6. Barrier: Compute Shader write -> Draw Indirect reading (VK_PIPELINE_STAGE_DRAW_INDIRECT_BIT)
    std::array<VkBufferMemoryBarrier, 2> postBarriers{};

    postBarriers[0].sType               = VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER;
    postBarriers[0].srcAccessMask       = VK_ACCESS_SHADER_WRITE_BIT;
    postBarriers[0].dstAccessMask       = VK_ACCESS_INDIRECT_COMMAND_READ_BIT;
    postBarriers[0].srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    postBarriers[0].dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    postBarriers[0].buffer              = frameRes.indirectCommandBuffer;
    postBarriers[0].offset              = 0;
    postBarriers[0].size                = VK_WHOLE_SIZE;

    postBarriers[1].sType               = VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER;
    postBarriers[1].srcAccessMask       = VK_ACCESS_SHADER_WRITE_BIT;
    postBarriers[1].dstAccessMask       = VK_ACCESS_INDIRECT_COMMAND_READ_BIT;
    postBarriers[1].srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    postBarriers[1].dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    postBarriers[1].buffer              = frameRes.indirectCountBuffer;
    postBarriers[1].offset              = 0;
    postBarriers[1].size                = VK_WHOLE_SIZE;

    vkCmdPipelineBarrier(
        cmd,
        VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
        VK_PIPELINE_STAGE_DRAW_INDIRECT_BIT,
        0,
        0, nullptr,
        static_cast<uint32_t>(postBarriers.size()), postBarriers.data(),
        0, nullptr
    );
}

uint32_t IndirectCommandBuildPass::ReadBackDrawCount(EngineResources& resources, uint32_t frameIndex)
{
    if (frameIndex >= m_Frames.size()) return 0;
    
    const IndirectCommandFrameResources& frameRes = m_Frames[frameIndex];
    if (frameRes.indirectCountBuffer == VK_NULL_HANDLE) return 0;

    uint32_t count = 0;
    void* mapped = nullptr;
    VkResult res = vmaMapMemory(resources.allocator, frameRes.indirectCountAlloc, &mapped);
    if (res == VK_SUCCESS && mapped != nullptr) {
        std::memcpy(&count, mapped, sizeof(uint32_t));
        vmaUnmapMemory(resources.allocator, frameRes.indirectCountAlloc);
    } else {
        LOG_ERROR("IndirectCommandBuildPass: Failed to map indirectCountBuffer memory for readback!");
    }
    return count;
}

} // namespace eng::renderer
