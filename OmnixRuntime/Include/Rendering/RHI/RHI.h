#pragma once

#include "RHIBuffer.h"
#include "RHICommandBuffer.h"
#include "RHIDevice.h"
#include "RHIPipeline.h"
#include "RHIRenderPass.h"
#include "RHISwapchain.h"
#include "RHISync.h"
#include "RHITexture.h"

namespace eng::renderer {
    using RHISwapChain = eng::vulkan::VulkanSwapChain;
    using RHIShaderModule = VkShaderModule;
    using RHIDescriptorSet = VkDescriptorSet;
    using RHIDescriptorSetLayout = VkDescriptorSetLayout;
    using RHIDescriptorPool = VkDescriptorPool;
    using RHICommandPool = VkCommandPool;
    using RHIQueue = VkQueue;
}
