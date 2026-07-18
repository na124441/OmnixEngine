#include "Core/pch.h"
#include "Rendering/Core/RenderTargetManager.h"
#include "Rendering/Core/FramebufferManager.h"
#include "Core/Vulkan/VkUtils.h"
#include <algorithm>
#include <cassert>

namespace eng::renderer {

    static std::string LayoutToString(VkImageLayout layout) {
        switch (layout) {
            case VK_IMAGE_LAYOUT_UNDEFINED: return "UNDEFINED";
            case VK_IMAGE_LAYOUT_GENERAL: return "GENERAL";
            case VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL: return "COLOR_ATTACHMENT_OPTIMAL";
            case VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL: return "DEPTH_STENCIL_ATTACHMENT_OPTIMAL";
            case VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL: return "DEPTH_STENCIL_READ_ONLY_OPTIMAL";
            case VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL: return "SHADER_READ_ONLY_OPTIMAL";
            case VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL: return "TRANSFER_SRC_OPTIMAL";
            case VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL: return "TRANSFER_DST_OPTIMAL";
            case VK_IMAGE_LAYOUT_PREINITIALIZED: return "PREINITIALIZED";
            case VK_IMAGE_LAYOUT_PRESENT_SRC_KHR: return "PRESENT_SRC_KHR";
            default: return "UNKNOWN(" + std::to_string(layout) + ")";
        }
    }

    static std::string AccessToString(VkAccessFlags access) {
        std::string res = "";
        if (access & VK_ACCESS_INDIRECT_COMMAND_READ_BIT) res += "INDIRECT_COMMAND_READ | ";
        if (access & VK_ACCESS_INDEX_READ_BIT) res += "INDEX_READ | ";
        if (access & VK_ACCESS_VERTEX_ATTRIBUTE_READ_BIT) res += "VERTEX_ATTRIBUTE_READ | ";
        if (access & VK_ACCESS_UNIFORM_READ_BIT) res += "UNIFORM_READ | ";
        if (access & VK_ACCESS_INPUT_ATTACHMENT_READ_BIT) res += "INPUT_ATTACHMENT_READ | ";
        if (access & VK_ACCESS_SHADER_READ_BIT) res += "SHADER_READ | ";
        if (access & VK_ACCESS_SHADER_WRITE_BIT) res += "SHADER_WRITE | ";
        if (access & VK_ACCESS_COLOR_ATTACHMENT_READ_BIT) res += "COLOR_ATTACHMENT_READ | ";
        if (access & VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT) res += "COLOR_ATTACHMENT_WRITE | ";
        if (access & VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_READ_BIT) res += "DEPTH_STENCIL_READ | ";
        if (access & VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT) res += "DEPTH_WRITE | ";
        if (access & VK_ACCESS_TRANSFER_READ_BIT) res += "TRANSFER_READ | ";
        if (access & VK_ACCESS_TRANSFER_WRITE_BIT) res += "TRANSFER_WRITE | ";
        if (access & VK_ACCESS_HOST_READ_BIT) res += "HOST_READ | ";
        if (access & VK_ACCESS_HOST_WRITE_BIT) res += "HOST_WRITE | ";
        if (access & VK_ACCESS_MEMORY_READ_BIT) res += "MEMORY_READ | ";
        if (access & VK_ACCESS_MEMORY_WRITE_BIT) res += "MEMORY_WRITE | ";
        if (!res.empty()) {
            res = res.substr(0, res.size() - 3);
        } else {
            res = "0";
        }
        return res;
    }

    static std::string StageToString(VkPipelineStageFlags stage) {
        std::string res = "";
        if (stage & VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT) res += "TOP_OF_PIPE | ";
        if (stage & VK_PIPELINE_STAGE_DRAW_INDIRECT_BIT) res += "DRAW_INDIRECT | ";
        if (stage & VK_PIPELINE_STAGE_VERTEX_INPUT_BIT) res += "VERTEX_INPUT | ";
        if (stage & VK_PIPELINE_STAGE_VERTEX_SHADER_BIT) res += "VERTEX_SHADER | ";
        if (stage & VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT) res += "FRAGMENT_SHADER | ";
        if (stage & VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT) res += "EARLY_FRAGMENT_TESTS | ";
        if (stage & VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT) res += "LATE_FRAGMENT_TESTS | ";
        if (stage & VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT) res += "COLOR_ATTACHMENT_OUTPUT | ";
        if (stage & VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT) res += "COMPUTE_SHADER | ";
        if (stage & VK_PIPELINE_STAGE_TRANSFER_BIT) res += "TRANSFER | ";
        if (stage & VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT) res += "BOTTOM_OF_PIPE | ";
        if (!res.empty()) {
            res = res.substr(0, res.size() - 3);
        } else {
            res = "0";
        }
        return res;
    }

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
        VkAccessFlags dstAccess,
        VkImageLayout expectedOldLayout,
        const std::string& passName,
        int frameIndex
    )
    {
        RenderTarget* target = Get(handle);
        if (!target) return;

        // Command Buffer validation (Phase 4)
        if (m_ActiveCommandBuffer != VK_NULL_HANDLE && cmd != m_ActiveCommandBuffer) {
            LOG_ERROR("[CommandBuffer Mismatch] Transition recorded into CommandBuffer " + 
                      std::to_string((uintptr_t)cmd) + " but active pass uses " + 
                      std::to_string((uintptr_t)m_ActiveCommandBuffer));
            #ifndef NDEBUG
            assert(false && "CommandBuffer validation failure");
            #endif
        }

        // Expected Old Layout Validation (Phase 1 part 2)
        if (expectedOldLayout != VK_IMAGE_LAYOUT_MAX_ENUM && target->currentLayout != expectedOldLayout) {
            LOG_ERROR("[Layout Validation ASSERT]\nResource: " + target->debugName + 
                      "\nTracked:  " + LayoutToString(target->currentLayout) + 
                      "\nExpected: " + LayoutToString(expectedOldLayout) + 
                      "\nPass:     " + passName);
            #ifndef NDEBUG
            assert(false && "Layout Validation failure");
            #endif
        }

        // Cross-validation (Phase 3 part 2)
        if (m_ActiveFramebuffer != VK_NULL_HANDLE && m_FramebufferManager) {
            if (newLayout == VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL || 
                newLayout == VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL) {
                bool hasAttachment = m_FramebufferManager->HasAttachment(m_ActiveFramebuffer, target->view);
                if (!hasAttachment) {
                    LOG_ERROR("[Frame Resource Mismatch] Transitioning image '" + target->debugName + 
                              "' to attachment layout " + LayoutToString(newLayout) + 
                              " but it is NOT attached to the active framebuffer " + 
                              std::to_string((uintptr_t)m_ActiveFramebuffer));
                    #ifndef NDEBUG
                    assert(false && "Frame Resource Mismatch: image view not in active framebuffer attachments");
                    #endif
                }
            }
        }

        // Detailed transition logging (Phase 1 part 1)
        std::string logMsg = "[Frame " + (frameIndex >= 0 ? std::to_string(frameIndex) : "N/A") + "]\n" +
                             "Pass:     " + passName + "\n" +
                             "Resource: " + target->debugName + "\n" +
                             "VkImage:  " + std::to_string((uintptr_t)target->image) + "\n" +
                             "Layout:   " + LayoutToString(target->currentLayout) + " -> " + LayoutToString(newLayout) + "\n" +
                             "Stage:    " + StageToString(srcStage) + " -> " + StageToString(dstStage) + "\n" +
                             "Access:   " + AccessToString(srcAccess) + " -> " + AccessToString(dstAccess);
        LOG_INFO(logMsg);

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
            std::string msg = "RenderTarget layout mismatch for '" + (target ? target->debugName : "Unknown") + 
                              "': expected " + LayoutToString(expected) + " but tracked layout is " + 
                              (target ? LayoutToString(target->currentLayout) : "NULL");
            LOG_ERROR(msg);
            #ifndef NDEBUG
            assert(false && "RenderTarget layout mismatch");
            #endif
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
