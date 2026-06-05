#include "Core/pch.h"
#include "stb/stb_image.h"
#include "Texture.h"
#include <cstring>
#include "Core/Engine/EngineResources.h"

namespace eng::renderer {

bool Texture::loadFromFile(const std::string& filename,
                           VkDevice dev,
                           VmaAllocator alloc,
                           VkCommandPool transferPool,
                           VkQueue graphicsQueue)
{
    device    = dev;
    allocator = alloc;

    // ---------------------------------------------------------------
    // Load image data (force 4 channels)
    int texWidth, texHeight, texChannels;
    stbi_uc* pixels = stbi_load(filename.c_str(),
                                 &texWidth, &texHeight,
                                 &texChannels, STBI_rgb_alpha);
    if (!pixels) {
        LOG_ERROR(("Failed to load texture image: " + filename).c_str());
        return false;
    }
    VkDeviceSize imageSize = static_cast<VkDeviceSize>(texWidth * texHeight * 4);
    LOG_INFO(("Loaded texture '" + filename + "' (" +
             std::to_string(texWidth) + "x" + std::to_string(texHeight) + ")").c_str());

    // ---------------------------------------------------------------
    // 1️⃣ Staging buffer (host visible)
    VkBuffer stagingBuffer;
    VmaAllocation stagingAlloc;
    VkBufferCreateInfo bufInfo{};
    bufInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
    bufInfo.size  = imageSize;
    bufInfo.usage = VK_BUFFER_USAGE_TRANSFER_SRC_BIT;
    bufInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

    VmaAllocationCreateInfo bufAllocInfo{};
    bufAllocInfo.usage = VMA_MEMORY_USAGE_CPU_ONLY;

    VK_CHECK(vmaCreateBuffer(allocator, &bufInfo, &bufAllocInfo,
                             &stagingBuffer, &stagingAlloc, nullptr));
    ::eng::ResourceTracker::incBuffer();

    // Copy pixel data into staging buffer
    void* data = nullptr;
    VK_CHECK(vmaMapMemory(allocator, stagingAlloc, &data));
    std::memcpy(data, pixels, static_cast<size_t>(imageSize));
    vmaUnmapMemory(allocator, stagingAlloc);
    stbi_image_free(pixels);

    // ---------------------------------------------------------------
    // 2️⃣ Create GPU‑only image
    VkImageCreateInfo imgInfo{};
    imgInfo.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
    imgInfo.imageType = VK_IMAGE_TYPE_2D;
    imgInfo.format = VK_FORMAT_R8G8B8A8_SRGB;
    imgInfo.extent.width  = static_cast<uint32_t>(texWidth);
    imgInfo.extent.height = static_cast<uint32_t>(texHeight);
    imgInfo.extent.depth  = 1;
    imgInfo.mipLevels = 1;
    imgInfo.arrayLayers = 1;
    imgInfo.samples = VK_SAMPLE_COUNT_1_BIT;
    imgInfo.tiling  = VK_IMAGE_TILING_OPTIMAL;
    imgInfo.usage   = VK_IMAGE_USAGE_TRANSFER_DST_BIT |
                      VK_IMAGE_USAGE_SAMPLED_BIT;
    imgInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
    imgInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;

    VmaAllocationCreateInfo imgAllocInfo{};
    imgAllocInfo.usage = VMA_MEMORY_USAGE_GPU_ONLY;

    VK_CHECK(vmaCreateImage(allocator, &imgInfo,
                            &imgAllocInfo, &image,
                            &allocation, nullptr));
    ::eng::ResourceTracker::incImage();

    // ---------------------------------------------------------------
    // 3️⃣ Transition image layout & copy from staging to image
    VkCommandBuffer cmd = EngineResources::get().beginSingleTimeCommands();

    // Transition to TRANSFER_DST_OPTIMAL
    VkImageMemoryBarrier barrier{};
    barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
    barrier.oldLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    barrier.newLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
    barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    barrier.image = image;
    barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    barrier.subresourceRange.baseMipLevel = 0;
    barrier.subresourceRange.levelCount   = 1;
    barrier.subresourceRange.baseArrayLayer = 0;
    barrier.subresourceRange.layerCount   = 1;
    barrier.srcAccessMask = 0;
    barrier.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;

    vkCmdPipelineBarrier(cmd,
                         VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,
                         VK_PIPELINE_STAGE_TRANSFER_BIT,
                         0,
                         0, nullptr,
                         0, nullptr,
                         1, &barrier);

    // Copy buffer → image
    VkBufferImageCopy region{};
    region.bufferOffset = 0;
    region.bufferRowLength = 0;
    region.bufferImageHeight = 0;
    region.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    region.imageSubresource.mipLevel = 0;
    region.imageSubresource.baseArrayLayer = 0;
    region.imageSubresource.layerCount = 1;
    region.imageOffset = {0,0,0};
    region.imageExtent = {
        static_cast<uint32_t>(texWidth),
        static_cast<uint32_t>(texHeight),
        1
    };
    vkCmdCopyBufferToImage(cmd,
                           stagingBuffer,
                           image,
                           VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                           1,
                           &region);

    // Transition to SHADER_READ_ONLY_OPTIMAL
    barrier.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
    barrier.newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
    barrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
    barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;

    vkCmdPipelineBarrier(cmd,
                         VK_PIPELINE_STAGE_TRANSFER_BIT,
                         VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
                         0,
                         0, nullptr,
                         0, nullptr,
                         1, &barrier);

    EngineResources::get().endSingleTimeCommands(cmd);

    // ---------------------------------------------------------------
    // 4️⃣ Destroy staging resources
    vmaDestroyBuffer(allocator, stagingBuffer, stagingAlloc);
    ::eng::ResourceTracker::decBuffer();

    // ---------------------------------------------------------------
    // 5️⃣ Create image view
    VkImageViewCreateInfo viewInfo{};
    viewInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
    viewInfo.image = image;
    viewInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
    viewInfo.format = VK_FORMAT_R8G8B8A8_SRGB;
    viewInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    viewInfo.subresourceRange.baseMipLevel = 0;
    viewInfo.subresourceRange.levelCount = 1;
    viewInfo.subresourceRange.baseArrayLayer = 0;
    viewInfo.subresourceRange.layerCount = 1;

    VK_CHECK(vkCreateImageView(device, &viewInfo, nullptr, &imageView));

    // ---------------------------------------------------------------
    // 6️⃣ Create sampler (simple clamp‑to‑edge, linear filtering)
    VkSamplerCreateInfo samplerInfo{};
    samplerInfo.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
    samplerInfo.magFilter = VK_FILTER_LINEAR;
    samplerInfo.minFilter = VK_FILTER_LINEAR;
    samplerInfo.addressModeU = VK_SAMPLER_ADDRESS_MODE_REPEAT;
    samplerInfo.addressModeV = VK_SAMPLER_ADDRESS_MODE_REPEAT;
    samplerInfo.addressModeW = VK_SAMPLER_ADDRESS_MODE_REPEAT;
    samplerInfo.anisotropyEnable = VK_TRUE;
    samplerInfo.maxAnisotropy = 16;
    samplerInfo.borderColor = VK_BORDER_COLOR_INT_OPAQUE_BLACK;
    samplerInfo.unnormalizedCoordinates = VK_FALSE;
    samplerInfo.compareEnable = VK_FALSE;
    samplerInfo.mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR;

    VK_CHECK(vkCreateSampler(device, &samplerInfo, nullptr, &samplerHandle));
    LOG_INFO(("Texture '" + filename + "' uploaded and ready.").c_str());

    return true;
}

// ---------------------------------------------------------------------
// ---------------------------------------------------------------------
void Texture::destroy()
{
    if (samplerHandle != VK_NULL_HANDLE) {
        vkDestroySampler(device, samplerHandle, nullptr);
        samplerHandle = VK_NULL_HANDLE;
    }
    if (imageView != VK_NULL_HANDLE) {
        vkDestroyImageView(device, imageView, nullptr);
        imageView = VK_NULL_HANDLE;
    }
    if (image != VK_NULL_HANDLE) {
        vmaDestroyImage(allocator, image, allocation);
        image = VK_NULL_HANDLE;
        allocation = VK_NULL_HANDLE;
        ::eng::ResourceTracker::decImage();
    }
}

// ---------------------------------------------------------------------
Texture* Texture::getWhiteTexture(const EngineResources& res)
{
    static std::unique_ptr<Texture> whiteTex = nullptr;
    if (whiteTex) return whiteTex.get();

    LOG_INFO("Creating fallback 1x1 white texture...");
    whiteTex = std::make_unique<Texture>();
    whiteTex->device = res.device;
    whiteTex->allocator = res.allocator;

    uint32_t whitePixel = 0xFFFFFFFF;
    VkDeviceSize imageSize = 4;

    // 1. Staging buffer
    VkBuffer stagingBuffer;
    VmaAllocation stagingAlloc;
    VkBufferCreateInfo bufInfo{};
    bufInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
    bufInfo.size  = imageSize;
    bufInfo.usage = VK_BUFFER_USAGE_TRANSFER_SRC_BIT;
    bufInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

    VmaAllocationCreateInfo bufAllocInfo{};
    bufAllocInfo.usage = VMA_MEMORY_USAGE_CPU_ONLY;

    VK_CHECK(vmaCreateBuffer(res.allocator, &bufInfo, &bufAllocInfo, &stagingBuffer, &stagingAlloc, nullptr));
    ::eng::ResourceTracker::incBuffer();

    void* data = nullptr;
    vmaMapMemory(res.allocator, stagingAlloc, &data);
    std::memcpy(data, &whitePixel, 4);
    vmaUnmapMemory(res.allocator, stagingAlloc);

    // 2. Image
    VkImageCreateInfo imgInfo{};
    imgInfo.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
    imgInfo.imageType = VK_IMAGE_TYPE_2D;
    imgInfo.format = VK_FORMAT_R8G8B8A8_UNORM; // Use UNORM for data
    imgInfo.extent = {1, 1, 1};
    imgInfo.mipLevels = 1;
    imgInfo.arrayLayers = 1;
    imgInfo.samples = VK_SAMPLE_COUNT_1_BIT;
    imgInfo.tiling  = VK_IMAGE_TILING_OPTIMAL;
    imgInfo.usage   = VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT;
    imgInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
    imgInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;

    VmaAllocationCreateInfo imgAllocInfo{};
    imgAllocInfo.usage = VMA_MEMORY_USAGE_GPU_ONLY;

    VK_CHECK(vmaCreateImage(res.allocator, &imgInfo, &imgAllocInfo, &whiteTex->image, &whiteTex->allocation, nullptr));
    ::eng::ResourceTracker::incImage();

    // 3. Upload
    VkCommandBuffer cmd = res.beginSingleTimeCommands();
    
    // Undefined -> Transfer Dst
    VkImageMemoryBarrier barrier{};
    barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
    barrier.oldLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    barrier.newLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
    barrier.image = whiteTex->image;
    barrier.subresourceRange = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1};
    barrier.srcAccessMask = 0;
    barrier.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;

    vkCmdPipelineBarrier(cmd, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT, 0, 0, nullptr, 0, nullptr, 1, &barrier);

    VkBufferImageCopy region{};
    region.imageSubresource = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 0, 1};
    region.imageExtent = {1, 1, 1};
    vkCmdCopyBufferToImage(cmd, stagingBuffer, whiteTex->image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &region);

    // Transfer Dst -> Shader Read
    barrier.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
    barrier.newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
    barrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
    barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;

    vkCmdPipelineBarrier(cmd, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, 0, 0, nullptr, 0, nullptr, 1, &barrier);

    res.endSingleTimeCommands(cmd);

    vmaDestroyBuffer(res.allocator, stagingBuffer, stagingAlloc);
    ::eng::ResourceTracker::decBuffer();

    // 4. View & Sampler
    VkImageViewCreateInfo viewInfo{};
    viewInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
    viewInfo.image = whiteTex->image;
    viewInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
    viewInfo.format = VK_FORMAT_R8G8B8A8_UNORM;
    viewInfo.subresourceRange = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1};
    VK_CHECK(vkCreateImageView(res.device, &viewInfo, nullptr, &whiteTex->imageView));

    VkSamplerCreateInfo samplerInfo{};
    samplerInfo.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
    samplerInfo.magFilter = VK_FILTER_NEAREST;
    samplerInfo.minFilter = VK_FILTER_NEAREST;
    samplerInfo.addressModeU = VK_SAMPLER_ADDRESS_MODE_REPEAT;
    samplerInfo.addressModeV = VK_SAMPLER_ADDRESS_MODE_REPEAT;
    samplerInfo.addressModeW = VK_SAMPLER_ADDRESS_MODE_REPEAT;
    VK_CHECK(vkCreateSampler(res.device, &samplerInfo, nullptr, &whiteTex->samplerHandle));

    LOG_INFO("Fallback 1x1 white texture ready.");
    return whiteTex.get();
}

Texture* Texture::getFlatNormalTexture(const EngineResources& res)
{
    static std::unique_ptr<Texture> normalTex = nullptr;
    if (normalTex) return normalTex.get();

    LOG_INFO("Creating fallback 1x1 flat normal texture...");
    normalTex = std::make_unique<Texture>();
    normalTex->device = res.device;
    normalTex->allocator = res.allocator;

    uint8_t normalPixel[4] = { 128, 128, 255, 255 }; // Flat normal vector: (0, 0, 1) mapped to 0-255 range
    VkDeviceSize imageSize = 4;

    // 1. Staging buffer
    VkBuffer stagingBuffer;
    VmaAllocation stagingAlloc;
    VkBufferCreateInfo bufInfo{};
    bufInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
    bufInfo.size  = imageSize;
    bufInfo.usage = VK_BUFFER_USAGE_TRANSFER_SRC_BIT;
    bufInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

    VmaAllocationCreateInfo bufAllocInfo{};
    bufAllocInfo.usage = VMA_MEMORY_USAGE_CPU_ONLY;

    VK_CHECK(vmaCreateBuffer(res.allocator, &bufInfo, &bufAllocInfo, &stagingBuffer, &stagingAlloc, nullptr));
    ::eng::ResourceTracker::incBuffer();

    void* data = nullptr;
    vmaMapMemory(res.allocator, stagingAlloc, &data);
    std::memcpy(data, normalPixel, 4);
    vmaUnmapMemory(res.allocator, stagingAlloc);

    // 2. Image
    VkImageCreateInfo imgInfo{};
    imgInfo.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
    imgInfo.imageType = VK_IMAGE_TYPE_2D;
    imgInfo.format = VK_FORMAT_R8G8B8A8_UNORM;
    imgInfo.extent = {1, 1, 1};
    imgInfo.mipLevels = 1;
    imgInfo.arrayLayers = 1;
    imgInfo.samples = VK_SAMPLE_COUNT_1_BIT;
    imgInfo.tiling  = VK_IMAGE_TILING_OPTIMAL;
    imgInfo.usage   = VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT;
    imgInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
    imgInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;

    VmaAllocationCreateInfo imgAllocInfo{};
    imgAllocInfo.usage = VMA_MEMORY_USAGE_GPU_ONLY;

    VK_CHECK(vmaCreateImage(res.allocator, &imgInfo, &imgAllocInfo, &normalTex->image, &normalTex->allocation, nullptr));
    ::eng::ResourceTracker::incImage();

    // 3. Upload
    VkCommandBuffer cmd = res.beginSingleTimeCommands();
    
    VkImageMemoryBarrier barrier{};
    barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
    barrier.oldLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    barrier.newLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
    barrier.image = normalTex->image;
    barrier.subresourceRange = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1};
    barrier.srcAccessMask = 0;
    barrier.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;

    vkCmdPipelineBarrier(cmd, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT, 0, 0, nullptr, 0, nullptr, 1, &barrier);

    VkBufferImageCopy region{};
    region.imageSubresource = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 0, 1};
    region.imageExtent = {1, 1, 1};
    vkCmdCopyBufferToImage(cmd, stagingBuffer, normalTex->image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &region);

    barrier.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
    barrier.newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
    barrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
    barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;

    vkCmdPipelineBarrier(cmd, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, 0, 0, nullptr, 0, nullptr, 1, &barrier);

    res.endSingleTimeCommands(cmd);

    vmaDestroyBuffer(res.allocator, stagingBuffer, stagingAlloc);
    ::eng::ResourceTracker::decBuffer();

    // 4. View & Sampler
    VkImageViewCreateInfo viewInfo{};
    viewInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
    viewInfo.image = normalTex->image;
    viewInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
    viewInfo.format = VK_FORMAT_R8G8B8A8_UNORM;
    viewInfo.subresourceRange = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1};
    VK_CHECK(vkCreateImageView(res.device, &viewInfo, nullptr, &normalTex->imageView));

    VkSamplerCreateInfo samplerInfo{};
    samplerInfo.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
    samplerInfo.magFilter = VK_FILTER_NEAREST;
    samplerInfo.minFilter = VK_FILTER_NEAREST;
    samplerInfo.addressModeU = VK_SAMPLER_ADDRESS_MODE_REPEAT;
    samplerInfo.addressModeV = VK_SAMPLER_ADDRESS_MODE_REPEAT;
    samplerInfo.addressModeW = VK_SAMPLER_ADDRESS_MODE_REPEAT;
    VK_CHECK(vkCreateSampler(res.device, &samplerInfo, nullptr, &normalTex->samplerHandle));

    LOG_INFO("Fallback 1x1 flat normal texture ready.");
    return normalTex.get();
}

} // namespace eng::renderer
