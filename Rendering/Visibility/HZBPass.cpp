#include "Core/pch.h"
#include "HZBPass.h"
#include "Core/Vulkan/VkUtils.h"
#include "Core/Engine/Log.h"
#include "Core/Engine/VmaHelpers.h"
#include "Core/Engine/ResourceTracker.h"
#include <algorithm>

namespace eng::renderer {

    static uint32_t CalculateMipCount(uint32_t width, uint32_t height) {
        uint32_t levels = 1;
        while (width > 1 || height > 1) {
            width = std::max(1u, width / 2);
            height = std::max(1u, height / 2);
            levels++;
        }
        return levels;
    }

    struct HZBPushConstants {
        glm::ivec2 size;
    };

    void HZBPass::Initialize(EngineResources& resources) {
        LOG_INFO("HZBPass: Initializing...");

        // 1. Create Descriptor Set Layout
        VkDescriptorSetLayoutBinding bindings[2]{};
        // Binding 0: Input Texture (Sampled Image / depth or prev mip)
        bindings[0].binding            = 0;
        bindings[0].descriptorType     = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
        bindings[0].descriptorCount    = 1;
        bindings[0].stageFlags         = VK_SHADER_STAGE_COMPUTE_BIT;
        bindings[0].pImmutableSamplers = nullptr;

        // Binding 1: Output Texture (Storage Image / current mip)
        bindings[1].binding            = 1;
        bindings[1].descriptorType     = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
        bindings[1].descriptorCount    = 1;
        bindings[1].stageFlags         = VK_SHADER_STAGE_COMPUTE_BIT;
        bindings[1].pImmutableSamplers = nullptr;

        VkDescriptorSetLayoutCreateInfo layoutInfo{};
        layoutInfo.sType        = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
        layoutInfo.bindingCount = 2;
        layoutInfo.pBindings    = bindings;

        VK_CHECK(vkCreateDescriptorSetLayout(resources.device, &layoutInfo, nullptr, &m_DescriptorSetLayout));

        // 2. Create Pipeline Layout
        VkPushConstantRange pushRange{};
        pushRange.stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;
        pushRange.offset = 0;
        pushRange.size = sizeof(HZBPushConstants);

        VkPipelineLayoutCreateInfo pipelineLayoutInfo{};
        pipelineLayoutInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
        pipelineLayoutInfo.setLayoutCount = 1;
        pipelineLayoutInfo.pSetLayouts = &m_DescriptorSetLayout;
        pipelineLayoutInfo.pushConstantRangeCount = 1;
        pipelineLayoutInfo.pPushConstantRanges = &pushRange;

        VK_CHECK(vkCreatePipelineLayout(resources.device, &pipelineLayoutInfo, nullptr, &m_PipelineLayout));

        // 3. Create Pipelines
        createPipelines(resources);

        // 4. Create Sampler for HZB sampling
        VkSamplerCreateInfo samplerInfo{};
        samplerInfo.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
        samplerInfo.magFilter = VK_FILTER_NEAREST;
        samplerInfo.minFilter = VK_FILTER_NEAREST;
        samplerInfo.mipmapMode = VK_SAMPLER_MIPMAP_MODE_NEAREST;
        samplerInfo.addressModeU = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
        samplerInfo.addressModeV = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
        samplerInfo.addressModeW = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
        samplerInfo.minLod = 0.0f;
        samplerInfo.maxLod = 16.0f;
        VK_CHECK(vkCreateSampler(resources.device, &samplerInfo, nullptr, &m_HZBSampler));

        // 5. Initialize frames vector
        m_Frames.resize(resources.MAX_FRAMES_IN_FLIGHT);

        LOG_INFO("HZBPass: Initialization complete.");
    }

    void HZBPass::Shutdown(EngineResources& resources) {
        LOG_INFO("HZBPass: Shutting down...");

        for (auto& frameRes : m_Frames) {
            destroyFrameResources(resources, frameRes);
        }
        m_Frames.clear();

        if (m_HZBSampler != VK_NULL_HANDLE) {
            vkDestroySampler(resources.device, m_HZBSampler, nullptr);
            m_HZBSampler = VK_NULL_HANDLE;
        }

        if (m_CopyPipeline != VK_NULL_HANDLE) {
            vkDestroyPipeline(resources.device, m_CopyPipeline, nullptr);
            m_CopyPipeline = VK_NULL_HANDLE;
        }

        if (m_DownsamplePipeline != VK_NULL_HANDLE) {
            vkDestroyPipeline(resources.device, m_DownsamplePipeline, nullptr);
            m_DownsamplePipeline = VK_NULL_HANDLE;
        }

        if (m_PipelineLayout != VK_NULL_HANDLE) {
            vkDestroyPipelineLayout(resources.device, m_PipelineLayout, nullptr);
            m_PipelineLayout = VK_NULL_HANDLE;
        }

        if (m_DescriptorSetLayout != VK_NULL_HANDLE) {
            vkDestroyDescriptorSetLayout(resources.device, m_DescriptorSetLayout, nullptr);
            m_DescriptorSetLayout = VK_NULL_HANDLE;
        }

        LOG_INFO("HZBPass: Shutdown complete.");
    }

    void HZBPass::createPipelines(EngineResources& resources) {
        // Depth to HZB Pipeline
        VkShaderModule copyModule = resources.loadShaderModule("shaders/depth_to_hzb.spv");
        if (copyModule == VK_NULL_HANDLE) {
            LOG_ERROR("HZBPass: Failed to load depth_to_hzb.spv");
            return;
        }

        VkComputePipelineCreateInfo copyInfo{};
        copyInfo.sType = VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO;
        copyInfo.layout = m_PipelineLayout;
        copyInfo.stage.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
        copyInfo.stage.stage = VK_SHADER_STAGE_COMPUTE_BIT;
        copyInfo.stage.module = copyModule;
        copyInfo.stage.pName = "main";

        VK_CHECK(vkCreateComputePipelines(resources.device, VK_NULL_HANDLE, 1, &copyInfo, nullptr, &m_CopyPipeline));
        vkDestroyShaderModule(resources.device, copyModule, nullptr);

        // HZB Downsample Pipeline
        VkShaderModule downsampleModule = resources.loadShaderModule("shaders/hzb_downsample.spv");
        if (downsampleModule == VK_NULL_HANDLE) {
            LOG_ERROR("HZBPass: Failed to load hzb_downsample.spv");
            return;
        }

        VkComputePipelineCreateInfo downsampleInfo{};
        downsampleInfo.sType = VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO;
        downsampleInfo.layout = m_PipelineLayout;
        downsampleInfo.stage.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
        downsampleInfo.stage.stage = VK_SHADER_STAGE_COMPUTE_BIT;
        downsampleInfo.stage.module = downsampleModule;
        downsampleInfo.stage.pName = "main";

        VK_CHECK(vkCreateComputePipelines(resources.device, VK_NULL_HANDLE, 1, &downsampleInfo, nullptr, &m_DownsamplePipeline));
        vkDestroyShaderModule(resources.device, downsampleModule, nullptr);

        LOG_INFO("HZBPass: Pipelines created successfully.");
    }

    void HZBPass::destroyFrameResources(EngineResources& resources, HZBFrameResources& frameRes) {
        if (frameRes.descriptorPool != VK_NULL_HANDLE) {
            vkDestroyDescriptorPool(resources.device, frameRes.descriptorPool, nullptr);
            frameRes.descriptorPool = VK_NULL_HANDLE;
        }

        for (auto view : frameRes.mipViews) {
            if (view != VK_NULL_HANDLE) {
                vkDestroyImageView(resources.device, view, nullptr);
            }
        }
        frameRes.mipViews.clear();

        if (frameRes.hzbSRV != VK_NULL_HANDLE) {
            vkDestroyImageView(resources.device, frameRes.hzbSRV, nullptr);
            frameRes.hzbSRV = VK_NULL_HANDLE;
        }

        if (frameRes.hzbImage != VK_NULL_HANDLE) {
            vmaDestroyImage(resources.allocator, frameRes.hzbImage, frameRes.hzbAllocation);
            frameRes.hzbImage = VK_NULL_HANDLE;
            frameRes.hzbAllocation = VK_NULL_HANDLE;
            ::eng::ResourceTracker::decImage();
        }

        frameRes.copyDescriptorSet = VK_NULL_HANDLE;
        frameRes.downsampleDescriptorSets.clear();
        frameRes.width = 0;
        frameRes.height = 0;
        frameRes.mipLevels = 0;
    }

    void HZBPass::recreateFrameResources(EngineResources& resources, HZBFrameResources& frameRes, uint32_t width, uint32_t height) {
        destroyFrameResources(resources, frameRes);

        frameRes.width = width;
        frameRes.height = height;
        frameRes.mipLevels = CalculateMipCount(width, height);

        LOG_INFO("HZBPass: Allocating HZB Texture size " + std::to_string(width) + "x" + std::to_string(height) + " with " + std::to_string(frameRes.mipLevels) + " mips");

        // 1. Create HZB Image
        VkImageCreateInfo imgInfo{};
        imgInfo.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
        imgInfo.imageType = VK_IMAGE_TYPE_2D;
        imgInfo.format = VK_FORMAT_R32_SFLOAT;
        imgInfo.extent.width = width;
        imgInfo.extent.height = height;
        imgInfo.extent.depth = 1;
        imgInfo.mipLevels = frameRes.mipLevels;
        imgInfo.arrayLayers = 1;
        imgInfo.samples = VK_SAMPLE_COUNT_1_BIT;
        imgInfo.tiling = VK_IMAGE_TILING_OPTIMAL;
        imgInfo.usage = VK_IMAGE_USAGE_STORAGE_BIT | VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT;
        imgInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
        imgInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;

        VmaAllocationCreateInfo allocInfo{};
        allocInfo.usage = VMA_MEMORY_USAGE_GPU_ONLY;

        VK_CHECK(vmaCreateImage(resources.allocator, &imgInfo, &allocInfo, &frameRes.hzbImage, &frameRes.hzbAllocation, nullptr));
        ::eng::ResourceTracker::incImage();

        // Set Vulkan Debug Name
        PFN_vkSetDebugUtilsObjectNameEXT pfnSetObjectName = 
            (PFN_vkSetDebugUtilsObjectNameEXT)vkGetDeviceProcAddr(resources.device, "vkSetDebugUtilsObjectNameEXT");
        if (pfnSetObjectName) {
            VkDebugUtilsObjectNameInfoEXT nameInfo{};
            nameInfo.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_OBJECT_NAME_INFO_EXT;
            nameInfo.objectType = VK_OBJECT_TYPE_IMAGE;
            nameInfo.objectHandle = reinterpret_cast<uint64_t>(frameRes.hzbImage);
            std::string name = "HZBImage_" + std::to_string(width) + "x" + std::to_string(height);
            nameInfo.pObjectName = name.c_str();
            pfnSetObjectName(resources.device, &nameInfo);
        }

        // 2. Create Overall SRV
        VkImageViewCreateInfo viewInfo{};
        viewInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
        viewInfo.image = frameRes.hzbImage;
        viewInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
        viewInfo.format = VK_FORMAT_R32_SFLOAT;
        viewInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        viewInfo.subresourceRange.baseMipLevel = 0;
        viewInfo.subresourceRange.levelCount = frameRes.mipLevels;
        viewInfo.subresourceRange.baseArrayLayer = 0;
        viewInfo.subresourceRange.layerCount = 1;

        VK_CHECK(vkCreateImageView(resources.device, &viewInfo, nullptr, &frameRes.hzbSRV));

        // 3. Create Individual Mip Views
        frameRes.mipViews.resize(frameRes.mipLevels);
        for (uint32_t i = 0; i < frameRes.mipLevels; ++i) {
            VkImageViewCreateInfo mipViewInfo{};
            mipViewInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
            mipViewInfo.image = frameRes.hzbImage;
            mipViewInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
            mipViewInfo.format = VK_FORMAT_R32_SFLOAT;
            mipViewInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
            mipViewInfo.subresourceRange.baseMipLevel = i;
            mipViewInfo.subresourceRange.levelCount = 1;
            mipViewInfo.subresourceRange.baseArrayLayer = 0;
            mipViewInfo.subresourceRange.layerCount = 1;

            VK_CHECK(vkCreateImageView(resources.device, &mipViewInfo, nullptr, &frameRes.mipViews[i]));
        }

        // 4. Create Descriptor Pool for this configuration
        uint32_t setNeeded = frameRes.mipLevels; // 1 copy, (mipLevels - 1) downsample steps
        VkDescriptorPoolSize poolSizes[2]{};
        poolSizes[0].type = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
        poolSizes[0].descriptorCount = setNeeded;
        poolSizes[1].type = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
        poolSizes[1].descriptorCount = setNeeded;

        VkDescriptorPoolCreateInfo poolInfo{};
        poolInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
        poolInfo.poolSizeCount = 2;
        poolInfo.pPoolSizes = poolSizes;
        poolInfo.maxSets = setNeeded;
        poolInfo.flags = 0;

        VK_CHECK(vkCreateDescriptorPool(resources.device, &poolInfo, nullptr, &frameRes.descriptorPool));

        // Allocate Descriptor Sets
        std::vector<VkDescriptorSetLayout> layouts(setNeeded, m_DescriptorSetLayout);
        VkDescriptorSetAllocateInfo allocSet{};
        allocSet.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
        allocSet.descriptorPool = frameRes.descriptorPool;
        allocSet.descriptorSetCount = setNeeded;
        allocSet.pSetLayouts = layouts.data();

        std::vector<VkDescriptorSet> allocatedSets(setNeeded);
        VK_CHECK(vkAllocateDescriptorSets(resources.device, &allocSet, allocatedSets.data()));

        frameRes.copyDescriptorSet = allocatedSets[0];
        frameRes.downsampleDescriptorSets.assign(allocatedSets.begin() + 1, allocatedSets.end());
    }

    void HZBPass::Execute(
        VkCommandBuffer cmd,
        EngineResources& resources,
        uint32_t frameIndex,
        VkImageView depthImageView,
        VkImage depthImage,
        uint32_t width,
        uint32_t height
    ) {
        if (width == 0 || height == 0) return;

        auto& frameRes = m_Frames[frameIndex];

        // 1. Reallocate/recreate HZB resources if size changes
        if (frameRes.hzbImage == VK_NULL_HANDLE || frameRes.width != width || frameRes.height != height) {
            recreateFrameResources(resources, frameRes, width, height);
        }

        // 2. Build Descriptor Sets for the frame
        // Set 0: Copy Depth to HZB mip 0
        {
            VkDescriptorImageInfo inputInfo{};
            inputInfo.imageLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL;
            inputInfo.imageView = depthImageView;
            inputInfo.sampler = m_HZBSampler;

            VkDescriptorImageInfo outputInfo{};
            outputInfo.imageLayout = VK_IMAGE_LAYOUT_GENERAL;
            outputInfo.imageView = frameRes.mipViews[0];

            VkWriteDescriptorSet writes[2]{};
            writes[0].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
            writes[0].dstSet = frameRes.copyDescriptorSet;
            writes[0].dstBinding = 0;
            writes[0].descriptorCount = 1;
            writes[0].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
            writes[0].pImageInfo = &inputInfo;

            writes[1].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
            writes[1].dstSet = frameRes.copyDescriptorSet;
            writes[1].dstBinding = 1;
            writes[1].descriptorCount = 1;
            writes[1].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
            writes[1].pImageInfo = &outputInfo;

            vkUpdateDescriptorSets(resources.device, 2, writes, 0, nullptr);
        }

        // Sets 1..N: Downsampling
        for (uint32_t i = 1; i < frameRes.mipLevels; ++i) {
            VkDescriptorImageInfo inputInfo{};
            inputInfo.imageLayout = VK_IMAGE_LAYOUT_GENERAL;
            inputInfo.imageView = frameRes.mipViews[i - 1];
            inputInfo.sampler = m_HZBSampler;

            VkDescriptorImageInfo outputInfo{};
            outputInfo.imageLayout = VK_IMAGE_LAYOUT_GENERAL;
            outputInfo.imageView = frameRes.mipViews[i];

            VkWriteDescriptorSet writes[2]{};
            writes[0].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
            writes[0].dstSet = frameRes.downsampleDescriptorSets[i - 1];
            writes[0].dstBinding = 0;
            writes[0].descriptorCount = 1;
            writes[0].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
            writes[0].pImageInfo = &inputInfo;

            writes[1].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
            writes[1].dstSet = frameRes.downsampleDescriptorSets[i - 1];
            writes[1].dstBinding = 1;
            writes[1].descriptorCount = 1;
            writes[1].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
            writes[1].pImageInfo = &outputInfo;

            vkUpdateDescriptorSets(resources.device, 2, writes, 0, nullptr);
        }

        // 3. Transition GBuffer depth image to SHADER_READ_ONLY_OPTIMAL if not already
        // (Usually handled by the renderer, but let's make sure it is in correct layout)

        // Transition entire HZB Image layout to GENERAL
        {
            VkImageMemoryBarrier barrier{};
            barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
            barrier.oldLayout = VK_IMAGE_LAYOUT_UNDEFINED;
            barrier.newLayout = VK_IMAGE_LAYOUT_GENERAL;
            barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
            barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
            barrier.image = frameRes.hzbImage;
            barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
            barrier.subresourceRange.baseMipLevel = 0;
            barrier.subresourceRange.levelCount = frameRes.mipLevels;
            barrier.subresourceRange.baseArrayLayer = 0;
            barrier.subresourceRange.layerCount = 1;
            barrier.srcAccessMask = 0;
            barrier.dstAccessMask = VK_ACCESS_SHADER_WRITE_BIT | VK_ACCESS_SHADER_READ_BIT;

            vkCmdPipelineBarrier(cmd,
                VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,
                VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
                0, 0, nullptr, 0, nullptr, 1, &barrier);
        }

        // 4. Dispatch Copy Pass (Mip 0)
        {
            vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_COMPUTE, m_CopyPipeline);
            vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_COMPUTE, m_PipelineLayout, 0, 1, &frameRes.copyDescriptorSet, 0, nullptr);

            HZBPushConstants pc{};
            pc.size = glm::ivec2(width, height);
            vkCmdPushConstants(cmd, m_PipelineLayout, VK_SHADER_STAGE_COMPUTE_BIT, 0, sizeof(HZBPushConstants), &pc);

            uint32_t groupX = (width + 15) / 16;
            uint32_t groupY = (height + 15) / 16;
            vkCmdDispatch(cmd, groupX, groupY, 1);
        }

        // 5. Dispatch Downsampling Passes
        uint32_t currentWidth = width;
        uint32_t currentHeight = height;

        vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_COMPUTE, m_DownsamplePipeline);

        for (uint32_t i = 1; i < frameRes.mipLevels; ++i) {
            // Pipeline barrier: Wait for writes to mip i-1 to complete before sampling it
            VkImageMemoryBarrier barrier{};
            barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
            barrier.oldLayout = VK_IMAGE_LAYOUT_GENERAL;
            barrier.newLayout = VK_IMAGE_LAYOUT_GENERAL;
            barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
            barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
            barrier.image = frameRes.hzbImage;
            barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
            barrier.subresourceRange.baseMipLevel = i - 1;
            barrier.subresourceRange.levelCount = 1;
            barrier.subresourceRange.baseArrayLayer = 0;
            barrier.subresourceRange.layerCount = 1;
            barrier.srcAccessMask = VK_ACCESS_SHADER_WRITE_BIT;
            barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;

            vkCmdPipelineBarrier(cmd,
                VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
                VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
                0, 0, nullptr, 0, nullptr, 1, &barrier);

            currentWidth = std::max(1u, currentWidth / 2);
            currentHeight = std::max(1u, currentHeight / 2);

            vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_COMPUTE, m_PipelineLayout, 0, 1, &frameRes.downsampleDescriptorSets[i - 1], 0, nullptr);

            HZBPushConstants pc{};
            pc.size = glm::ivec2(currentWidth, currentHeight);
            vkCmdPushConstants(cmd, m_PipelineLayout, VK_SHADER_STAGE_COMPUTE_BIT, 0, sizeof(HZBPushConstants), &pc);

            uint32_t groupX = (currentWidth + 7) / 8;
            uint32_t groupY = (currentHeight + 7) / 8;
            vkCmdDispatch(cmd, groupX, groupY, 1);
        }

        // Final layout transition of the whole HZB texture to SHADER_READ_ONLY_OPTIMAL for debug/visualization or occlusion culling
        {
            VkImageMemoryBarrier barrier{};
            barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
            barrier.oldLayout = VK_IMAGE_LAYOUT_GENERAL;
            barrier.newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
            barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
            barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
            barrier.image = frameRes.hzbImage;
            barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
            barrier.subresourceRange.baseMipLevel = 0;
            barrier.subresourceRange.levelCount = frameRes.mipLevels;
            barrier.subresourceRange.baseArrayLayer = 0;
            barrier.subresourceRange.layerCount = 1;
            barrier.srcAccessMask = VK_ACCESS_SHADER_WRITE_BIT;
            barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;

            vkCmdPipelineBarrier(cmd,
                VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
                VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT | VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
                0, 0, nullptr, 0, nullptr, 1, &barrier);
        }
    }

    void HZBPass::RecreateResources(EngineResources& resources, uint32_t width, uint32_t height) {
        if (m_Frames.size() < resources.MAX_FRAMES_IN_FLIGHT) {
            m_Frames.resize(resources.MAX_FRAMES_IN_FLIGHT);
        }
        for (uint32_t i = 0; i < resources.MAX_FRAMES_IN_FLIGHT; ++i) {
            recreateFrameResources(resources, m_Frames[i], width, height);
        }
    }

} // namespace eng::renderer
