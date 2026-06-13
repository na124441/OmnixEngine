#pragma once
#include <vulkan/vulkan.h>
#include "Rendering/Core/RenderTargetManager.h"
#include <vector>
#include <string>

namespace eng::renderer {

    struct FramebufferHandle {
        uint32_t index = UINT32_MAX;
        uint32_t generation = 0;

        bool IsValid() const {
            return index != UINT32_MAX;
        }

        bool operator==(const FramebufferHandle& other) const {
            return index == other.index && generation == other.generation;
        }

        bool operator!=(const FramebufferHandle& other) const {
            return !(*this == other);
        }
    };

    struct FramebufferDesc {
        VkRenderPass renderPass = VK_NULL_HANDLE;
        std::vector<RenderTargetHandle> attachments;
        std::vector<VkImageView> rawAttachments;
        uint32_t width = 0;
        uint32_t height = 0;
        uint32_t layers = 1;
        std::string debugName;
    };

    struct FramebufferResource {
        VkFramebuffer framebuffer = VK_NULL_HANDLE;
        FramebufferDesc desc;
        uint64_t version = 0;
        bool valid = false;
    };

    class FramebufferManager {
    public:
        FramebufferManager() = default;
        ~FramebufferManager();

        void Initialize(VkDevice device, RenderTargetManager* targetManager);
        void Shutdown();

        FramebufferHandle Create(const FramebufferDesc& desc);
        void Destroy(FramebufferHandle handle);
        
        VkFramebuffer Get(FramebufferHandle handle);
        const FramebufferResource* GetResource(FramebufferHandle handle) const;
        bool IsValid(FramebufferHandle handle) const;

        void InvalidateByRenderTarget(RenderTargetHandle target);
        void RebuildInvalidated();
        bool Validate(FramebufferHandle handle) const;

    private:
        VkDevice m_Device = VK_NULL_HANDLE;
        RenderTargetManager* m_TargetManager = nullptr;

        struct ManagerFramebufferEntry {
            FramebufferResource resource;
            uint32_t generation = 0;
            bool active = false;
        };

        std::vector<ManagerFramebufferEntry> m_Framebuffers;
        std::vector<uint32_t> m_FreeIndices;
    };

} // namespace eng::renderer
