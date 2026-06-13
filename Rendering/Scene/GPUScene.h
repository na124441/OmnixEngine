#pragma once
#include <vulkan/vulkan.h>
#include <vector>
#include <unordered_map>
#include <glm/glm.hpp>
#include "RenderingEngine/Core/Engine/EngineResources.h"
#include "Rendering/Core/RenderScene.h"
#include "RenderingEngine/Renderer/LightingUBO.h"
#include "RenderingEngine/Renderer/scene/RenderQueue.h"

namespace eng::renderer {

    struct GPUSceneFrameResources {
        VkBuffer cameraBuffer = VK_NULL_HANDLE;
        VmaAllocation cameraAlloc = VK_NULL_HANDLE;

        VkBuffer instanceBuffer = VK_NULL_HANDLE;
        VmaAllocation instanceAlloc = VK_NULL_HANDLE;
        VkDeviceSize instanceBufferSize = 0;
        uint32_t instanceCapacity = 0;

        VkBuffer materialBuffer = VK_NULL_HANDLE;
        VmaAllocation materialAlloc = VK_NULL_HANDLE;
        VkDeviceSize materialBufferSize = 0;
        uint32_t materialCapacity = 0;

        VkBuffer lightBuffer = VK_NULL_HANDLE;
        VmaAllocation lightAlloc = VK_NULL_HANDLE;

        VkBuffer objectIdBuffer = VK_NULL_HANDLE;
        VmaAllocation objectIdAlloc = VK_NULL_HANDLE;
        VkDeviceSize objectIdBufferSize = 0;
        uint32_t objectIdCapacity = 0;

        VkDescriptorPool descriptorPool = VK_NULL_HANDLE;
        VkDescriptorSet descriptorSet = VK_NULL_HANDLE;
    };

    class GPUScene {
    public:
        GPUScene() = default;
        ~GPUScene() = default;

        GPUScene(const GPUScene&) = delete;
        GPUScene& operator=(const GPUScene&) = delete;
        GPUScene(GPUScene&&) noexcept = default;
        GPUScene& operator=(GPUScene&&) noexcept = default;

        void Initialize(EngineResources& resources);
        void Shutdown(EngineResources& resources);

        void UpdateFrame(
            EngineResources& resources,
            uint32_t frameIndex,
            const RenderScene& renderScene,
            const std::vector<RenderItem>& renderQueueItems,
            const std::unordered_map<uint64_t, Material*>& ecsMaterialCache,
            const Material* defaultMaterial,
            uint32_t shadingMode = 0
        );

        VkDescriptorSetLayout GetDescriptorSetLayout() const { return m_DescriptorSetLayout; }
        VkDescriptorSet GetDescriptorSet(uint32_t frameIndex) const { return m_Frames[frameIndex].descriptorSet; }
        const GPUSceneFrameResources& GetFrameResources(uint32_t frameIndex) const { return m_Frames[frameIndex]; }

    private:
        void createDescriptorSetLayout(EngineResources& resources);
        void createFrameResources(EngineResources& resources, GPUSceneFrameResources& frameRes);
        void destroyFrameResources(EngineResources& resources, GPUSceneFrameResources& frameRes);

        void resizeBufferIfNeeded(
            EngineResources& resources,
            VkBuffer& buffer,
            VmaAllocation& allocation,
            VkDeviceSize& currentSize,
            uint32_t& currentCapacity,
            uint32_t neededCapacity,
            VkDeviceSize elementSize,
            VkBufferUsageFlags usage
        );

        void writeDescriptorSet(EngineResources& resources, GPUSceneFrameResources& frameRes);

        VkDescriptorSetLayout m_DescriptorSetLayout = VK_NULL_HANDLE;
        std::vector<GPUSceneFrameResources> m_Frames;
    };

} // namespace eng::renderer
