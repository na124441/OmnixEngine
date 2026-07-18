#pragma once

#include "RenderingEngine/Core/memory/LinearAllocator.h"
#include "Core/memory/RingAllocator.h"
#include "Core/containers/Array.h"
#include "Core/types/Handle.h"
#include "Core/types/Result.h"
#include "RHI/RHI.h"
#include <cstdint>
#include <vector>
#include <memory>

namespace eng::runtime {

    /**
     * @class FrameResources
     *
     * Central manager for all per-frame transient resources.
     */
    class FrameResources {
    public:
        struct Config {
            size_t linearAllocatorSize = 8 * 1024 * 1024;    // 8 MB
            size_t ringAllocatorSize = 2 * 1024 * 1024;     // 2 MB
            size_t descriptorSetPoolSize = 1024;            // 1024 descriptor sets
            size_t tempBufferCount = 256;                   // Max temporary buffers
            size_t tempTextureCount = 64;                   // Max temporary textures
        };

        explicit FrameResources(eng::rhi::Device* rhiDevice);
        FrameResources(eng::rhi::Device* rhiDevice, const Config& config);
        ~FrameResources();

        void Reset() noexcept;

        [[nodiscard]] eng::rhi::FenceHandle BeginFrame();
        [[nodiscard]] eng::core::Result EndFrame();

        eng::core::LinearAllocator<>& GetLinearAllocator() noexcept { return m_LinearAllocator; }
        eng::core::RingAllocator<>& GetRingAllocator() noexcept { return m_RingAllocator; }

        [[nodiscard]] eng::core::Result CreateTransientBuffer(
            const void* desc, // Placeholder for BufferDesc
            eng::rhi::BufferHandle& outHandle) noexcept;

        [[nodiscard]] eng::core::Result CreateTransientTexture(
            const void* desc, // Placeholder for TextureDesc
            eng::rhi::TextureHandle& outHandle) noexcept;

        template<typename T>
        [[nodiscard]] eng::core::Result CreateUniformBuffer(
            const T* data,
            eng::rhi::BufferHandle& outHandle) noexcept
        {
            if (!data) return eng::core::Result(eng::core::ResultCode::InvalidArgument);
            T* staging = m_RingAllocator.Allocate<T>();
            if (!staging) return eng::core::Result(eng::core::ResultCode::OutOfMemory);
            *staging = *data;
            return eng::core::Result();
        }

        [[nodiscard]] eng::core::Result AllocateDescriptorSet(
            const void* desc, // Placeholder for DescriptorSetDesc
            eng::rhi::DescriptorSetHandle& outHandle) noexcept;

        eng::rhi::FenceHandle GetFrameFence() const noexcept { return m_CurrentFence; }
        void SetFrameFence(eng::rhi::FenceHandle fence) noexcept { m_CurrentFence = fence; }
        uint64_t GetFrameIndex() const noexcept { return m_FrameIndex; }

    private:
        eng::rhi::Device* m_RHIDevice;
        Config m_Config;
        eng::core::LinearAllocator<> m_LinearAllocator;
        eng::core::RingAllocator<> m_RingAllocator;
        std::unique_ptr<uint8_t[]> m_LinearMemory;
        std::unique_ptr<uint8_t[]> m_RingMemory;

        struct TransientBufferInfo {
            eng::rhi::BufferHandle handle;
            bool persistent;
        };

        struct TransientTextureInfo {
            eng::rhi::TextureHandle handle;
            bool persistent;
        };

        std::vector<TransientBufferInfo> m_TransientBuffers;
        std::vector<TransientTextureInfo> m_TransientTextures;
        std::vector<eng::rhi::DescriptorSetHandle> m_DescriptorSets;

        eng::rhi::FenceHandle m_CurrentFence;
        uint64_t m_FrameIndex = 0;
    };

} // namespace eng::runtime
