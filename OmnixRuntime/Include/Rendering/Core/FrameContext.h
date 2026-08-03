#pragma once
#include <cstdint>
#include <glm/glm.hpp>
#include <vulkan/vulkan.h>

namespace eng::runtime {
    class World;
    class IECSWorld;
}

namespace eng::renderer {

    struct FrameContext {
        uint32_t frameIndex = 0;
        double deltaTime = 0.0;
        glm::uvec2 viewportSize{0, 0};
        
        // Active camera data
        glm::mat4 viewMatrix{1.0f};
        glm::mat4 projectionMatrix{1.0f};
        glm::vec3 cameraPosition{0.0f};
        float cameraNear = 0.1f;
        float cameraFar = 1000.0f;
        float cameraFov = 60.0f;
        
        // Active command buffer and swapchain image
        VkCommandBuffer commandBuffer = VK_NULL_HANDLE;
        VkImage swapchainImage = VK_NULL_HANDLE;
        
        // Frame-local GPU resources
        VkBuffer uboBuffer = VK_NULL_HANDLE;
        VkDescriptorSet uboDescriptor = VK_NULL_HANDLE;
        VkDescriptorPool descriptorPool = VK_NULL_HANDLE;
        VkBuffer lightingUboBuffer = VK_NULL_HANDLE;
        VkDescriptorSet lightingDescriptor = VK_NULL_HANDLE;

        // Compatibility members
        eng::runtime::World* world = nullptr;
    };

} // namespace eng::renderer
