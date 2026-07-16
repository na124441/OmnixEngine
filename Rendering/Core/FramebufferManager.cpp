#include "Core/pch.h"
#include "FramebufferManager.h"
#include "Core/Vulkan/VkUtils.h"
#include "Core/Engine/Log.h"
#include <stdexcept>
#include <algorithm>

namespace eng::renderer {

    FramebufferManager::~FramebufferManager() {
        Shutdown();
    }

    void FramebufferManager::Initialize(VkDevice device, RenderTargetManager* targetManager) {
        m_Device = device;
        m_TargetManager = targetManager;
    }

    void FramebufferManager::Shutdown() {
        if (m_Device == VK_NULL_HANDLE) return;

        for (auto& entry : m_Framebuffers) {
            if (entry.active && entry.resource.framebuffer != VK_NULL_HANDLE) {
                vkDestroyFramebuffer(m_Device, entry.resource.framebuffer, nullptr);
            }
        }
        m_Framebuffers.clear();
        m_FreeIndices.clear();
        m_Device = VK_NULL_HANDLE;
        m_TargetManager = nullptr;
    }

    FramebufferHandle FramebufferManager::Create(const FramebufferDesc& desc) {
        if (m_Device == VK_NULL_HANDLE || m_TargetManager == nullptr) {
            throw std::runtime_error("FramebufferManager: Not initialized.");
        }

        uint32_t index = 0;
        if (!m_FreeIndices.empty()) {
            index = m_FreeIndices.back();
            m_FreeIndices.pop_back();
        } else {
            index = static_cast<uint32_t>(m_Framebuffers.size());
            m_Framebuffers.emplace_back();
        }

        auto& entry = m_Framebuffers[index];
        entry.active = true;
        entry.resource.desc = desc;
        entry.resource.version = 1;
        entry.resource.valid = false; // Trigger rebuild

        // Perform initial rebuild immediately
        entry.resource.framebuffer = VK_NULL_HANDLE;
        RebuildInvalidated();

        FramebufferHandle handle{};
        handle.index = index;
        handle.generation = entry.generation;
        return handle;
    }

    void FramebufferManager::Destroy(FramebufferHandle handle) {
        if (!IsValid(handle)) return;

        auto& entry = m_Framebuffers[handle.index];
        if (entry.resource.framebuffer != VK_NULL_HANDLE) {
            vkDestroyFramebuffer(m_Device, entry.resource.framebuffer, nullptr);
            entry.resource.framebuffer = VK_NULL_HANDLE;
        }

        entry.active = false;
        entry.generation++;
        entry.resource.valid = false;
        m_FreeIndices.push_back(handle.index);
    }

    VkFramebuffer FramebufferManager::Get(FramebufferHandle handle) {
        if (!IsValid(handle)) return VK_NULL_HANDLE;
        auto& entry = m_Framebuffers[handle.index];
        if (!entry.resource.valid) {
            RebuildInvalidated();
        }
        return entry.resource.framebuffer;
    }

    const FramebufferResource* FramebufferManager::GetResource(FramebufferHandle handle) const {
        if (!IsValid(handle)) return nullptr;
        return &m_Framebuffers[handle.index].resource;
    }

    bool FramebufferManager::IsValid(FramebufferHandle handle) const {
        if (handle.index >= m_Framebuffers.size()) return false;
        const auto& entry = m_Framebuffers[handle.index];
        return entry.active && entry.generation == handle.generation;
    }

    void FramebufferManager::InvalidateByRenderTarget(RenderTargetHandle target) {
        for (auto& entry : m_Framebuffers) {
            if (!entry.active) continue;
            auto& attachments = entry.resource.desc.attachments;
            if (std::find(attachments.begin(), attachments.end(), target) != attachments.end()) {
                entry.resource.valid = false;
            }
        }
    }

    void FramebufferManager::RebuildInvalidated() {
        if (m_Device == VK_NULL_HANDLE || m_TargetManager == nullptr) return;

        for (auto& entry : m_Framebuffers) {
            if (!entry.active || entry.resource.valid) continue;

            // Destroy old framebuffer if it exists
            if (entry.resource.framebuffer != VK_NULL_HANDLE) {
                vkDestroyFramebuffer(m_Device, entry.resource.framebuffer, nullptr);
                entry.resource.framebuffer = VK_NULL_HANDLE;
            }

            // Gather attachment image views
            std::vector<VkImageView> vkAttachments;
            bool attachmentsValid = true;

            if (!entry.resource.desc.rawAttachments.empty()) {
                vkAttachments = entry.resource.desc.rawAttachments;
            } else {
                vkAttachments.reserve(entry.resource.desc.attachments.size());
                for (auto targetHandle : entry.resource.desc.attachments) {
                    const RenderTarget* target = m_TargetManager->Get(targetHandle);
                    if (target && target->IsValid()) {
                        if (target->extent.width != entry.resource.desc.width || target->extent.height != entry.resource.desc.height) {
                            LOG_ERROR("[FramebufferError]\nFramebuffer: " + entry.resource.desc.debugName +
                                      "\nExpected: " + std::to_string(entry.resource.desc.width) + "x" + std::to_string(entry.resource.desc.height) +
                                      "\nAttachment " + target->debugName + ": " + std::to_string(target->extent.width) + "x" + std::to_string(target->extent.height));
                            attachmentsValid = false;
                            break;
                        }
                        vkAttachments.push_back(target->view);
                    } else {
                        LOG_WARN("FramebufferManager: Rebuild skipped due to invalid target attachment for '" + entry.resource.desc.debugName + "'");
                        attachmentsValid = false;
                        break;
                    }
                }
            }

            if (!attachmentsValid) continue;

            VkFramebufferCreateInfo fbInfo{};
            fbInfo.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
            fbInfo.renderPass = entry.resource.desc.renderPass;
            fbInfo.attachmentCount = static_cast<uint32_t>(vkAttachments.size());
            fbInfo.pAttachments = vkAttachments.data();
            fbInfo.width = entry.resource.desc.width;
            fbInfo.height = entry.resource.desc.height;
            fbInfo.layers = entry.resource.desc.layers;

            if (vkCreateFramebuffer(m_Device, &fbInfo, nullptr, &entry.resource.framebuffer) == VK_SUCCESS) {
                entry.resource.valid = true;
                entry.resource.version++;
                LOG_INFO("FramebufferManager: Rebuilt framebuffer '" + entry.resource.desc.debugName + "' successfully (" + std::to_string(entry.resource.desc.width) + "x" + std::to_string(entry.resource.desc.height) + ")");
            } else {
                LOG_ERROR("FramebufferManager: Failed to recreate framebuffer '" + entry.resource.desc.debugName + "'");
            }
        }
    }

    bool FramebufferManager::Validate(FramebufferHandle handle) const {
        if (!IsValid(handle)) return false;
        const auto& entry = m_Framebuffers[handle.index];
        return entry.resource.valid && entry.resource.framebuffer != VK_NULL_HANDLE;
    }

    bool FramebufferManager::HasAttachment(VkFramebuffer fb, VkImageView view) const {
        for (const auto& entry : m_Framebuffers) {
            if (entry.active && entry.resource.framebuffer == fb) {
                // Check desc.rawAttachments
                if (!entry.resource.desc.rawAttachments.empty()) {
                    for (auto rawView : entry.resource.desc.rawAttachments) {
                        if (rawView == view) return true;
                    }
                }
                // Check desc.attachments
                if (m_TargetManager) {
                    for (auto targetHandle : entry.resource.desc.attachments) {
                        const RenderTarget* target = m_TargetManager->Get(targetHandle);
                        if (target && target->view == view) {
                            return true;
                        }
                    }
                }
            }
        }
        return false;
    }

} // namespace eng::renderer
