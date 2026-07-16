#include "Core/pch.h"
#include "RVGClusterCullPass.h"
#include "Rendering/GPUScene/GPUScene.h"
#include "Rendering/Geometry/Assets/RVGRegistry.h"
#include "Rendering/Geometry/Streaming/RVGPageStreamingManager.h"
#include "Renderer/scene/Texture.h"
#include "Core/Vulkan/VkUtils.h"
#include "Core/Engine/Log.h"
#include "Core/Engine/VmaHelpers.h"
#include "Core/Engine/ResourceTracker.h"
#include <cstring>
#include <algorithm>
#include <array>

namespace eng::renderer {

    struct RVGCullPushConstants {
        uint32_t maxInstanceCount = 0;
        uint32_t frustumOnlyMode = 0;
        float depthBias = 0.0001f;
        float lodBias = 0.0f;
        float targetPixelError = 2.0f;
        uint32_t maxTraversalDepth = 16;
        uint32_t debugMode = 0;
        uint32_t forceRoot = 0;
        uint32_t forceFullDetail = 0;
    };

    void RVGClusterCullPass::Initialize(EngineResources& resources) {
        LOG_INFO("RVGClusterCullPass: Initializing...");

        // Create Descriptor Set Layout
        VkDescriptorSetLayoutBinding bindings[11]{};
        
        // 0: InstanceBuffer (Storage)
        bindings[0].binding = 0;
        bindings[0].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
        bindings[0].descriptorCount = 1;
        bindings[0].stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;

        // 1: RVGAssetBuffer (Storage)
        bindings[1].binding = 1;
        bindings[1].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
        bindings[1].descriptorCount = 1;
        bindings[1].stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;

        // 2: RVGClusterBuffer (Storage)
        bindings[2].binding = 2;
        bindings[2].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
        bindings[2].descriptorCount = 1;
        bindings[2].stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;

        // 3: ClusterIndirectCommandBuffer (Storage)
        bindings[3].binding = 3;
        bindings[3].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
        bindings[3].descriptorCount = 1;
        bindings[3].stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;

        // 4: ClusterIndirectCountBuffer (Storage)
        bindings[4].binding = 4;
        bindings[4].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
        bindings[4].descriptorCount = 1;
        bindings[4].stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;

        // 5: CameraUBO (Uniform)
        bindings[5].binding = 5;
        bindings[5].descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
        bindings[5].descriptorCount = 1;
        bindings[5].stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;

        // 6: FrustumBuffer (Uniform)
        bindings[6].binding = 6;
        bindings[6].descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
        bindings[6].descriptorCount = 1;
        bindings[6].stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;

        // 7: hzbTexture (Combined Image Sampler)
        bindings[7].binding = 7;
        bindings[7].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
        bindings[7].descriptorCount = 1;
        bindings[7].stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;

        // 8: RVGNodeBuffer (Storage)
        bindings[8].binding = 8;
        bindings[8].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
        bindings[8].descriptorCount = 1;
        bindings[8].stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;

        // 9: Virtual Page Table (Storage)
        bindings[9].binding = 9;
        bindings[9].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
        bindings[9].descriptorCount = 1;
        bindings[9].stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;

        // 10: Streaming Request Buffer (Storage)
        bindings[10].binding = 10;
        bindings[10].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
        bindings[10].descriptorCount = 1;
        bindings[10].stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;

        VkDescriptorSetLayoutCreateInfo layoutInfo{};
        layoutInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
        layoutInfo.bindingCount = 11;
        layoutInfo.pBindings = bindings;

        VK_CHECK(vkCreateDescriptorSetLayout(resources.device, &layoutInfo, nullptr, &m_DescriptorSetLayout));

        // Create Pipeline Layout
        VkPushConstantRange pushRange{};
        pushRange.stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;
        pushRange.offset = 0;
        pushRange.size = sizeof(RVGCullPushConstants);

        VkPipelineLayoutCreateInfo pipelineLayoutInfo{};
        pipelineLayoutInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
        pipelineLayoutInfo.setLayoutCount = 1;
        pipelineLayoutInfo.pSetLayouts = &m_DescriptorSetLayout;
        pipelineLayoutInfo.pushConstantRangeCount = 1;
        pipelineLayoutInfo.pPushConstantRanges = &pushRange;

        VK_CHECK(vkCreatePipelineLayout(resources.device, &pipelineLayoutInfo, nullptr, &m_PipelineLayout));

        // Create Pipeline
        createPipeline(resources);

        // Allocate frame resources
        m_Frames.resize(resources.MAX_FRAMES_IN_FLIGHT);
        for (uint32_t i = 0; i < resources.MAX_FRAMES_IN_FLIGHT; ++i) {
            createFrameResources(resources, m_Frames[i]);
        }

        LOG_INFO("RVGClusterCullPass: Initialization complete.");
    }

    void RVGClusterCullPass::Shutdown(EngineResources& resources) {
        LOG_INFO("RVGClusterCullPass: Shutting down...");

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

        LOG_INFO("RVGClusterCullPass: Shutdown complete.");
    }

    void RVGClusterCullPass::createPipeline(EngineResources& resources) {
        VkShaderModule computeShader = resources.loadShaderModule("shaders/rvg_cull.spv");
        if (computeShader == VK_NULL_HANDLE) {
            LOG_ERROR("RVGClusterCullPass: Failed to load compute shader shaders/rvg_cull.spv");
            return;
        }

        VkComputePipelineCreateInfo pipelineInfo{};
        pipelineInfo.sType = VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO;
        pipelineInfo.layout = m_PipelineLayout;
        pipelineInfo.stage.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
        pipelineInfo.stage.stage = VK_SHADER_STAGE_COMPUTE_BIT;
        pipelineInfo.stage.module = computeShader;
        pipelineInfo.stage.pName = "main";

        VK_CHECK(vkCreateComputePipelines(resources.device, VK_NULL_HANDLE, 1, &pipelineInfo, nullptr, &m_Pipeline));
        vkDestroyShaderModule(resources.device, computeShader, nullptr);
    }

    void RVGClusterCullPass::createFrameResources(EngineResources& resources, RVGClusterCullFrameResources& frameRes) {
        // Descriptor Pool
        VkDescriptorPoolSize poolSizes[2]{};
        poolSizes[0].type = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
        poolSizes[0].descriptorCount = 8;
        poolSizes[1].type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
        poolSizes[1].descriptorCount = 4;

        VkDescriptorPoolCreateInfo poolInfo{};
        poolInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
        poolInfo.poolSizeCount = 2;
        poolInfo.pPoolSizes = poolSizes;
        poolInfo.maxSets = 4;
        poolInfo.flags = VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT;

        VK_CHECK(vkCreateDescriptorPool(resources.device, &poolInfo, nullptr, &frameRes.descriptorPool));

        // Descriptor Set
        VkDescriptorSetAllocateInfo setAlloc{};
        setAlloc.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
        setAlloc.descriptorPool = frameRes.descriptorPool;
        setAlloc.descriptorSetCount = 1;
        setAlloc.pSetLayouts = &m_DescriptorSetLayout;

        VK_CHECK(vkAllocateDescriptorSets(resources.device, &setAlloc, &frameRes.descriptorSet));

        // Create Count Buffer (initialized to 0)
        VkBufferCreateInfo countBufInfo{};
        countBufInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
        countBufInfo.size = sizeof(uint32_t);
        countBufInfo.usage = VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_INDIRECT_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_SRC_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT;
        countBufInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

        VmaAllocationCreateInfo countAllocInfo{};
        countAllocInfo.usage = VMA_MEMORY_USAGE_CPU_TO_GPU;

        VK_CHECK(vmaCreateBuffer(resources.allocator, &countBufInfo, &countAllocInfo, &frameRes.indirectCountBuffer, &frameRes.indirectCountAlloc, nullptr));
        ::eng::ResourceTracker::incBuffer();

        // Initial dynamic command buffer allocation (default capacity 256 cluster draws)
        frameRes.indirectCommandCapacity = 256;
        VkBufferCreateInfo cmdBufInfo{};
        cmdBufInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
        cmdBufInfo.size = frameRes.indirectCommandCapacity * sizeof(VkDrawIndexedIndirectCommand);
        cmdBufInfo.usage = VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_INDIRECT_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_SRC_BIT;
        cmdBufInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

        VmaAllocationCreateInfo cmdAllocInfo{};
        cmdAllocInfo.usage = VMA_MEMORY_USAGE_CPU_TO_GPU;

        VK_CHECK(vmaCreateBuffer(resources.allocator, &cmdBufInfo, &cmdAllocInfo, &frameRes.indirectCommandBuffer, &frameRes.indirectCommandAlloc, nullptr));
        frameRes.indirectCommandBufferSize = cmdBufInfo.size;
        ::eng::ResourceTracker::incBuffer();
    }

    void RVGClusterCullPass::destroyFrameResources(EngineResources& resources, RVGClusterCullFrameResources& frameRes) {
        if (frameRes.descriptorPool != VK_NULL_HANDLE) {
            vkDestroyDescriptorPool(resources.device, frameRes.descriptorPool, nullptr);
            frameRes.descriptorPool = VK_NULL_HANDLE;
            frameRes.descriptorSet = VK_NULL_HANDLE;
        }

        if (frameRes.indirectCountBuffer != VK_NULL_HANDLE) {
            vmaDestroyBuffer(resources.allocator, frameRes.indirectCountBuffer, frameRes.indirectCountAlloc);
            frameRes.indirectCountBuffer = VK_NULL_HANDLE;
            frameRes.indirectCountAlloc = VK_NULL_HANDLE;
            ::eng::ResourceTracker::decBuffer();
        }

        if (frameRes.indirectCommandBuffer != VK_NULL_HANDLE) {
            vmaDestroyBuffer(resources.allocator, frameRes.indirectCommandBuffer, frameRes.indirectCommandAlloc);
            frameRes.indirectCommandBuffer = VK_NULL_HANDLE;
            frameRes.indirectCommandAlloc = VK_NULL_HANDLE;
            frameRes.indirectCommandBufferSize = 0;
            frameRes.indirectCommandCapacity = 0;
            ::eng::ResourceTracker::decBuffer();
        }
    }

    void RVGClusterCullPass::resizeIndirectCommandBufferIfNeeded(
        EngineResources& resources,
        RVGClusterCullFrameResources& frameRes,
        uint32_t neededCapacity
    ) {
        if (neededCapacity <= frameRes.indirectCommandCapacity) return;

        // Destroy previous buffer
        if (frameRes.indirectCommandBuffer != VK_NULL_HANDLE) {
            vkDeviceWaitIdle(resources.device);
            vmaDestroyBuffer(resources.allocator, frameRes.indirectCommandBuffer, frameRes.indirectCommandAlloc);
            ::eng::ResourceTracker::decBuffer();
        }

        frameRes.indirectCommandCapacity = std::max(neededCapacity, frameRes.indirectCommandCapacity * 2);
        
        VkBufferCreateInfo cmdBufInfo{};
        cmdBufInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
        cmdBufInfo.size = frameRes.indirectCommandCapacity * sizeof(VkDrawIndexedIndirectCommand);
        cmdBufInfo.usage = VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_INDIRECT_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_SRC_BIT;
        cmdBufInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

        VmaAllocationCreateInfo cmdAllocInfo{};
        cmdAllocInfo.usage = VMA_MEMORY_USAGE_CPU_TO_GPU;

        VK_CHECK(vmaCreateBuffer(resources.allocator, &cmdBufInfo, &cmdAllocInfo, &frameRes.indirectCommandBuffer, &frameRes.indirectCommandAlloc, nullptr));
        frameRes.indirectCommandBufferSize = cmdBufInfo.size;
        ::eng::ResourceTracker::incBuffer();

        LOG_INFO("RVGClusterCullPass: Resized indirectCommandBuffer to capacity " + std::to_string(frameRes.indirectCommandCapacity));
    }

    void RVGClusterCullPass::writeDescriptorSet(
        EngineResources& resources,
        RVGClusterCullFrameResources& frameRes,
        uint32_t frameIndex,
        VkBuffer instanceBuffer,
        VkDeviceSize instanceBufferSize,
        VkBuffer rvgAssetBuffer,
        VkBuffer rvgClusterBuffer,
        VkBuffer rvgNodeBuffer,
        VkBuffer cameraBuffer,
        VkBuffer frustumBuffer,
        VkImageView hzbSRV,
        VkSampler hzbSampler
    ) {
        VkDescriptorBufferInfo instanceBufInfo{};
        instanceBufInfo.buffer = instanceBuffer;
        instanceBufInfo.offset = 0;
        instanceBufInfo.range = instanceBufferSize;

        VkDescriptorBufferInfo assetBufInfo{};
        assetBufInfo.buffer = rvgAssetBuffer;
        assetBufInfo.offset = 0;
        assetBufInfo.range = VK_WHOLE_SIZE;

        VkDescriptorBufferInfo clusterBufInfo{};
        clusterBufInfo.buffer = rvgClusterBuffer;
        clusterBufInfo.offset = 0;
        clusterBufInfo.range = VK_WHOLE_SIZE;

        VkDescriptorBufferInfo cmdBufInfo{};
        cmdBufInfo.buffer = frameRes.indirectCommandBuffer;
        cmdBufInfo.offset = 0;
        cmdBufInfo.range = frameRes.indirectCommandBufferSize;

        VkDescriptorBufferInfo countBufInfo{};
        countBufInfo.buffer = frameRes.indirectCountBuffer;
        countBufInfo.offset = 0;
        countBufInfo.range = VK_WHOLE_SIZE;

        VkDescriptorBufferInfo cameraBufInfo{};
        cameraBufInfo.buffer = cameraBuffer;
        cameraBufInfo.offset = 0;
        cameraBufInfo.range = VK_WHOLE_SIZE;

        VkDescriptorBufferInfo frustumBufInfo{};
        frustumBufInfo.buffer = frustumBuffer;
        frustumBufInfo.offset = 0;
        frustumBufInfo.range = VK_WHOLE_SIZE;

        VkDescriptorImageInfo hzbImgInfo{};
        hzbImgInfo.imageView = hzbSRV != VK_NULL_HANDLE ? hzbSRV : Texture::getWhiteTexture(resources)->view();
        hzbImgInfo.sampler = hzbSampler != VK_NULL_HANDLE ? hzbSampler : Texture::getWhiteTexture(resources)->sampler();
        hzbImgInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

        VkDescriptorBufferInfo nodeBufInfo{};
        nodeBufInfo.buffer = rvgNodeBuffer;
        nodeBufInfo.offset = 0;
        nodeBufInfo.range = VK_WHOLE_SIZE;

        VkBuffer virtualPageTableBuf = RVGPageStreamingManager::Get().GetVirtualPageTableBuffer(frameIndex);
        VkBuffer streamingRequestBuf = RVGPageStreamingManager::Get().GetStreamingRequestBuffer(frameIndex);

        if (virtualPageTableBuf == VK_NULL_HANDLE || streamingRequestBuf == VK_NULL_HANDLE) {
            virtualPageTableBuf = instanceBuffer;
            streamingRequestBuf = instanceBuffer;
        }

        VkDescriptorBufferInfo virtualPageTableInfo{};
        virtualPageTableInfo.buffer = virtualPageTableBuf;
        virtualPageTableInfo.offset = 0;
        virtualPageTableInfo.range  = VK_WHOLE_SIZE;

        VkDescriptorBufferInfo streamingRequestInfo{};
        streamingRequestInfo.buffer = streamingRequestBuf;
        streamingRequestInfo.offset = 0;
        streamingRequestInfo.range  = VK_WHOLE_SIZE;

        VkWriteDescriptorSet writes[11]{};
        for (int i = 0; i < 11; ++i) {
            writes[i].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
            writes[i].dstSet = frameRes.descriptorSet;
            writes[i].dstBinding = i;
            writes[i].descriptorCount = 1;
        }

        writes[0].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
        writes[0].pBufferInfo = &instanceBufInfo;

        writes[1].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
        writes[1].pBufferInfo = &assetBufInfo;

        writes[2].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
        writes[2].pBufferInfo = &clusterBufInfo;

        writes[3].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
        writes[3].pBufferInfo = &cmdBufInfo;

        writes[4].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
        writes[4].pBufferInfo = &countBufInfo;

        writes[5].descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
        writes[5].pBufferInfo = &cameraBufInfo;

        writes[6].descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
        writes[6].pBufferInfo = &frustumBufInfo;

        writes[7].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
        writes[7].pImageInfo = &hzbImgInfo;

        writes[8].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
        writes[8].pBufferInfo = &nodeBufInfo;

        writes[9].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
        writes[9].pBufferInfo = &virtualPageTableInfo;

        writes[10].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
        writes[10].pBufferInfo = &streamingRequestInfo;

        vkUpdateDescriptorSets(resources.device, 11, writes, 0, nullptr);
    }

    void RVGClusterCullPass::Execute(
        VkCommandBuffer cmd,
        EngineResources& resources,
        uint32_t frameIndex,
        GPUScene& gpuScene,
        VkImageView hzbSRV,
        VkSampler hzbSampler,
        uint32_t instanceCount,
        bool frustumOnlyMode,
        float lodBias,
        float targetPixelError,
        uint32_t maxTraversalDepth,
        uint32_t debugMode,
        uint32_t forceRoot,
        uint32_t forceFullDetail
    ) {
        if (instanceCount == 0 || gpuScene.GetDiagnostics().instanceCount == 0) return;

        auto& frameRes = m_Frames[frameIndex];
        const auto& sceneRes = gpuScene.GetFrameResources(frameIndex);
        const auto& globalData = resources.getCurrentFrameData(frameIndex);

        VkBuffer assetTable = RVGRegistry::Get().GetAssetTableBuffer();
        VkBuffer clusters = RVGRegistry::Get().GetClustersBuffer();
        VkBuffer nodes = RVGRegistry::Get().GetNodesBuffer();

        // If no virtual geometry assets loaded yet, bypass culling
        if (assetTable == VK_NULL_HANDLE || clusters == VK_NULL_HANDLE || nodes == VK_NULL_HANDLE) {
            // Just clear draw count to 0
            uint32_t zero = 0;
            void* mapped = nullptr;
            vmaMapMemory(resources.allocator, frameRes.indirectCountAlloc, &mapped);
            std::memcpy(mapped, &zero, sizeof(uint32_t));
            vmaUnmapMemory(resources.allocator, frameRes.indirectCountAlloc);
            return;
        }

        // Calculate maximum potential cluster draws
        uint32_t totalClustersCount = 0;
        for (uint32_t i = 0; i < RVGRegistry::Get().GetAssetCount(); ++i) {
            totalClustersCount += RVGRegistry::Get().GetAsset(i)->GetClusterCount();
        }
        uint32_t neededCapacity = totalClustersCount * instanceCount;
        resizeIndirectCommandBufferIfNeeded(resources, frameRes, neededCapacity);

        // Clear indirect count buffer to 0
        uint32_t zero = 0;
        void* mapped = nullptr;
        VK_CHECK(vmaMapMemory(resources.allocator, frameRes.indirectCountAlloc, &mapped));
        std::memcpy(mapped, &zero, sizeof(uint32_t));
        vmaUnmapMemory(resources.allocator, frameRes.indirectCountAlloc);

        // Update descriptors
        writeDescriptorSet(
            resources,
            frameRes,
            frameIndex,
            sceneRes.instanceBuffer,
            sceneRes.instanceBufferSize,
            assetTable,
            clusters,
            nodes,
            globalData.uboBuffer,
            sceneRes.frustumBuffer,
            hzbSRV,
            hzbSampler
        );

        // Dispatch compute shader
        vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_COMPUTE, m_Pipeline);
        vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_COMPUTE, m_PipelineLayout, 0, 1, &frameRes.descriptorSet, 0, nullptr);

        RVGCullPushConstants pc{};
        pc.maxInstanceCount = instanceCount;
        pc.frustumOnlyMode = frustumOnlyMode ? 1 : 0;
        pc.depthBias = 0.0001f;
        pc.lodBias = lodBias;
        pc.targetPixelError = targetPixelError;
        pc.maxTraversalDepth = maxTraversalDepth;
        pc.debugMode = debugMode;
        pc.forceRoot = forceRoot;
        pc.forceFullDetail = forceFullDetail;

        vkCmdPushConstants(cmd, m_PipelineLayout, VK_SHADER_STAGE_COMPUTE_BIT, 0, sizeof(RVGCullPushConstants), &pc);

        uint32_t groupCount = (instanceCount + 63) / 64;
        vkCmdDispatch(cmd, groupCount, 1, 1);

        // Insert memory barriers to ensure indirect commands are ready for the draw calls
        VkBufferMemoryBarrier barriers[2]{};
        
        barriers[0].sType = VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER;
        barriers[0].srcAccessMask = VK_ACCESS_SHADER_WRITE_BIT;
        barriers[0].dstAccessMask = VK_ACCESS_INDIRECT_COMMAND_READ_BIT;
        barriers[0].buffer = frameRes.indirectCommandBuffer;
        barriers[0].offset = 0;
        barriers[0].size = VK_WHOLE_SIZE;

        barriers[1].sType = VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER;
        barriers[1].srcAccessMask = VK_ACCESS_SHADER_WRITE_BIT;
        barriers[1].dstAccessMask = VK_ACCESS_INDIRECT_COMMAND_READ_BIT;
        barriers[1].buffer = frameRes.indirectCountBuffer;
        barriers[1].offset = 0;
        barriers[1].size = VK_WHOLE_SIZE;

        vkCmdPipelineBarrier(
            cmd,
            VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
            VK_PIPELINE_STAGE_DRAW_INDIRECT_BIT,
            0,
            0, nullptr,
            2, barriers,
            0, nullptr
        );
    }

    uint32_t RVGClusterCullPass::ReadBackDrawCount(EngineResources& resources, uint32_t frameIndex) {
        const auto& frameRes = m_Frames[frameIndex];
        if (frameRes.indirectCountBuffer == VK_NULL_HANDLE) return 0;

        uint32_t count = 0;
        void* mapped = nullptr;
        if (vmaMapMemory(resources.allocator, frameRes.indirectCountAlloc, &mapped) == VK_SUCCESS) {
            std::memcpy(&count, mapped, sizeof(uint32_t));
            vmaUnmapMemory(resources.allocator, frameRes.indirectCountAlloc);
        } else {
            LOG_ERROR("RVGClusterCullPass: Failed to map indirectCountBuffer for host readback.");
        }
        return count;
    }

} // namespace eng::renderer
