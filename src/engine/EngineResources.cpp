#include "EngineResources.h"
#include "VkResultCheck.h"
#include "ResourceTracker.h"

VkCommandBuffer EngineResources::beginSingleTimeCommands() const {
    VkCommandBufferAllocateInfo allocInfo{};
    allocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
    allocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    allocInfo.commandPool = commandPools[0]; // Use the first pool
    allocInfo.commandBufferCount = 1;

    VkCommandBuffer commandBuffer;
    VK_CHECK(vkAllocateCommandBuffers(device, &allocInfo, &commandBuffer));

    VkCommandBufferBeginInfo beginInfo{};
    beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
    beginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;

    VK_CHECK(vkBeginCommandBuffer(commandBuffer, &beginInfo));

    return commandBuffer;
}

void EngineResources::endSingleTimeCommands(VkCommandBuffer commandBuffer) const {
    VK_CHECK(vkEndCommandBuffer(commandBuffer));

    VkSubmitInfo submitInfo{};
    submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
    submitInfo.commandBufferCount = 1;
    submitInfo.pCommandBuffers = &commandBuffer;

    VK_CHECK(vkQueueSubmit(graphicsQueue, 1, &submitInfo, VK_NULL_HANDLE));
    VK_CHECK(vkQueueWaitIdle(graphicsQueue));

    vkFreeCommandBuffers(device, commandPools[0], 1, &commandBuffer);
}

void EngineResources::recreateSwapChain() const {
    // This is a placeholder as the actual recreation logic involves 
    // many dependencies from EngineLoop. In a real scenario, this would
    // call back into the main engine loop or swapchain manager.
}

uint32_t EngineResources::findMemoryType(uint32_t typeFilter, VkMemoryPropertyFlags properties) const {
    VkPhysicalDeviceMemoryProperties memProperties;
    vkGetPhysicalDeviceMemoryProperties(physicalDevice, &memProperties);

    for (uint32_t i = 0; i < memProperties.memoryTypeCount; i++) {
        if ((typeFilter & (1 << i)) && (memProperties.memoryTypes[i].propertyFlags & properties) == properties) {
            return i;
        }
    }
    return 0;
}

void EngineResources::createBuffer(VkDeviceSize size, VkBufferUsageFlags usage, VkMemoryPropertyFlags properties, VkBuffer& buffer, VkDeviceMemory& bufferMemory) const {
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

    VK_CHECK(vkBindBufferMemory(device, buffer, bufferMemory, 0));
    ResourceTracker::incBuffer();
}
