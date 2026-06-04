/*******************************************************************************************************************
 * @file  FrameResources.cpp
 * @brief Implementation of the per-frame resource management system.
 *******************************************************************************************************************/

#include "runtime/frame/FrameResources.h"
#include "rhi/RHI.h"
#include "core/log/Log.h"
#include <Result.h>
using namespace core;
namespace runtime {

    core::Result FrameResources::InitializeAllocators() noexcept
    {
        if (!m_LinearMemory || !m_RingMemory) {
            ENG_LOG_ERROR("Failed to allocate memory for frame allocators");
            return eng::core::Result(eng::core::ResultCode::OutOfMemory);
        }

        ENG_LOG_DEBUG("Initialized frame resources with {} KB linear, {} KB ring allocators",
            m_Config.linearAllocatorSize / 1024,
            m_Config.ringAllocatorSize / 1024);

        return eng::core::Result(); // Success
    }

    eng::core::Result FrameResources::CreateTransientBuffer(
        const eng::rhi::BufferDesc& desc,
        eng::rhi::BufferHandle& outHandle) noexcept
    {
        if (!m_RHIDevice) {
            return eng::core::Result(eng::core::ResultCode::NotInitialized);
        }

        if (m_BufferCount >= m_Config.tempBufferCount) {
            ENG_LOG_WARN("Transient buffer limit reached ({})", m_Config.tempBufferCount);
            return eng::core::Result(eng::core::ResultCode::OutOfMemory);
        }

        // Create buffer through RHI
        auto result = m_RHIDevice->CreateBuffer(desc, outHandle);
        if (result.IsFailure()) {
            ENG_LOG_ERROR("Failed to create transient buffer");
            return result;
        }

        // Track buffer for cleanup
        m_TransientBuffers.push_back({ outHandle, false });
        ++m_BufferCount;

        return eng::core::Result(); // Success
    }

    eng::core::Result FrameResources::CreateTransientTexture(
        const eng::rhi::TextureDesc& desc,
        eng::rhi::TextureHandle& outHandle) noexcept
    {
        if (!m_RHIDevice) {
            return eng::core::Result(eng::core::ResultCode::NotInitialized);
        }

        if (m_TextureCount >= m_Config.tempTextureCount) {
            ENG_LOG_WARN("Transient texture limit reached ({})", m_Config.tempTextureCount);
            return eng::core::Result(eng::core::ResultCode::OutOfMemory);
        }

        // Create texture through RHI
        auto result = m_RHIDevice->CreateTexture(desc, outHandle);
        if (result.IsFailure()) {
            ENG_LOG_ERROR("Failed to create transient texture");
            return result;
        }

        // Track texture for cleanup
        m_TransientTextures.push_back({ outHandle, false });
        ++m_TextureCount;

        return eng::core::Result(); // Success
    }

    eng::core::Result FrameResources::AllocateDescriptorSet(
        const eng::rhi::DescriptorSetDesc& desc,
        eng::rhi::DescriptorSetHandle& outHandle) noexcept
    {
        if (!m_RHIDevice) {
            return eng::core::Result(eng::core::ResultCode::NotInitialized);
        }

        if (m_DescriptorSetCount >= m_Config.descriptorSetPoolSize) {
            ENG_LOG_WARN("Descriptor set limit reached ({})", m_Config.descriptorSetPoolSize);
            return eng::core::Result(eng::core::ResultCode::OutOfMemory);
        }

        // Allocate descriptor set through RHI
        auto result = m_RHIDevice->AllocateDescriptorSet(desc, outHandle);
        if (result.IsFailure()) {
            ENG_LOG_ERROR("Failed to allocate descriptor set");
            return result;
        }

        // Track descriptor set for cleanup
        m_DescriptorSets.push_back(outHandle);
        ++m_DescriptorSetCount;

        return eng::core::Result(); // Success
    }

    eng::core::Result FrameResources::UpdateDescriptorSetBuffer(
        eng::rhi::DescriptorSetHandle setHandle,
        uint32_t binding,
        eng::rhi::BufferHandle buffer,
        size_t offset,
        size_t size) noexcept
    {
        if (!m_RHIDevice) {
            return eng::core::Result(eng::core::ResultCode::NotInitialized);
        }

        // Update descriptor set through RHI
        auto result = m_RHIDevice->UpdateDescriptorSetBuffer(setHandle, binding, buffer, offset, size);
        if (result.IsFailure()) {
            ENG_LOG_ERROR("Failed to update descriptor set buffer binding");
            return result;
        }

        return eng::core::Result(); // Success
    }

    eng::core::Result FrameResources::UpdateDescriptorSetTexture(
        eng::rhi::DescriptorSetHandle setHandle,
        uint32_t binding,
        eng::rhi::TextureHandle texture,
        eng::rhi::SamplerHandle sampler) noexcept
    {
        if (!m_RHIDevice) {
            return eng::core::Result(eng::core::ResultCode::NotInitialized);
        }

        // Update descriptor set through RHI
        auto result = m_RHIDevice->UpdateDescriptorSetTexture(setHandle, binding, texture, sampler);
        if (result.IsFailure()) {
            ENG_LOG_ERROR("Failed to update descriptor set texture binding");
            return result;
        }

        return eng::core::Result(); // Success
    }

    void FrameResources::CleanupTransientResources() noexcept
    {
        if (!m_RHIDevice) return;

        // Destroy transient buffers
        for (const auto& bufferInfo : m_TransientBuffers) {
            if (!bufferInfo.persistent) {
                m_RHIDevice->DestroyBuffer(bufferInfo.handle);
            }
        }
        m_TransientBuffers.clear();

        // Destroy transient textures
        for (const auto& textureInfo : m_TransientTextures) {
            if (!textureInfo.persistent) {
                m_RHIDevice->DestroyTexture(textureInfo.handle);
            }
        }
        m_TransientTextures.clear();

        // Free descriptor sets
        for (const auto& setHandle : m_DescriptorSets) {
            m_RHIDevice->FreeDescriptorSet(setHandle);
        }
        m_DescriptorSets.clear();

        // Reset counts
        m_BufferCount = 0;
        m_TextureCount = 0;
        m_DescriptorSetCount = 0;
    }

    void FrameResources::ResetAllocators() noexcept
    {
        // Reset linear allocator
        m_LinearAllocator.Reset();

        // Advance ring allocator generation
        m_RingAllocator.AdvanceGeneration();
    }

} // namespace eng::runtime
