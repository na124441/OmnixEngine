#include "Core/pch.h"
#include "EngineResources.h"
#include "RenderingEngine/Renderer/LightingUBO.h"
#include <fstream>
#include "VmaHelpers.h"
#include "Core/Vulkan/VkUtils.h"
#include "Log.h"
#include "Renderer/scene/GlobalUBO.h"
#include <string>

namespace eng::renderer {

VkCommandBuffer EngineResources::beginSingleTimeCommands() const
{
    VkCommandBufferAllocateInfo allocInfo{};
    allocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
    allocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    allocInfo.commandPool = commandPools[0]; // Use first frame's pool
    allocInfo.commandBufferCount = 1;

    VkCommandBuffer commandBuffer;
    vkAllocateCommandBuffers(device, &allocInfo, &commandBuffer);

    VkCommandBufferBeginInfo beginInfo{};
    beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
    beginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;

    vkBeginCommandBuffer(commandBuffer, &beginInfo);
    return commandBuffer;
}

void EngineResources::endSingleTimeCommands(VkCommandBuffer cmd) const
{
    vkEndCommandBuffer(cmd);

    VkSubmitInfo submitInfo{};
    submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
    submitInfo.commandBufferCount = 1;
    submitInfo.pCommandBuffers = &cmd;

    vkQueueSubmit(graphicsQueue, 1, &submitInfo, VK_NULL_HANDLE);
    vkQueueWaitIdle(graphicsQueue);

    vkFreeCommandBuffers(device, commandPools[0], 1, &cmd);
}

void EngineResources::recreateSwapChain()
{
    LOG_INFO("EngineResources: Swapchain recreation triggered - updating per-frame UBOs");
    
    // Per Phase 6: Re-create per-frame UBOs because the projection matrix may have changed.
    destroyPerFrameResources();
    createPerFrameResources();
    
    LOG_INFO("Swapchain recreated - sync objects untouched.");
}

// ---------- Command pools ----------
void EngineResources::createCommandPools()
{
    commandPools.resize(MAX_FRAMES_IN_FLIGHT);
    VkCommandPoolCreateInfo poolInfo{};
    poolInfo.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
    poolInfo.queueFamilyIndex = graphicsQueueFamily;
    poolInfo.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;

    for (size_t i = 0; i < MAX_FRAMES_IN_FLIGHT; ++i) {
        VK_CHECK(vkCreateCommandPool(device, &poolInfo, nullptr,
                                      &commandPools[i]));
    }
    LOG_INFO("Created " + std::to_string(MAX_FRAMES_IN_FLIGHT) +
              " command pools (one per frame).");
}

// ---------- Per‑frame command buffers ----------
void EngineResources::createCommandBuffers()
{
    commandBuffers.resize(MAX_FRAMES_IN_FLIGHT);
    for (size_t frame = 0; frame < MAX_FRAMES_IN_FLIGHT; ++frame) {
        std::array<VkCommandBuffer, PASS_COUNT> perPass{};
        for (uint32_t p = 0; p < PASS_COUNT; ++p) {
            VkCommandBufferAllocateInfo allocInfo{};
            allocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
            allocInfo.commandPool = commandPools[frame];
            allocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
            allocInfo.commandBufferCount = 1;
            VK_CHECK(vkAllocateCommandBuffers(device, &allocInfo,
                                             &perPass[p]));
        }
        commandBuffers[frame] = perPass;
    }
    LOG_INFO("Allocated " + std::to_string(PASS_COUNT) + " command buffers per frame.");
}

void EngineResources::destroyCommandPools()
{
    for (auto pool : commandPools) {
        if (pool != VK_NULL_HANDLE) {
            vkDestroyCommandPool(device, pool, nullptr);
        }
    }
    commandPools.clear();
    commandBuffers.clear();
}



void EngineResources::createSyncObjects()
{
    imageAvailableSemaphores.resize(MAX_FRAMES_IN_FLIGHT);
    renderFinishedSemaphores.resize(MAX_FRAMES_IN_FLIGHT);
    inFlightFences.resize(MAX_FRAMES_IN_FLIGHT);

    VkSemaphoreCreateInfo semInfo{};
    semInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;

    VkFenceCreateInfo fenceInfo{};
    fenceInfo.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
    fenceInfo.flags = VK_FENCE_CREATE_SIGNALED_BIT;

    for (size_t i = 0; i < MAX_FRAMES_IN_FLIGHT; ++i) {
        VK_CHECK(vkCreateSemaphore(device, &semInfo, nullptr, &imageAvailableSemaphores[i]));
        VK_CHECK(vkCreateSemaphore(device, &semInfo, nullptr, &renderFinishedSemaphores[i]));
        VK_CHECK(vkCreateFence(device, &fenceInfo, nullptr, &inFlightFences[i]));
    }

    LOG_INFO("Sync objects created ("
              + std::to_string(imageAvailableSemaphores.size()) + " image-available, "
              + std::to_string(renderFinishedSemaphores.size()) + " render-finished, "
              + std::to_string(inFlightFences.size()) + " fences).");
}

void EngineResources::destroySyncObjects()
{
    for (auto s : imageAvailableSemaphores) if (s != VK_NULL_HANDLE) vkDestroySemaphore(device, s, nullptr);
    for (auto s : renderFinishedSemaphores) if (s != VK_NULL_HANDLE) vkDestroySemaphore(device, s, nullptr);
    for (auto f : inFlightFences) if (f != VK_NULL_HANDLE) vkDestroyFence(device, f, nullptr);
    
    imageAvailableSemaphores.clear();
    renderFinishedSemaphores.clear();
    inFlightFences.clear();
}

VkShaderModule EngineResources::loadShaderModule(const std::string& filename) const
{
    std::ifstream file(filename, std::ios::ate | std::ios::binary);

    if (!file.is_open()) {
        return VK_NULL_HANDLE;
    }

    size_t fileSize = (size_t)file.tellg();
    std::vector<char> buffer(fileSize);

    file.seekg(0);
    file.read(buffer.data(), fileSize);
    file.close();

    VkShaderModuleCreateInfo createInfo{};
    createInfo.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
    createInfo.codeSize = buffer.size();
    createInfo.pCode = reinterpret_cast<const uint32_t*>(buffer.data());

    VkShaderModule shaderModule;
    if (vkCreateShaderModule(device, &createInfo, nullptr, &shaderModule) != VK_SUCCESS) {
        return VK_NULL_HANDLE;
    }

    return shaderModule;
}

void EngineResources::ensureStagingBuffer(VkDeviceSize neededSize)
{
    if (transfer.stagingBuffer != VK_NULL_HANDLE && transfer.size >= neededSize) {
        // already large enough – nothing to do
        return;
    }

    // If a buffer exists but is too small, destroy it first
    if (transfer.stagingBuffer != VK_NULL_HANDLE) {
        destroyBufferVMA(allocator, transfer.stagingBuffer, transfer.stagingAlloc);
        transfer.stagingBuffer = VK_NULL_HANDLE;
        transfer.stagingAlloc  = VK_NULL_HANDLE;
        transfer.size = 0;
    }

    LOG_INFO("Creating staging buffer of size " + std::to_string(neededSize) + " bytes");

    VkBufferCreateInfo bufInfo{};
    bufInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
    bufInfo.size  = neededSize;
    bufInfo.usage = VK_BUFFER_USAGE_TRANSFER_SRC_BIT;
    bufInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

    VmaAllocationCreateInfo allocInfo{};
    allocInfo.usage = VMA_MEMORY_USAGE_CPU_ONLY;   // host visible, coherent

    VkResult res = createBufferVMA(allocator, &bufInfo, &allocInfo,
                                  &transfer.stagingBuffer,
                                  &transfer.stagingAlloc);
    VK_CHECK(res);

    transfer.size = neededSize;
}

void EngineResources::copyStagingToDevice(VkCommandBuffer cmd,
                                          VkBuffer dstBuffer,
                                          VkDeviceSize dstOffset,
                                          VkDeviceSize copySize)
{
    VkBufferCopy region{};
    region.srcOffset = 0;          // we always copy from offset 0 of the staging buffer
    region.dstOffset = dstOffset;
    region.size      = copySize;

    vkCmdCopyBuffer(cmd, transfer.stagingBuffer, dstBuffer, 1, &region);
}

uint32_t EngineResources::findMemoryType(uint32_t typeFilter, VkMemoryPropertyFlags properties) const
{
    VkPhysicalDeviceMemoryProperties memProperties;
    vkGetPhysicalDeviceMemoryProperties(physicalDevice, &memProperties);

    for (uint32_t i = 0; i < memProperties.memoryTypeCount; i++) {
        if ((typeFilter & (1 << i)) && (memProperties.memoryTypes[i].propertyFlags & properties) == properties) {
            return i;
        }
    }
    LOG_FATAL("Failed to find suitable memory type!");
    return 0;
}

void EngineResources::createBuffer(VkDeviceSize size, VkBufferUsageFlags usage, VkMemoryPropertyFlags properties, VkBuffer& buffer, VkDeviceMemory& bufferMemory) const
{
    VkBufferCreateInfo bufferInfo{};
    bufferInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
    bufferInfo.size = size;
    bufferInfo.usage = usage;
    bufferInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

    VK_CHECK(vkCreateBuffer(device, &bufferInfo, nullptr, &buffer));

    VkMemoryRequirements memRequirements;
    vkGetBufferMemoryRequirements(device, buffer, &memRequirements);

    VkMemoryAllocateInfo allocInfo{};
    allocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    allocInfo.allocationSize = memRequirements.size;
    allocInfo.memoryTypeIndex = findMemoryType(memRequirements.memoryTypeBits, properties);

    VK_CHECK(vkAllocateMemory(device, &allocInfo, nullptr, &bufferMemory));
    vkBindBufferMemory(device, buffer, bufferMemory, 0);
}



static VkDeviceSize alignUp(VkDeviceSize value, VkDeviceSize alignment)
{
    return (value + alignment - 1) & ~(alignment - 1);
}

void EngineResources::createPerFrameResources()
{
    // Global descriptor‑set layout (camera UBO) --------------------------------
    if (globalSetLayout == VK_NULL_HANDLE) {
        VkDescriptorSetLayoutBinding uboBinding{};
        uboBinding.binding            = 0;
        uboBinding.descriptorType     = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
        uboBinding.descriptorCount    = 1;
        uboBinding.stageFlags         = VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT;
        uboBinding.pImmutableSamplers = nullptr;

        VkDescriptorSetLayoutCreateInfo layoutInfo{};
        layoutInfo.sType        = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
        layoutInfo.bindingCount = 1;
        layoutInfo.pBindings    = &uboBinding;

        VK_CHECK(vkCreateDescriptorSetLayout(device, &layoutInfo,
                                               nullptr, &globalSetLayout));
        LOG_INFO("Global descriptor set layout (camera UBO) created.");
    }

    // Allocate per‑frame data ----------------------------------------------------
    perFrameData.resize(MAX_FRAMES_IN_FLIGHT);
    VkDeviceSize uboSize = alignUp(sizeof(GlobalUBO), static_cast<VkDeviceSize>(256));

    for (size_t i = 0; i < MAX_FRAMES_IN_FLIGHT; ++i) {
        FrameData& fd = perFrameData[i];

        // ---- Temporary descriptor pool (per‑frame) -----------------------------
        VkDescriptorPoolSize poolSize{};
        poolSize.type            = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
        poolSize.descriptorCount = 1; // one global UBO per frame

        VkDescriptorPoolCreateInfo poolInfo{};
        poolInfo.sType         = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
        poolInfo.poolSizeCount = 1;
        poolInfo.pPoolSizes    = &poolSize;
        poolInfo.maxSets       = 1;
        poolInfo.flags         = VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT;

        VK_CHECK(vkCreateDescriptorPool(device, &poolInfo, nullptr,
                                         &fd.descriptorPool));

        // ---- UBO buffer -------------------------------------------------------
        VkBufferCreateInfo bufInfo{};
        bufInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
        bufInfo.size  = uboSize;
        bufInfo.usage = VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT;
        bufInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

        VmaAllocationCreateInfo vmaAllocInfo{};
        vmaAllocInfo.usage = VMA_MEMORY_USAGE_CPU_TO_GPU;

        VK_CHECK(vmaCreateBuffer(allocator, &bufInfo, &vmaAllocInfo,
                                 &fd.uboBuffer, &fd.uboAlloc, nullptr));
        ::eng::ResourceTracker::incBuffer();

        // ---- Descriptor set (global UBO) ---------------------------------------
        VkDescriptorSetAllocateInfo setAlloc{};
        setAlloc.sType              = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
        setAlloc.descriptorPool     = fd.descriptorPool;
        setAlloc.descriptorSetCount = 1;
        setAlloc.pSetLayouts        = &globalSetLayout;

        VK_CHECK(vkAllocateDescriptorSets(device, &setAlloc, &fd.uboDescriptor));

        // Write descriptor
        VkDescriptorBufferInfo bufInfoDesc{};
        bufInfoDesc.buffer = fd.uboBuffer;
        bufInfoDesc.offset = 0;
        bufInfoDesc.range  = sizeof(GlobalUBO);

        VkWriteDescriptorSet write{};
        write.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        write.dstSet          = fd.uboDescriptor;
        write.dstBinding      = 0;
        write.dstArrayElement = 0;
        write.descriptorCount = 1;
        write.descriptorType  = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
        write.pBufferInfo     = &bufInfoDesc;

        vkUpdateDescriptorSets(device, 1, &write, 0, nullptr);
    }

    LOG_INFO("Created per‑frame UBOs and descriptor pools.");
}

void EngineResources::destroyPerFrameResources()
{
    for (FrameData& fd : perFrameData) {
        if (fd.uboBuffer != VK_NULL_HANDLE) {
            destroyBufferVMA(allocator, fd.uboBuffer, fd.uboAlloc);
            fd.uboBuffer = VK_NULL_HANDLE;
        }
        if (fd.descriptorPool != VK_NULL_HANDLE) {
            vkDestroyDescriptorPool(device, fd.descriptorPool, nullptr);
            fd.descriptorPool = VK_NULL_HANDLE;
        }
    }
    perFrameData.clear();

    // Layout is owned by the Renderer/Pipeline, so we don't destroy it here.
}

// ---------------------------------------------------------------------
void EngineResources::createMaterialDescriptorResources()
{
    // Bindings:
    // 0 – per‑material uniform buffer (roughness/metallic/etc.)
    // 1 – albedo texture (combined image sampler)
    // 2 – normal texture (combined image sampler)
    // 3 – metallic-roughness texture (combined image sampler)
    // 4 – AO texture (combined image sampler)
    // 5 – emissive texture (combined image sampler)
    VkDescriptorSetLayoutBinding bindings[6]{};

    // 0 – material uniform buffer
    bindings[0].binding            = 0;
    bindings[0].descriptorType     = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
    bindings[0].descriptorCount   = 1;
    bindings[0].stageFlags        = VK_SHADER_STAGE_FRAGMENT_BIT;
    bindings[0].pImmutableSamplers = nullptr;

    // 1 – albedo texture
    bindings[1].binding            = 1;
    bindings[1].descriptorType     = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    bindings[1].descriptorCount   = 1;
    bindings[1].stageFlags        = VK_SHADER_STAGE_FRAGMENT_BIT;
    bindings[1].pImmutableSamplers = nullptr;

    // 2 – normal texture (optional – still created)
    bindings[2].binding            = 2;
    bindings[2].descriptorType     = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    bindings[2].descriptorCount   = 1;
    bindings[2].stageFlags        = VK_SHADER_STAGE_FRAGMENT_BIT;
    bindings[2].pImmutableSamplers = nullptr;

    // 3 – metallic-roughness texture
    bindings[3].binding            = 3;
    bindings[3].descriptorType     = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    bindings[3].descriptorCount   = 1;
    bindings[3].stageFlags        = VK_SHADER_STAGE_FRAGMENT_BIT;
    bindings[3].pImmutableSamplers = nullptr;

    // 4 – AO texture
    bindings[4].binding            = 4;
    bindings[4].descriptorType     = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    bindings[4].descriptorCount   = 1;
    bindings[4].stageFlags        = VK_SHADER_STAGE_FRAGMENT_BIT;
    bindings[4].pImmutableSamplers = nullptr;

    // 5 – emissive texture
    bindings[5].binding            = 5;
    bindings[5].descriptorType     = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    bindings[5].descriptorCount   = 1;
    bindings[5].stageFlags        = VK_SHADER_STAGE_FRAGMENT_BIT;
    bindings[5].pImmutableSamplers = nullptr;

    VkDescriptorSetLayoutCreateInfo layoutInfo{};
    layoutInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
    layoutInfo.bindingCount = 6;
    layoutInfo.pBindings = bindings;

    VK_CHECK(vkCreateDescriptorSetLayout(device,
                                          &layoutInfo,
                                          nullptr,
                                          &materialSetLayout));
    LOG_INFO("Material descriptor set layout created.");

    // ---- Pool ---------------------------------------------------------
    const uint32_t maxMaterials = 256; // arbitrary upper bound for now

    VkDescriptorPoolSize poolSizes[2]{};
    poolSizes[0].type            = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
    poolSizes[0].descriptorCount = maxMaterials;          // one per material
    poolSizes[1].type            = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    poolSizes[1].descriptorCount = maxMaterials * 5;      // albedo + normal + metallic/roughness + AO + emissive

    VkDescriptorPoolCreateInfo poolInfo{};
    poolInfo.sType         = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
    poolInfo.maxSets       = maxMaterials;
    poolInfo.poolSizeCount = 2;
    poolInfo.pPoolSizes    = poolSizes;
    poolInfo.flags         = VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT;

    VK_CHECK(vkCreateDescriptorPool(device,
                                     &poolInfo,
                                     nullptr,
                                     &materialDescriptorPool));
    LOG_INFO("Material descriptor pool created (maxSets = " +
             std::to_string(maxMaterials) + ").");
}

void EngineResources::destroyMaterialDescriptorResources()
{
    if (materialDescriptorPool != VK_NULL_HANDLE) {
        vkDestroyDescriptorPool(device, materialDescriptorPool, nullptr);
        materialDescriptorPool = VK_NULL_HANDLE;
    }
    if (materialSetLayout != VK_NULL_HANDLE) {
        vkDestroyDescriptorSetLayout(device, materialSetLayout, nullptr);
        materialSetLayout = VK_NULL_HANDLE;
    }
}

void EngineResources::createLightingDescriptorResources()
{
    // 1. Layout
    VkDescriptorSetLayoutBinding binding{};
    binding.binding = 0;
    binding.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
    binding.descriptorCount = 1;
    binding.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;
    binding.pImmutableSamplers = nullptr;

    VkDescriptorSetLayoutCreateInfo layoutInfo{};
    layoutInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
    layoutInfo.bindingCount = 1;
    layoutInfo.pBindings = &binding;

    VK_CHECK(vkCreateDescriptorSetLayout(device, &layoutInfo, nullptr, &lightingSetLayout));
    LOG_INFO("Lighting descriptor set layout (set = 2) created.");

    // 2. Pool
    VkDescriptorPoolSize lightPoolSize{};
    lightPoolSize.type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
    lightPoolSize.descriptorCount = MAX_FRAMES_IN_FLIGHT;

    VkDescriptorPoolCreateInfo lightPoolInfo{};
    lightPoolInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
    lightPoolInfo.poolSizeCount = 1;
    lightPoolInfo.pPoolSizes = &lightPoolSize;
    lightPoolInfo.maxSets = MAX_FRAMES_IN_FLIGHT;

    VK_CHECK(vkCreateDescriptorPool(device, &lightPoolInfo, nullptr, &lightingDescriptorPool));
    LOG_INFO("Lighting descriptor pool created.");

    // 3. Per-frame buffers and descriptor sets
    perFrameLightingData.resize(MAX_FRAMES_IN_FLIGHT);
    for (size_t i = 0; i < MAX_FRAMES_IN_FLIGHT; ++i) {
        VkDeviceSize ubSize = sizeof(LightData);
        VkBufferCreateInfo bufInfo{};
        bufInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
        bufInfo.size  = ubSize;
        bufInfo.usage = VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT;
        bufInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

        VmaAllocationCreateInfo allocInfo{};
        allocInfo.usage = VMA_MEMORY_USAGE_CPU_TO_GPU;

        VK_CHECK(createBufferVMA(allocator, &bufInfo, &allocInfo, &perFrameLightingData[i].uboBuffer, &perFrameLightingData[i].uboAlloc));

        VkDescriptorSetAllocateInfo dsInfo{};
        dsInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
        dsInfo.descriptorPool = lightingDescriptorPool;
        dsInfo.descriptorSetCount = 1;
        dsInfo.pSetLayouts = &lightingSetLayout;

        VK_CHECK(vkAllocateDescriptorSets(device, &dsInfo, &perFrameLightingData[i].descriptor));

        VkDescriptorBufferInfo bufInfoDesc{};
        bufInfoDesc.buffer = perFrameLightingData[i].uboBuffer;
        bufInfoDesc.offset = 0;
        bufInfoDesc.range  = ubSize;

        VkWriteDescriptorSet write{};
        write.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        write.dstSet = perFrameLightingData[i].descriptor;
        write.dstBinding = 0;
        write.dstArrayElement = 0;
        write.descriptorCount = 1;
        write.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
        write.pBufferInfo = &bufInfoDesc;

        vkUpdateDescriptorSets(device, 1, &write, 0, nullptr);
    }
    LOG_INFO("Lighting UBOs and descriptor sets allocated.");
}

void EngineResources::destroyLightingDescriptorResources()
{
    if (lightingDescriptorPool != VK_NULL_HANDLE) {
        vkDestroyDescriptorPool(device, lightingDescriptorPool, nullptr);
        lightingDescriptorPool = VK_NULL_HANDLE;
    }
    if (lightingSetLayout != VK_NULL_HANDLE) {
        vkDestroyDescriptorSetLayout(device, lightingSetLayout, nullptr);
        lightingSetLayout = VK_NULL_HANDLE;
    }

    for (auto& lf : perFrameLightingData) {
        if (lf.uboBuffer != VK_NULL_HANDLE) {
            destroyBufferVMA(allocator, lf.uboBuffer, lf.uboAlloc);
            lf.uboBuffer = VK_NULL_HANDLE;
        }
    }
    perFrameLightingData.clear();
}

void EngineResources::createPipelineLayout()
{
    if (pipelineLayout != VK_NULL_HANDLE) {
        vkDestroyPipelineLayout(device, pipelineLayout, nullptr);
        pipelineLayout = VK_NULL_HANDLE;
    }

    std::vector<VkDescriptorSetLayout> setLayouts = {
        globalSetLayout,   // set 0 – GPUScene
        materialSetLayout  // set 1 – material textures
    };

    VkPushConstantRange pushRange{};
    pushRange.stageFlags = VK_SHADER_STAGE_VERTEX_BIT;
    pushRange.offset = 0;
    pushRange.size = sizeof(uint32_t); // instance index

    VkPipelineLayoutCreateInfo layoutInfo{};
    layoutInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
    layoutInfo.setLayoutCount = static_cast<uint32_t>(setLayouts.size());
    layoutInfo.pSetLayouts = setLayouts.data();
    layoutInfo.pushConstantRangeCount = 1;
    layoutInfo.pPushConstantRanges = &pushRange;

    VK_CHECK(vkCreatePipelineLayout(device, &layoutInfo, nullptr, &pipelineLayout));
    LOG_INFO("Pipeline layout created (2 descriptor sets + 4-byte push constant).");
}

} // namespace eng::renderer
