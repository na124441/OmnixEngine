#include "Core/pch.h"
#include "OcclusionCullPass.h"
#include "Rendering/GPUScene/GPUScene.h"
#include "Renderer/scene/Texture.h"
#include "Core/Vulkan/VkUtils.h"
#include "Core/Engine/Log.h"
#include "Core/Engine/VmaHelpers.h"
#include "Core/Engine/ResourceTracker.h"
#include <cstring>
#include <algorithm>
#include <array>

namespace eng::renderer {

    struct OcclusionPushConstants {
        uint32_t maxInstanceCount;
        uint32_t frustumOnlyMode;
        float depthBias;
    };

    void OcclusionCullPass::Initialize(EngineResources& resources, VkDescriptorSetLayout gbufferLayout) {
        m_GbufferDescriptorSetLayout = gbufferLayout;
        LOG_INFO("OcclusionCullPass: Initializing...");

        // Create Descriptor Set Layout
        VkDescriptorSetLayoutBinding bindings[8]{};
        // 0: InstanceBuffer (Storage)
        bindings[0].binding = 0;
        bindings[0].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
        bindings[0].descriptorCount = 1;
        bindings[0].stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;
        
        // 1: FrustumVisibleInstanceBuffer (Storage)
        bindings[1].binding = 1;
        bindings[1].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
        bindings[1].descriptorCount = 1;
        bindings[1].stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;

        // 2: FrustumVisibleCountBuffer (Storage)
        bindings[2].binding = 2;
        bindings[2].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
        bindings[2].descriptorCount = 1;
        bindings[2].stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;

        // 3: CameraUBO (Uniform)
        bindings[3].binding = 3;
        bindings[3].descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
        bindings[3].descriptorCount = 1;
        bindings[3].stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;

        // 4: hzbTexture (Combined Image Sampler)
        bindings[4].binding = 4;
        bindings[4].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
        bindings[4].descriptorCount = 1;
        bindings[4].stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;

        // 5: FinalVisibleInstanceBuffer (Storage)
        bindings[5].binding = 5;
        bindings[5].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
        bindings[5].descriptorCount = 1;
        bindings[5].stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;

        // 6: FinalVisibleCountBuffer (Storage)
        bindings[6].binding = 6;
        bindings[6].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
        bindings[6].descriptorCount = 1;
        bindings[6].stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;

        // 7: OcclusionCullStatsBuffer (Storage)
        bindings[7].binding = 7;
        bindings[7].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
        bindings[7].descriptorCount = 1;
        bindings[7].stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;

        VkDescriptorSetLayoutCreateInfo layoutInfo{};
        layoutInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
        layoutInfo.bindingCount = 8;
        layoutInfo.pBindings = bindings;

        VK_CHECK(vkCreateDescriptorSetLayout(resources.device, &layoutInfo, nullptr, &m_DescriptorSetLayout));

        // Create Pipeline Layout
        VkPushConstantRange pushRange{};
        pushRange.stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;
        pushRange.offset = 0;
        pushRange.size = sizeof(OcclusionPushConstants);

        VkPipelineLayoutCreateInfo pipelineLayoutInfo{};
        pipelineLayoutInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
        pipelineLayoutInfo.setLayoutCount = 1;
        pipelineLayoutInfo.pSetLayouts = &m_DescriptorSetLayout;
        pipelineLayoutInfo.pushConstantRangeCount = 1;
        pipelineLayoutInfo.pPushConstantRanges = &pushRange;

        VK_CHECK(vkCreatePipelineLayout(resources.device, &pipelineLayoutInfo, nullptr, &m_PipelineLayout));

        createPipeline(resources);

        m_Frames.resize(resources.MAX_FRAMES_IN_FLIGHT);
        for (uint32_t i = 0; i < resources.MAX_FRAMES_IN_FLIGHT; ++i) {
            createFrameResources(resources, m_Frames[i], gbufferLayout);
        }

        LOG_INFO("OcclusionCullPass: Initialization complete.");
    }

    void OcclusionCullPass::Shutdown(EngineResources& resources) {
        LOG_INFO("OcclusionCullPass: Shutting down...");

        for (auto& frameRes : m_Frames) {
            destroyFrameResources(resources, frameRes);
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

        LOG_INFO("OcclusionCullPass: Shutdown complete.");
    }

    void OcclusionCullPass::createPipeline(EngineResources& resources) {
        VkShaderModule computeShader = resources.loadShaderModule("shaders/occlusion_cull.spv");
        if (computeShader == VK_NULL_HANDLE) {
            LOG_ERROR("OcclusionCullPass: Failed to load compute shader shaders/occlusion_cull.spv");
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
    }

    void OcclusionCullPass::createFrameResources(EngineResources& resources, OcclusionCullFrameResources& frameRes, VkDescriptorSetLayout gbufferLayout) {
        VkDescriptorPoolSize poolSizes[3]{};
        poolSizes[0].type = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
        poolSizes[0].descriptorCount = 32;
        poolSizes[1].type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
        poolSizes[1].descriptorCount = 32;
        poolSizes[2].type = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
        poolSizes[2].descriptorCount = 32;

        VkDescriptorPoolCreateInfo poolInfo{};
        poolInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
        poolInfo.poolSizeCount = 3;
        poolInfo.pPoolSizes = poolSizes;
        poolInfo.maxSets = 8;
        poolInfo.flags = VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT;

        VK_CHECK(vkCreateDescriptorPool(resources.device, &poolInfo, nullptr, &frameRes.descriptorPool));

        VkDescriptorSetAllocateInfo setAlloc{};
        setAlloc.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
        setAlloc.descriptorPool = frameRes.descriptorPool;
        setAlloc.descriptorSetCount = 1;
        setAlloc.pSetLayouts = &m_DescriptorSetLayout;

        VK_CHECK(vkAllocateDescriptorSets(resources.device, &setAlloc, &frameRes.descriptorSet));



        // Create Count Buffers (visible count and culled count)
        VkBufferCreateInfo bufInfo{};
        bufInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
        bufInfo.size = sizeof(uint32_t);
        bufInfo.usage = VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_TRANSFER_SRC_BIT;
        bufInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

        VmaAllocationCreateInfo vmaAllocInfo{};
        vmaAllocInfo.usage = VMA_MEMORY_USAGE_CPU_TO_GPU;

        VK_CHECK(vmaCreateBuffer(resources.allocator, &bufInfo, &vmaAllocInfo, &frameRes.finalVisibleCountBuffer, &frameRes.finalVisibleCountAlloc, nullptr));
        ::eng::ResourceTracker::incBuffer();

        VK_CHECK(vmaCreateBuffer(resources.allocator, &bufInfo, &vmaAllocInfo, &frameRes.occlusionCulledCountBuffer, &frameRes.occlusionCulledCountAlloc, nullptr));
        ::eng::ResourceTracker::incBuffer();
    }

    void OcclusionCullPass::destroyFrameResources(EngineResources& resources, OcclusionCullFrameResources& frameRes) {
        if (frameRes.finalVisibleInstanceBuffer != VK_NULL_HANDLE) {
            vmaDestroyBuffer(resources.allocator, frameRes.finalVisibleInstanceBuffer, frameRes.finalVisibleInstanceAlloc);
            ::eng::ResourceTracker::decBuffer();
            frameRes.finalVisibleInstanceBuffer = VK_NULL_HANDLE;
        }

        if (frameRes.finalVisibleCountBuffer != VK_NULL_HANDLE) {
            vmaDestroyBuffer(resources.allocator, frameRes.finalVisibleCountBuffer, frameRes.finalVisibleCountAlloc);
            ::eng::ResourceTracker::decBuffer();
            frameRes.finalVisibleCountBuffer = VK_NULL_HANDLE;
        }

        if (frameRes.occlusionCulledCountBuffer != VK_NULL_HANDLE) {
            vmaDestroyBuffer(resources.allocator, frameRes.occlusionCulledCountBuffer, frameRes.occlusionCulledCountAlloc);
            ::eng::ResourceTracker::decBuffer();
            frameRes.occlusionCulledCountBuffer = VK_NULL_HANDLE;
        }

        if (frameRes.descriptorPool != VK_NULL_HANDLE) {
            vkDestroyDescriptorPool(resources.device, frameRes.descriptorPool, nullptr);
            frameRes.descriptorPool = VK_NULL_HANDLE;
        }
    }

    void OcclusionCullPass::resizeFinalVisibleInstanceBufferIfNeeded(EngineResources& resources, OcclusionCullFrameResources& frameRes, uint32_t neededCapacity) {
        if (frameRes.finalVisibleInstanceBuffer != VK_NULL_HANDLE && frameRes.finalVisibleInstanceCapacity >= neededCapacity) {
            return;
        }

        uint32_t newCapacity = std::max(frameRes.finalVisibleInstanceCapacity * 2, 128u);
        while (newCapacity < neededCapacity) {
            newCapacity *= 2;
        }

        VkDeviceSize newSize = newCapacity * sizeof(uint32_t);

        if (frameRes.finalVisibleInstanceBuffer != VK_NULL_HANDLE) {
            vmaDestroyBuffer(resources.allocator, frameRes.finalVisibleInstanceBuffer, frameRes.finalVisibleInstanceAlloc);
            ::eng::ResourceTracker::decBuffer();
            frameRes.finalVisibleInstanceBuffer = VK_NULL_HANDLE;
        }

        VkBufferCreateInfo bufInfo{};
        bufInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
        bufInfo.size = newSize;
        bufInfo.usage = VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT;
        bufInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

        VmaAllocationCreateInfo allocInfo{};
        allocInfo.usage = VMA_MEMORY_USAGE_CPU_TO_GPU;

        VK_CHECK(vmaCreateBuffer(resources.allocator, &bufInfo, &allocInfo, &frameRes.finalVisibleInstanceBuffer, &frameRes.finalVisibleInstanceAlloc, nullptr));
        ::eng::ResourceTracker::incBuffer();

        frameRes.finalVisibleInstanceCapacity = newCapacity;
        frameRes.finalVisibleInstanceBufferSize = newSize;
    }

    void OcclusionCullPass::writeDescriptorSet(
        EngineResources& resources,
        OcclusionCullFrameResources& frameRes,
        VkBuffer instanceBuffer,
        VkDeviceSize instanceBufferSize,
        VkBuffer frustumBuffer,
        VkBuffer frustumVisibleBuffer,
        VkDeviceSize frustumVisibleBufferSize,
        VkBuffer frustumVisibleCountBuffer,
        VkBuffer cameraBuffer,
        VkImageView hzbSRV,
        VkSampler hzbSampler
    ) {
        VkDescriptorBufferInfo instanceBufInfo{};
        instanceBufInfo.buffer = instanceBuffer;
        instanceBufInfo.offset = 0;
        instanceBufInfo.range = instanceBufferSize;

        VkDescriptorBufferInfo frustumVisibleBufInfo{};
        frustumVisibleBufInfo.buffer = frustumVisibleBuffer;
        frustumVisibleBufInfo.offset = 0;
        frustumVisibleBufInfo.range = frustumVisibleBufferSize;

        VkDescriptorBufferInfo frustumVisibleCountBufInfo{};
        frustumVisibleCountBufInfo.buffer = frustumVisibleCountBuffer;
        frustumVisibleCountBufInfo.offset = 0;
        frustumVisibleCountBufInfo.range = VK_WHOLE_SIZE;

        VkDescriptorBufferInfo cameraBufInfo{};
        cameraBufInfo.buffer = cameraBuffer;
        cameraBufInfo.offset = 0;
        cameraBufInfo.range = VK_WHOLE_SIZE;

        VkDescriptorImageInfo hzbImgInfo{};
        hzbImgInfo.imageView = hzbSRV != VK_NULL_HANDLE ? hzbSRV : Texture::getWhiteTexture(resources)->view();
        hzbImgInfo.sampler = hzbSampler != VK_NULL_HANDLE ? hzbSampler : Texture::getWhiteTexture(resources)->sampler();
        hzbImgInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

        VkDescriptorBufferInfo finalVisibleBufInfo{};
        finalVisibleBufInfo.buffer = frameRes.finalVisibleInstanceBuffer;
        finalVisibleBufInfo.offset = 0;
        finalVisibleBufInfo.range = frameRes.finalVisibleInstanceBufferSize;

        VkDescriptorBufferInfo finalVisibleCountBufInfo{};
        finalVisibleCountBufInfo.buffer = frameRes.finalVisibleCountBuffer;
        finalVisibleCountBufInfo.offset = 0;
        finalVisibleCountBufInfo.range = VK_WHOLE_SIZE;

        VkDescriptorBufferInfo statsBufInfo{};
        statsBufInfo.buffer = frameRes.occlusionCulledCountBuffer;
        statsBufInfo.offset = 0;
        statsBufInfo.range = VK_WHOLE_SIZE;

        VkWriteDescriptorSet writes[8]{};
        for (int i = 0; i < 8; ++i) {
            writes[i].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
            writes[i].dstSet = frameRes.descriptorSet;
            writes[i].dstBinding = i;
            writes[i].descriptorCount = 1;
        }

        writes[0].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
        writes[0].pBufferInfo = &instanceBufInfo;

        writes[1].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
        writes[1].pBufferInfo = &frustumVisibleBufInfo;

        writes[2].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
        writes[2].pBufferInfo = &frustumVisibleCountBufInfo;

        writes[3].descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
        writes[3].pBufferInfo = &cameraBufInfo;

        writes[4].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
        writes[4].pImageInfo = &hzbImgInfo;

        writes[5].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
        writes[5].pBufferInfo = &finalVisibleBufInfo;

        writes[6].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
        writes[6].pBufferInfo = &finalVisibleCountBufInfo;

        writes[7].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
        writes[7].pBufferInfo = &statsBufInfo;

        vkUpdateDescriptorSets(resources.device, 8, writes, 0, nullptr);
    }

    void OcclusionCullPass::Execute(
        VkCommandBuffer cmd,
        EngineResources& resources,
        uint32_t frameIndex,
        GPUScene& gpuScene,
        VkBuffer frustumVisibleBuffer,
        VkDeviceSize frustumVisibleBufferSize,
        VkBuffer frustumVisibleCountBuffer,
        VkImageView hzbSRV,
        VkSampler hzbSampler,
        uint32_t instanceCount,
        bool frustumOnlyMode
    ) {
        auto& frameRes = m_Frames[frameIndex];

        // 1. Resize final visible buffer
        resizeFinalVisibleInstanceBufferIfNeeded(resources, frameRes, instanceCount);

        // 2. Initialize counts to 0
        uint32_t zero = 0;
        void* mappedCount = nullptr;
        if (vmaMapMemory(resources.allocator, frameRes.finalVisibleCountAlloc, &mappedCount) == VK_SUCCESS) {
            std::memcpy(mappedCount, &zero, sizeof(uint32_t));
            vmaUnmapMemory(resources.allocator, frameRes.finalVisibleCountAlloc);
        }
        void* mappedStats = nullptr;
        if (vmaMapMemory(resources.allocator, frameRes.occlusionCulledCountAlloc, &mappedStats) == VK_SUCCESS) {
            std::memcpy(mappedStats, &zero, sizeof(uint32_t));
            vmaUnmapMemory(resources.allocator, frameRes.occlusionCulledCountAlloc);
        }

        // 3. Write descriptor set
        const auto& sceneRes = gpuScene.GetFrameResources(frameIndex);
        VkBuffer instanceBuffer = sceneRes.instanceBuffer;
        VkDeviceSize instanceBufferSize = sceneRes.instanceBufferSize;
        VkBuffer cameraBuffer = sceneRes.cameraBuffer;

        writeDescriptorSet(
            resources,
            frameRes,
            instanceBuffer,
            instanceBufferSize,
            sceneRes.frustumBuffer,
            frustumVisibleBuffer,
            frustumVisibleBufferSize,
            frustumVisibleCountBuffer,
            cameraBuffer,
            hzbSRV,
            hzbSampler
        );

        // 4. Dispatch compute shader
        vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_COMPUTE, m_Pipeline);
        vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_COMPUTE, m_PipelineLayout, 0, 1, &frameRes.descriptorSet, 0, nullptr);

        OcclusionPushConstants pc{};
        pc.maxInstanceCount = instanceCount;
        pc.frustumOnlyMode = frustumOnlyMode ? 1 : 0;
        pc.depthBias = 0.0001f;

        vkCmdPushConstants(cmd, m_PipelineLayout, VK_SHADER_STAGE_COMPUTE_BIT, 0, sizeof(OcclusionPushConstants), &pc);

        // We dispatch based on instanceCount (conservatively, the size of input visible items)
        uint32_t groupCount = (instanceCount + 63) / 64;
        vkCmdDispatch(cmd, groupCount, 1, 1);

        // 5. Memory barrier for readback
        std::array<VkBufferMemoryBarrier, 3> barriers{};
        barriers[0].sType = VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER;
        barriers[0].srcAccessMask = VK_ACCESS_SHADER_WRITE_BIT;
        barriers[0].dstAccessMask = VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_TRANSFER_READ_BIT;
        barriers[0].srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        barriers[0].dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        barriers[0].buffer = frameRes.finalVisibleInstanceBuffer;
        barriers[0].offset = 0;
        barriers[0].size = VK_WHOLE_SIZE;

        barriers[1].sType = VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER;
        barriers[1].srcAccessMask = VK_ACCESS_SHADER_WRITE_BIT;
        barriers[1].dstAccessMask = VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_TRANSFER_READ_BIT | VK_ACCESS_HOST_READ_BIT;
        barriers[1].srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        barriers[1].dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        barriers[1].buffer = frameRes.finalVisibleCountBuffer;
        barriers[1].offset = 0;
        barriers[1].size = VK_WHOLE_SIZE;

        barriers[2].sType = VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER;
        barriers[2].srcAccessMask = VK_ACCESS_SHADER_WRITE_BIT;
        barriers[2].dstAccessMask = VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_TRANSFER_READ_BIT | VK_ACCESS_HOST_READ_BIT;
        barriers[2].srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        barriers[2].dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        barriers[2].buffer = frameRes.occlusionCulledCountBuffer;
        barriers[2].offset = 0;
        barriers[2].size = VK_WHOLE_SIZE;

        vkCmdPipelineBarrier(cmd,
            VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
            VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT | VK_PIPELINE_STAGE_TRANSFER_BIT | VK_PIPELINE_STAGE_HOST_BIT,
            0,
            0, nullptr,
            static_cast<uint32_t>(barriers.size()), barriers.data(),
            0, nullptr);
    }

    uint32_t OcclusionCullPass::ReadBackFinalCount(EngineResources& resources, uint32_t frameIndex) {
        if (frameIndex >= m_Frames.size()) return 0;
        const auto& frameRes = m_Frames[frameIndex];
        if (frameRes.finalVisibleCountBuffer == VK_NULL_HANDLE) return 0;

        uint32_t count = 0;
        void* mapped = nullptr;
        if (vmaMapMemory(resources.allocator, frameRes.finalVisibleCountAlloc, &mapped) == VK_SUCCESS) {
            std::memcpy(&count, mapped, sizeof(uint32_t));
            vmaUnmapMemory(resources.allocator, frameRes.finalVisibleCountAlloc);
        }
        return count;
    }

    uint32_t OcclusionCullPass::ReadBackCulledCount(EngineResources& resources, uint32_t frameIndex) {
        if (frameIndex >= m_Frames.size()) return 0;
        const auto& frameRes = m_Frames[frameIndex];
        if (frameRes.occlusionCulledCountBuffer == VK_NULL_HANDLE) return 0;

        uint32_t count = 0;
        void* mapped = nullptr;
        if (vmaMapMemory(resources.allocator, frameRes.occlusionCulledCountAlloc, &mapped) == VK_SUCCESS) {
            std::memcpy(&count, mapped, sizeof(uint32_t));
            vmaUnmapMemory(resources.allocator, frameRes.occlusionCulledCountAlloc);
        }
        return count;
    }

    std::vector<uint32_t> OcclusionCullPass::ReadBackFinalVisibleInstances(EngineResources& resources, uint32_t frameIndex, uint32_t count) {
        std::vector<uint32_t> visibleIndices;
        if (count == 0 || frameIndex >= m_Frames.size()) return visibleIndices;
        const auto& frameRes = m_Frames[frameIndex];
        if (frameRes.finalVisibleInstanceBuffer == VK_NULL_HANDLE) return visibleIndices;

        visibleIndices.resize(count);
        void* mapped = nullptr;
        if (vmaMapMemory(resources.allocator, frameRes.finalVisibleInstanceAlloc, &mapped) == VK_SUCCESS) {
            std::memcpy(visibleIndices.data(), mapped, count * sizeof(uint32_t));
            vmaUnmapMemory(resources.allocator, frameRes.finalVisibleInstanceAlloc);
        } else {
            visibleIndices.clear();
        }
        return visibleIndices;
    }

} // namespace eng::renderer
