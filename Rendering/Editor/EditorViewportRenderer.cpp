#include "Core/pch.h"
#include "EditorViewportRenderer.h"
#include "ThirdParty/imgui/imgui.h"
#include "ThirdParty/imgui/backends/imgui_impl_vulkan.h"
#include "Core/Engine/ResourceTracker.h"
#include "Core/Engine/VmaHelpers.h"
#include "Core/Vulkan/VkUtils.h"
#include <algorithm>
#include <array>

namespace eng::renderer {

void EditorViewportRenderer::setOffscreenRenderingEnabled(bool enabled) {
    if (m_OffscreenRenderingEnabled != enabled) {
        m_OffscreenRenderingEnabled = enabled;
        if (m_OffscreenRenderingEnabled) {
            createOffscreenResources(m_OffscreenWidth, m_OffscreenHeight);
        } else {
            destroyOffscreenResources();
        }
    }
}

void EditorViewportRenderer::createOffscreenResources(uint32_t width, uint32_t height) {
    if (!resources) return;

    width = std::max(1u, width);
    height = std::max(1u, height);

    if (m_OffscreenWidth == width && m_OffscreenHeight == height && m_OffscreenRenderPass != VK_NULL_HANDLE) {
        return;
    }

    destroyOffscreenResources();

    m_OffscreenWidth = width;
    m_OffscreenHeight = height;

    uint32_t maxFrames = resources->MAX_FRAMES_IN_FLIGHT;
    m_OffscreenImages.resize(maxFrames, VK_NULL_HANDLE);
    m_OffscreenAllocations.resize(maxFrames, VK_NULL_HANDLE);
    m_OffscreenImageViews.resize(maxFrames, VK_NULL_HANDLE);
    m_OffscreenDepthImages.resize(maxFrames, VK_NULL_HANDLE);
    m_OffscreenDepthAllocations.resize(maxFrames, VK_NULL_HANDLE);
    m_OffscreenDepthImageViews.resize(maxFrames, VK_NULL_HANDLE);
    m_OffscreenFramebuffers.resize(maxFrames, VK_NULL_HANDLE);
    m_OffscreenImGuiTextures.resize(maxFrames, VK_NULL_HANDLE);

    // 1. Create Offscreen Render Pass
    std::array<VkAttachmentDescription, 2> attachments{};
    
    // Color
    attachments[0].format = resources->swapChainImageFormat;
    attachments[0].samples = VK_SAMPLE_COUNT_1_BIT;
    attachments[0].loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
    attachments[0].storeOp = VK_ATTACHMENT_STORE_OP_STORE;
    attachments[0].stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
    attachments[0].stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    attachments[0].initialLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
    attachments[0].finalLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

    // Depth
    attachments[1].format = VK_FORMAT_D32_SFLOAT;
    attachments[1].samples = VK_SAMPLE_COUNT_1_BIT;
    attachments[1].loadOp = VK_ATTACHMENT_LOAD_OP_LOAD; // Load the prepass depth
    attachments[1].storeOp = VK_ATTACHMENT_STORE_OP_STORE;
    attachments[1].stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
    attachments[1].stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    attachments[1].initialLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
    attachments[1].finalLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

    VkAttachmentReference colorAttachmentRef{};
    colorAttachmentRef.attachment = 0;
    colorAttachmentRef.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

    VkAttachmentReference depthAttachmentRef{};
    depthAttachmentRef.attachment = 1;
    depthAttachmentRef.layout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

    VkSubpassDescription subpass{};
    subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
    subpass.colorAttachmentCount = 1;
    subpass.pColorAttachments = &colorAttachmentRef;
    subpass.pDepthStencilAttachment = &depthAttachmentRef;

    VkSubpassDependency dependency{};
    dependency.srcSubpass = VK_SUBPASS_EXTERNAL;
    dependency.dstSubpass = 0;
    dependency.srcStageMask = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
    dependency.dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
    dependency.srcAccessMask = VK_ACCESS_SHADER_READ_BIT;
    dependency.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;

    VkSubpassDependency dependency2{};
    dependency2.srcSubpass = 0;
    dependency2.dstSubpass = VK_SUBPASS_EXTERNAL;
    dependency2.srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
    dependency2.dstStageMask = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
    dependency2.srcAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
    dependency2.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;

    std::array<VkSubpassDependency, 2> dependencies = { dependency, dependency2 };

    VkRenderPassCreateInfo renderPassInfo{};
    renderPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
    renderPassInfo.attachmentCount = static_cast<uint32_t>(attachments.size());
    renderPassInfo.pAttachments = attachments.data();
    renderPassInfo.subpassCount = 1;
    renderPassInfo.pSubpasses = &subpass;
    renderPassInfo.dependencyCount = static_cast<uint32_t>(dependencies.size());
    renderPassInfo.pDependencies = dependencies.data();

    VK_CHECK(vkCreateRenderPass(resources->device, &renderPassInfo, nullptr, &m_OffscreenRenderPass));

    // 2. Create Sampler
    VkSamplerCreateInfo samplerInfo{};
    samplerInfo.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
    samplerInfo.magFilter = VK_FILTER_LINEAR;
    samplerInfo.minFilter = VK_FILTER_LINEAR;
    samplerInfo.addressModeU = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
    samplerInfo.addressModeV = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
    samplerInfo.addressModeW = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
    samplerInfo.anisotropyEnable = VK_FALSE;
    samplerInfo.maxAnisotropy = 1.0f;
    samplerInfo.borderColor = VK_BORDER_COLOR_INT_OPAQUE_BLACK;
    samplerInfo.unnormalizedCoordinates = VK_FALSE;
    samplerInfo.compareEnable = VK_FALSE;
    samplerInfo.mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR;

    VK_CHECK(vkCreateSampler(resources->device, &samplerInfo, nullptr, &m_OffscreenSampler));

    // 3. Create color and depth images, views, framebuffers, and textures
    for (uint32_t i = 0; i < maxFrames; ++i) {
        VkImageCreateInfo imgInfo{};
        imgInfo.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
        imgInfo.imageType = VK_IMAGE_TYPE_2D;
        imgInfo.format = resources->swapChainImageFormat;
        imgInfo.extent.width = width;
        imgInfo.extent.height = height;
        imgInfo.extent.depth = 1;
        imgInfo.mipLevels = 1;
        imgInfo.arrayLayers = 1;
        imgInfo.samples = VK_SAMPLE_COUNT_1_BIT;
        imgInfo.tiling = VK_IMAGE_TILING_OPTIMAL;
        imgInfo.usage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT;
        imgInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
        imgInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;

        VmaAllocationCreateInfo imgAllocInfo{};
        imgAllocInfo.usage = VMA_MEMORY_USAGE_GPU_ONLY;

        VK_CHECK(vmaCreateImage(resources->allocator, &imgInfo, &imgAllocInfo,
                                 &m_OffscreenImages[i], &m_OffscreenAllocations[i], nullptr));
        ::eng::ResourceTracker::incImage();

        // Transition layout UNDEFINED -> SHADER_READ_ONLY_OPTIMAL for color
        VkCommandBuffer cmd = resources->beginSingleTimeCommands();
        VkImageMemoryBarrier barrier{};
        barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
        barrier.oldLayout = VK_IMAGE_LAYOUT_UNDEFINED;
        barrier.newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
        barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        barrier.image = m_OffscreenImages[i];
        barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        barrier.subresourceRange.baseMipLevel = 0;
        barrier.subresourceRange.levelCount = 1;
        barrier.subresourceRange.baseArrayLayer = 0;
        barrier.subresourceRange.layerCount = 1;
        barrier.srcAccessMask = 0;
        barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;

        vkCmdPipelineBarrier(cmd,
                             VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,
                             VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
                             0,
                             0, nullptr,
                             0, nullptr,
                             1, &barrier);
        resources->endSingleTimeCommands(cmd);

        // Create ImageView
        VkImageViewCreateInfo viewInfo{};
        viewInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
        viewInfo.image = m_OffscreenImages[i];
        viewInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
        viewInfo.format = resources->swapChainImageFormat;
        viewInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        viewInfo.subresourceRange.baseMipLevel = 0;
        viewInfo.subresourceRange.levelCount = 1;
        viewInfo.subresourceRange.baseArrayLayer = 0;
        viewInfo.subresourceRange.layerCount = 1;

        VK_CHECK(vkCreateImageView(resources->device, &viewInfo, nullptr, &m_OffscreenImageViews[i]));

        // --- Depth Image ---
        VkImageCreateInfo depthImgInfo{};
        depthImgInfo.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
        depthImgInfo.imageType = VK_IMAGE_TYPE_2D;
        depthImgInfo.format = VK_FORMAT_D32_SFLOAT;
        depthImgInfo.extent.width = width;
        depthImgInfo.extent.height = height;
        depthImgInfo.extent.depth = 1;
        depthImgInfo.mipLevels = 1;
        depthImgInfo.arrayLayers = 1;
        depthImgInfo.samples = VK_SAMPLE_COUNT_1_BIT;
        depthImgInfo.tiling = VK_IMAGE_TILING_OPTIMAL;
        depthImgInfo.usage = VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT;
        depthImgInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
        depthImgInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;

        VK_CHECK(vmaCreateImage(resources->allocator, &depthImgInfo, &imgAllocInfo,
                                 &m_OffscreenDepthImages[i], &m_OffscreenDepthAllocations[i], nullptr));
        ::eng::ResourceTracker::incImage();

        // Transition layout UNDEFINED -> DEPTH_STENCIL_ATTACHMENT_OPTIMAL for depth
        cmd = resources->beginSingleTimeCommands();
        barrier.oldLayout = VK_IMAGE_LAYOUT_UNDEFINED;
        barrier.newLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
        barrier.image = m_OffscreenDepthImages[i];
        barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT;
        barrier.srcAccessMask = 0;
        barrier.dstAccessMask = VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_READ_BIT | VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;

        vkCmdPipelineBarrier(cmd,
                             VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,
                             VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT | VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT,
                             0,
                             0, nullptr,
                             0, nullptr,
                             1, &barrier);
        resources->endSingleTimeCommands(cmd);

        // Create Depth ImageView
        VkImageViewCreateInfo depthViewInfo{};
        depthViewInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
        depthViewInfo.image = m_OffscreenDepthImages[i];
        depthViewInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
        depthViewInfo.format = VK_FORMAT_D32_SFLOAT;
        depthViewInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT;
        depthViewInfo.subresourceRange.baseMipLevel = 0;
        depthViewInfo.subresourceRange.levelCount = 1;
        depthViewInfo.subresourceRange.baseArrayLayer = 0;
        depthViewInfo.subresourceRange.layerCount = 1;

        VK_CHECK(vkCreateImageView(resources->device, &depthViewInfo, nullptr, &m_OffscreenDepthImageViews[i]));

        // Create Framebuffer
        VkImageView attachmentsList[] = { m_OffscreenImageViews[i], m_OffscreenDepthImageViews[i] };
        VkFramebufferCreateInfo framebufferInfo{};
        framebufferInfo.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
        framebufferInfo.renderPass = m_OffscreenRenderPass;
        framebufferInfo.attachmentCount = 2;
        framebufferInfo.pAttachments = attachmentsList;
        framebufferInfo.width = width;
        framebufferInfo.height = height;
        framebufferInfo.layers = 1;

        VK_CHECK(vkCreateFramebuffer(resources->device, &framebufferInfo, nullptr, &m_OffscreenFramebuffers[i]));

        // Register texture with ImGui
        m_OffscreenImGuiTextures[i] = ImGui_ImplVulkan_AddTexture(m_OffscreenSampler, m_OffscreenImageViews[i], VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
    }
}

void EditorViewportRenderer::destroyOffscreenResources() {
    if (!resources) return;

    for (size_t i = 0; i < m_OffscreenImGuiTextures.size(); ++i) {
        if (m_OffscreenImGuiTextures[i] != VK_NULL_HANDLE) {
            ImGui_ImplVulkan_RemoveTexture(m_OffscreenImGuiTextures[i]);
            m_OffscreenImGuiTextures[i] = VK_NULL_HANDLE;
        }
        if (m_OffscreenFramebuffers[i] != VK_NULL_HANDLE) {
            vkDestroyFramebuffer(resources->device, m_OffscreenFramebuffers[i], nullptr);
            m_OffscreenFramebuffers[i] = VK_NULL_HANDLE;
        }
        if (m_OffscreenImageViews[i] != VK_NULL_HANDLE) {
            vkDestroyImageView(resources->device, m_OffscreenImageViews[i], nullptr);
            m_OffscreenImageViews[i] = VK_NULL_HANDLE;
        }
        if (i < m_OffscreenDepthImageViews.size() && m_OffscreenDepthImageViews[i] != VK_NULL_HANDLE) {
            vkDestroyImageView(resources->device, m_OffscreenDepthImageViews[i], nullptr);
            m_OffscreenDepthImageViews[i] = VK_NULL_HANDLE;
        }
        if (m_OffscreenImages[i] != VK_NULL_HANDLE) {
            vmaDestroyImage(resources->allocator, m_OffscreenImages[i], m_OffscreenAllocations[i]);
            m_OffscreenImages[i] = VK_NULL_HANDLE;
            m_OffscreenAllocations[i] = VK_NULL_HANDLE;
            ::eng::ResourceTracker::decImage();
        }
        if (i < m_OffscreenDepthImages.size() && m_OffscreenDepthImages[i] != VK_NULL_HANDLE) {
            vmaDestroyImage(resources->allocator, m_OffscreenDepthImages[i], m_OffscreenDepthAllocations[i]);
            m_OffscreenDepthImages[i] = VK_NULL_HANDLE;
            m_OffscreenDepthAllocations[i] = VK_NULL_HANDLE;
            ::eng::ResourceTracker::decImage();
        }
    }

    if (m_OffscreenSampler != VK_NULL_HANDLE) {
        vkDestroySampler(resources->device, m_OffscreenSampler, nullptr);
        m_OffscreenSampler = VK_NULL_HANDLE;
    }

    if (m_OffscreenRenderPass != VK_NULL_HANDLE) {
        vkDestroyRenderPass(resources->device, m_OffscreenRenderPass, nullptr);
        m_OffscreenRenderPass = VK_NULL_HANDLE;
    }

    m_OffscreenImages.clear();
    m_OffscreenAllocations.clear();
    m_OffscreenImageViews.clear();
    m_OffscreenDepthImages.clear();
    m_OffscreenDepthAllocations.clear();
    m_OffscreenDepthImageViews.clear();
    m_OffscreenFramebuffers.clear();
    m_OffscreenImGuiTextures.clear();
}

VkDescriptorSet EditorViewportRenderer::getOffscreenTexture(uint32_t frameIndex) const {
    if (!m_OffscreenRenderingEnabled || frameIndex >= m_OffscreenImGuiTextures.size()) {
        return VK_NULL_HANDLE;
    }
    return m_OffscreenImGuiTextures[frameIndex];
}

} // namespace eng::renderer
