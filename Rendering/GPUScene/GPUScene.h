#pragma once
#include <vulkan/vulkan.h>
#include <vector>
#include <unordered_map>
#include <glm/glm.hpp>
#include "RenderingEngine/Core/Engine/EngineResources.h"
#include "Rendering/Core/RenderScene.h"
#include "RenderingEngine/Renderer/LightingUBO.h"
#include "RenderingEngine/Renderer/scene/RenderQueue.h"
#include "Rendering/Radiance/RadianceGPUData.h"
#include "Rendering/Lighting/LocalLightGPU.h"
#include "Rendering/Lighting/ClusteredLightingTypes.h"
#include "GPUInstance.h"
#include "GPUVisibilityTypes.h"
#include "GPUMeshDrawData.h"

class Scene;

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

        VkBuffer meshDrawDataBuffer = VK_NULL_HANDLE;
        VmaAllocation meshDrawDataAlloc = VK_NULL_HANDLE;
        VkDeviceSize meshDrawDataBufferSize = 0;
        uint32_t meshDrawDataCapacity = 0;

        VkBuffer frustumBuffer = VK_NULL_HANDLE;
        VmaAllocation frustumAlloc = VK_NULL_HANDLE;

        VkDescriptorPool descriptorPool = VK_NULL_HANDLE;
        VkDescriptorSet descriptorSet = VK_NULL_HANDLE;
        VkDescriptorSet localLightsDescriptorSet = VK_NULL_HANDLE;
        VkDescriptorSet lightCullingDescriptorSet = VK_NULL_HANDLE;
        uint32_t localLightCount = 0;

        struct LocalLightBuffer {
            EngineResources* resources = nullptr;
            VkBuffer buffer = VK_NULL_HANDLE;
            VmaAllocation allocation = VK_NULL_HANDLE;
            VkDeviceSize size = 0;
            uint32_t capacity = 0;

            void Upload(const void* data, size_t dataSize);
        } localLightBuffer;

        // Clustered Lighting Buffers
        VkBuffer clusterBoundsBuffer = VK_NULL_HANDLE;
        VmaAllocation clusterBoundsAlloc = VK_NULL_HANDLE;
        VkDeviceSize clusterBoundsBufferSize = 0;
        uint32_t clusterBoundsCapacity = 0;

        VkBuffer clusterRangeBuffer = VK_NULL_HANDLE;
        VmaAllocation clusterRangeAlloc = VK_NULL_HANDLE;
        VkDeviceSize clusterRangeBufferSize = 0;
        uint32_t clusterRangeCapacity = 0;

        VkBuffer clusterLightIndexBuffer = VK_NULL_HANDLE;
        VmaAllocation clusterLightIndexAlloc = VK_NULL_HANDLE;
        VkDeviceSize clusterLightIndexBufferSize = 0;
        uint32_t clusterLightIndexCapacity = 0;

        VkBuffer clusterSettingsBuffer = VK_NULL_HANDLE;
        VmaAllocation clusterSettingsAlloc = VK_NULL_HANDLE;
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
            const Omnix::Radiance::RadianceFrameUBO& radianceUBO,
            uint32_t shadingMode = 0,
            const ::Scene* activeScene = nullptr
        );

        VkDescriptorSetLayout GetDescriptorSetLayout() const { return m_DescriptorSetLayout; }
        VkDescriptorSetLayout GetLocalLightsDescriptorSetLayout() const { return m_LocalLightsDescriptorSetLayout; }
        VkDescriptorSetLayout GetLightCullingDescriptorSetLayout() const { return m_LightCullingDescriptorSetLayout; }
        VkDescriptorSet GetDescriptorSet(uint32_t frameIndex) const { return m_Frames[frameIndex].descriptorSet; }
        VkDescriptorSet GetLocalLightsDescriptorSet(uint32_t frameIndex) const { return m_Frames[frameIndex].localLightsDescriptorSet; }
        VkDescriptorSet GetLightCullingDescriptorSet(uint32_t frameIndex) const { return m_Frames[frameIndex].lightCullingDescriptorSet; }
        uint32_t GetLocalLightCount(uint32_t frameIndex) const { return m_Frames[frameIndex].localLightCount; }
        const GPUSceneFrameResources& GetFrameResources(uint32_t frameIndex) const { return m_Frames[frameIndex]; }
        const std::vector<GPUInstance>& GetGPUInstances() const { return m_GPUInstances; }
        const std::vector<GPUMeshDrawData>& GetGPUMeshDrawData() const { return m_GPUMeshDrawData; }

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
        VkDescriptorSetLayout m_LocalLightsDescriptorSetLayout = VK_NULL_HANDLE;
        VkDescriptorSetLayout m_LightCullingDescriptorSetLayout = VK_NULL_HANDLE;
        std::vector<GPUSceneFrameResources> m_Frames;
        std::vector<GPUInstance> m_GPUInstances;
        std::vector<GPUMeshDrawData> m_GPUMeshDrawData;
    };

} // namespace eng::renderer
