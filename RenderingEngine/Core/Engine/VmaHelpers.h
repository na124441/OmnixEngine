#pragma once
#include <vulkan/vulkan.h>
#include "VmaUsage.h"
#include "Log.h"
#include "ResourceTracker.h"

namespace eng {

/* Simple wrapper that creates a buffer + allocation.
 * Returns VkResult to let the caller decide whether to abort or recover. */
inline VkResult createBufferVMA(
    VmaAllocator      allocator,
    const VkBufferCreateInfo* pCreateInfo,
    const VmaAllocationCreateInfo* pAllocInfo,
    VkBuffer*          pBuffer,
    VmaAllocation*     pAllocation)
{
    VkResult res = vmaCreateBuffer(allocator, pCreateInfo, pAllocInfo,
                                   pBuffer, pAllocation, nullptr);
    if (res != VK_SUCCESS) {
        LOG_ERROR(("VMA: Failed to create buffer (VkResult = " + std::to_string(res) + ")").c_str());
        return res;
    }

    // Track the buffer for manual lifetime checks (ResourceTracker)
    ResourceTracker::incBuffer();
    return VK_SUCCESS;
}

/* Destroy helper – mirrors the above */
inline void destroyBufferVMA(VmaAllocator allocator,
                             VkBuffer buffer,
                             VmaAllocation allocation)
{
    vmaDestroyBuffer(allocator, buffer, allocation);
    ResourceTracker::decBuffer();
}

} // namespace eng
