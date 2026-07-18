#pragma once
#include <vulkan/vulkan.h>
#include "Core/Engine/VmaUsage.h"
#include "Core/Engine/Log.h"
#include <string>
#include <vector>

namespace eng::renderer {

    struct RenderTargetDesc {
        uint32_t width = 0;
        uint32_t height = 0;
        VkFormat format = VK_FORMAT_UNDEFINED;
        VkImageUsageFlags usage = 0;
        VkImageAspectFlags aspect = 0;
        VkSampleCountFlagBits samples = VK_SAMPLE_COUNT_1_BIT;
        bool sampled = false;
        bool colorAttachment = false;
        bool depthAttachment = false;
        bool storage = false;
        std::string debugName;
    };

    struct RenderTarget {
        VkImage image = VK_NULL_HANDLE;
        VkImageView view = VK_NULL_HANDLE;
        VmaAllocation allocation = nullptr;
        VkFormat format = VK_FORMAT_UNDEFINED;
        VkExtent2D extent{0, 0};
        VkImageUsageFlags usage = 0;
        VkImageAspectFlags aspect = 0;
        VkImageLayout currentLayout = VK_IMAGE_LAYOUT_UNDEFINED;
        std::string debugName;
        uint64_t version = 0;

        bool IsValid() const {
            return image != VK_NULL_HANDLE &&
                   view != VK_NULL_HANDLE &&
                   extent.width > 0 &&
                   extent.height > 0;
        }
    };

    struct RenderTargetHandle {
        uint32_t index = UINT32_MAX;
        uint32_t generation = 0;

        bool IsValid() const {
            return index != UINT32_MAX;
        }

        bool operator==(const RenderTargetHandle& other) const {
            return index == other.index && generation == other.generation;
        }

        bool operator!=(const RenderTargetHandle& other) const {
            return !(*this == other);
        }
    };

    class FramebufferManager;

    class RenderTargetManager {
    public:
        RenderTargetManager() = default;
        ~RenderTargetManager();

        void Initialize(VkDevice device, VmaAllocator allocator);
        void Shutdown();

        RenderTargetHandle Create(const RenderTargetDesc& desc);
        void Destroy(RenderTargetHandle handle);
        RenderTargetHandle Recreate(RenderTargetHandle oldHandle, const RenderTargetDesc& newDesc);

        RenderTarget* Get(RenderTargetHandle handle);
        const RenderTarget* Get(RenderTargetHandle handle) const;
        bool IsValid(RenderTargetHandle handle) const;

        void Transition(
            VkCommandBuffer cmd,
            RenderTargetHandle handle,
            VkImageLayout newLayout,
            VkPipelineStageFlags srcStage,
            VkPipelineStageFlags dstStage,
            VkAccessFlags srcAccess,
            VkAccessFlags dstAccess,
            VkImageLayout expectedOldLayout = VK_IMAGE_LAYOUT_MAX_ENUM,
            const std::string& passName = "Unknown",
            int frameIndex = -1
        );

        void AssertLayout(RenderTargetHandle handle, VkImageLayout expected);
        void DumpAllTargets() const;

        void SetFramebufferManager(FramebufferManager* fbm) { m_FramebufferManager = fbm; }
        void SetActiveFramebuffer(VkFramebuffer fb) { m_ActiveFramebuffer = fb; }
        void SetActiveCommandBuffer(VkCommandBuffer cmd) { m_ActiveCommandBuffer = cmd; }

    private:
        VkDevice m_Device = VK_NULL_HANDLE;
        VmaAllocator m_Allocator = VK_NULL_HANDLE;

        FramebufferManager* m_FramebufferManager = nullptr;
        VkFramebuffer m_ActiveFramebuffer = VK_NULL_HANDLE;
        VkCommandBuffer m_ActiveCommandBuffer = VK_NULL_HANDLE;

        struct ManagerTargetEntry {
            RenderTarget target;
            uint32_t generation = 0;
            bool active = false;
        };

        std::vector<ManagerTargetEntry> m_Targets;
        std::vector<uint32_t> m_FreeIndices;
    };

} // namespace eng::renderer
