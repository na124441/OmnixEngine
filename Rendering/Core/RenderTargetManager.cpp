#include "Core/pch.h"
#include "RenderTargetManager.h"
#include "Core/Vulkan/VkUtils.h"
#include <algorithm>

namespace eng::renderer {

    static void SetVkDebugName(VkDevice device, VkObjectType objectType, uint64_t objectHandle, const char* name) {
        if (!device || !objectHandle || !name || !*name) return;
        PFN_vkSetDebugUtilsObjectNameEXT pfnSetObjectName = 
            (PFN_vkSetDebugUtilsObjectNameEXT)vkGetDeviceProcAddr(device, "vkSetDebugUtilsObjectNameEXT");
        if (pfnSetObjectName) {
            VkDebugUtilsObjectNameInfoEXT nameInfo{};
            nameInfo.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_OBJECT_NAME_INFO_EXT;
            nameInfo.objectType = objectType;
            nameInfo.objectHandle = objectHandle;
            nameInfo.pObjectName = name;
            pfnSetObjectName(device, &nameInfo);
        }
    }

    RenderTargetManager::~RenderTargetManager()
    {
        Shutdown();
    }

    void RenderTargetManager::Initialize(VkDevice device, VmaAllocator allocator)
    {
        m_Device = device;
        m_Allocator = allocator;
    }

    void RenderTargetManager::Shutdown()
    {
        if (m_Device == VK_NULL_HANDLE) return;

        for (auto& entry : m_Targets) {
            if (entry.active) {
                if (entry.target.view != VK_NULL_HANDLE) {
                    vkDestroyImageView(m_Device, entry.target.view, nullptr);
                }
                if (entry.target.image != VK_NULL_HANDLE && entry.target.allocation != nullptr) {
                    vmaDestroyImage(m_Allocator, entry.target.image, entry.target.allocation);
                }
            }
        }
        m_Targets.clear();
        m_FreeIndices.clear();
        m_Device = VK_NULL_HANDLE;
        m_Allocator = VK_NULL_HANDLE;
    }

    RenderTargetHandle RenderTargetManager::Create(const RenderTargetDesc& desc)
    {
        if (m_Device == VK_NULL_HANDLE || m_Allocator == VK_NULL_HANDLE) {
            throw std::runtime_error("RenderTargetManager: Not initialized.");
        }

        uint32_t index = 0;
        if (!m_FreeIndices.empty()) {
            index = m_FreeIndices.back();
            m_FreeIndices.pop_back();
        } else {
            index = static_cast<uint32_t>(m_Targets.size());
            m_Targets.emplace_back();
        }

        auto& entry = m_Targets[index];
        entry.active = true;
        entry.target.extent = { desc.width, desc.height };
        entry.target.format = desc.format;
        entry.target.usage = desc.usage;
        entry.target.aspect = desc.aspect;
        entry.target.currentLayout = VK_IMAGE_LAYOUT_UNDEFINED;
        entry.target.debugName = desc.debugName;
        entry.target.version = 1;

        // Create image
        VkImageCreateInfo imgInfo{};
        imgInfo.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
        imgInfo.imageType = VK_IMAGE_TYPE_2D;
        imgInfo.format = desc.format;
        imgInfo.extent.width = desc.width;
        imgInfo.extent.height = desc.height;
        imgInfo.extent.depth = 1;
        imgInfo.mipLevels = 1;
        imgInfo.arrayLayers = 1;
        imgInfo.samples = desc.samples;
        imgInfo.tiling = VK_IMAGE_TILING_OPTIMAL;
        imgInfo.usage = desc.usage;
        imgInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
        imgInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;

        VmaAllocationCreateInfo allocInfo{};
        allocInfo.usage = VMA_MEMORY_USAGE_GPU_ONLY;

        VK_CHECK(vmaCreateImage(m_Allocator, &imgInfo, &allocInfo, &entry.target.image, &entry.target.allocation, nullptr));

        // Create image view
        VkImageViewCreateInfo viewInfo{};
        viewInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
        viewInfo.image = entry.target.image;
        viewInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
        viewInfo.format = desc.format;
        viewInfo.subresourceRange.aspectMask = desc.aspect;
        viewInfo.subresourceRange.baseMipLevel = 0;
        viewInfo.subresourceRange.levelCount = 1;
        viewInfo.subresourceRange.baseArrayLayer = 0;
        viewInfo.subresourceRange.layerCount = 1;

        VK_CHECK(vkCreateImageView(m_Device, &viewInfo, nullptr, &entry.target.view));

        // Setup debug names if supported
        if (!desc.debugName.empty()) {
            SetVkDebugName(m_Device, VK_OBJECT_TYPE_IMAGE, reinterpret_cast<uint64_t>(entry.target.image), desc.debugName.c_str());
            SetVkDebugName(m_Device, VK_OBJECT_TYPE_IMAGE_VIEW, reinterpret_cast<uint64_t>(entry.target.view), (desc.debugName + "_view").c_str());
        }

        RenderTargetHandle handle{};
        handle.index = index;
        handle.generation = entry.generation;
        return handle;
    }

    void RenderTargetManager::Destroy(RenderTargetHandle handle)
    {
        if (!IsValid(handle)) return;

        auto& entry = m_Targets[handle.index];
        if (entry.target.view != VK_NULL_HANDLE) {
            vkDestroyImageView(m_Device, entry.target.view, nullptr);
            entry.target.view = VK_NULL_HANDLE;
        }
        if (entry.target.image != VK_NULL_HANDLE && entry.target.allocation != nullptr) {
            vmaDestroyImage(m_Allocator, entry.target.image, entry.target.allocation);
            entry.target.image = VK_NULL_HANDLE;
            entry.target.allocation = nullptr;
        }

        entry.active = false;
        entry.generation++; // Invalidate existing handles
        entry.target.extent = { 0, 0 };
        entry.target.format = VK_FORMAT_UNDEFINED;
        entry.target.currentLayout = VK_IMAGE_LAYOUT_UNDEFINED;
        
        m_FreeIndices.push_back(handle.index);
    }

    RenderTargetHandle RenderTargetManager::Recreate(RenderTargetHandle oldHandle, const RenderTargetDesc& newDesc)
    {
        uint64_t oldVersion = 0;
        if (IsValid(oldHandle)) {
            oldVersion = m_Targets[oldHandle.index].target.version;
            Destroy(oldHandle);
        }

        RenderTargetHandle newHandle = Create(newDesc);
        m_Targets[newHandle.index].target.version = oldVersion + 1;
        return newHandle;
    }

    RenderTarget* RenderTargetManager::Get(RenderTargetHandle handle)
    {
        if (!IsValid(handle)) return nullptr;
        return &m_Targets[handle.index].target;
    }

    const RenderTarget* RenderTargetManager::Get(RenderTargetHandle handle) const
    {
        if (!IsValid(handle)) return nullptr;
        return &m_Targets[handle.index].target;
    }

    bool RenderTargetManager::IsValid(RenderTargetHandle handle) const
    {
        if (handle.index >= m_Targets.size()) return false;
        const auto& entry = m_Targets[handle.index];
        return entry.active && entry.generation == handle.generation;
    }

    void RenderTargetManager::Transition(
        VkCommandBuffer cmd,
        RenderTargetHandle handle,
        VkImageLayout newLayout,
        VkPipelineStageFlags srcStage,
        VkPipelineStageFlags dstStage,
        VkAccessFlags srcAccess,
        VkAccessFlags dstAccess
    )
    {
        RenderTarget* target = Get(handle);
        if (!target) return;

        VkImageMemoryBarrier barrier{};
        barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
        barrier.oldLayout = target->currentLayout;
        barrier.newLayout = newLayout;
        barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        barrier.image = target->image;
        barrier.subresourceRange.aspectMask = target->aspect;
        barrier.subresourceRange.baseMipLevel = 0;
        barrier.subresourceRange.levelCount = 1;
        barrier.subresourceRange.baseArrayLayer = 0;
        barrier.subresourceRange.layerCount = 1;
        barrier.srcAccessMask = srcAccess;
        barrier.dstAccessMask = dstAccess;

        vkCmdPipelineBarrier(cmd, srcStage, dstStage, 0, 0, nullptr, 0, nullptr, 1, &barrier);
        target->currentLayout = newLayout;
    }

    void RenderTargetManager::AssertLayout(RenderTargetHandle handle, VkImageLayout expected)
    {
        const RenderTarget* target = Get(handle);
        if (!target || target->currentLayout != expected) {
            LOG_WARN("RenderTarget layout mismatch: expected layout not met for '" + 
                     (target ? target->debugName : "Unknown") + "'");
        }
    }

    void RenderTargetManager::DumpAllTargets() const
    {
        LOG_INFO("================= Render Target Manager Dump =================");
        for (size_t i = 0; i < m_Targets.size(); ++i) {
            const auto& entry = m_Targets[i];
            if (entry.active) {
                LOG_INFO("Target [" + std::to_string(i) + "] Debug Name: '" + entry.target.debugName + "'");
                LOG_INFO("  Size:         " + std::to_string(entry.target.extent.width) + "x" + std::to_string(entry.target.extent.height));
                LOG_INFO("  Format:       " + std::to_string(entry.target.format));
                LOG_INFO("  Usage:        " + std::to_string(entry.target.usage));
                LOG_INFO("  Layout:       " + std::to_string(entry.target.currentLayout));
                LOG_INFO("  Version:      " + std::to_string(entry.target.version));
            }
        }
        LOG_INFO("==============================================================");
    }

} // namespace eng::renderer
