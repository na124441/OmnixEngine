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
#include "Rendering/Lighting/ShadowAtlas.h"
#include "Rendering/GPUScene/GPUInstance.h"
#include "Rendering/GPUScene/GPUMeshDrawData.h"
#include "Rendering/GPUScene/GPUVisibilityTypes.h"
#include <mutex>

class Scene;

namespace eng::renderer {

    struct ScheduledLocalLightShadow {
        uint32_t lightIndex;      // Index in localLightsList
        uint32_t faceIndex;       // 0 for spot lights, 0-5 for point lights
        uint32_t tileX;
        uint32_t tileY;
        uint32_t tileSize;
        glm::mat4 viewMatrix;
        glm::mat4 projMatrix;
        bool isSpot;
    };

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

        // G3: GPU Material Override Buffer
        VkBuffer materialOverrideBuffer = VK_NULL_HANDLE;
        VmaAllocation materialOverrideAlloc = VK_NULL_HANDLE;
        VkDeviceSize materialOverrideBufferSize = 0;
        uint32_t materialOverrideCapacity = 0;

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

    struct GPUSceneDiagnostics {
        uint32_t instanceCount = 0;
        uint32_t activeSlots = 0;
        uint32_t freeSlots = 0;
        uint32_t dirtyRangesCount = 0;
        uint64_t uploadBytesThisFrame = 0;
        uint32_t staleHandleErrors = 0;
        uint32_t gpuMeshRecordCount = 0;
        uint32_t materialOverrideCount = 0;
    };

    class GPUScene {
    public:
        GPUScene();
        ~GPUScene();

        GPUScene(const GPUScene&) = delete;
        GPUScene& operator=(const GPUScene&) = delete;
        GPUScene(GPUScene&&) noexcept = default;
        GPUScene& operator=(GPUScene&&) noexcept = default;

        void Initialize(EngineResources& resources);
        void Shutdown(EngineResources& resources);

        // G3 Stable Instance Allocation APIs
        GPUSceneInstanceHandle CreateInstance(const GPUGeometryInstance& initialData);
        void UpdateInstance(GPUSceneInstanceHandle handle, const GPUGeometryInstance& data);
        void DestroyInstance(GPUSceneInstanceHandle handle);
        bool IsInstanceValid(GPUSceneInstanceHandle handle) const;
        
        // Entity to Instance mapping
        void RegisterEntityInstance(uint32_t entityID, GPUSceneInstanceHandle handle);
        GPUSceneInstanceHandle GetEntityInstance(uint32_t entityID) const;
        void UnregisterEntityInstance(uint32_t entityID);

        // Material overrides
        void SetInstanceMaterialOverrides(GPUSceneInstanceHandle handle, const std::vector<uint32_t>& materialIDs);
        void ClearInstanceMaterialOverrides(GPUSceneInstanceHandle handle);

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

        GPUSceneDiagnostics GetDiagnostics() const;
        const std::vector<ScheduledLocalLightShadow>& GetScheduledShadows() const { return m_ScheduledShadows; }

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

        // Persistent Slot Tables
        struct InstanceSlot {
            GPUGeometryInstance instance;
            uint32_t generation = 0;
            bool active = false;
            uint32_t entityID = 0;
            uint32_t dirtyFrames = 0; // Number of frames remaining to upload this slot
            uint32_t overrideOffset = 0xFFFFFFFF; // Offset in overrides array
            uint32_t overrideCount = 0;
        };

        struct MeshSlot {
            GPUMeshRecord record;
            const Mesh* meshPtr = nullptr;
            uint32_t generation = 0;
            bool active = false;
        };

        VkDescriptorSetLayout m_DescriptorSetLayout = VK_NULL_HANDLE;
        VkDescriptorSetLayout m_LocalLightsDescriptorSetLayout = VK_NULL_HANDLE;
        VkDescriptorSetLayout m_LightCullingDescriptorSetLayout = VK_NULL_HANDLE;
        std::vector<GPUSceneFrameResources> m_Frames;

        // Stable records and managers
        std::vector<InstanceSlot> m_InstanceSlots;
        std::vector<uint32_t> m_FreeInstanceSlots;
        std::unordered_map<uint32_t, GPUSceneInstanceHandle> m_EntityToInstance;

        // Material overrides store
        std::vector<uint32_t> m_MaterialOverrides;

        // Mesh management
        std::vector<MeshSlot> m_MeshSlots;
        std::unordered_map<const Mesh*, uint32_t> m_MeshToIndex;

        // Compat caches populated in UpdateFrame
        std::vector<GPUInstance> m_GPUInstances;
        std::vector<GPUMeshDrawData> m_GPUMeshDrawData;

        // Shadow Atlas Allocation
        std::unique_ptr<ShadowAtlasAllocator> m_ShadowAtlasAllocator;
        std::vector<ScheduledLocalLightShadow> m_ScheduledShadows;

        struct LightCacheEntry {
            uint32_t x = 0;
            uint32_t y = 0;
            uint32_t size = 0;
            glm::vec3 lastPos{0.0f};
            glm::vec3 lastDir{0.0f};
            bool valid = false;
        };
        std::unordered_map<uint32_t, LightCacheEntry> m_SpotLightCache;
        std::unordered_map<uint32_t, std::array<LightCacheEntry, 6>> m_PointLightCache;

        // Stats
        uint64_t m_UploadBytesThisFrame = 0;
        uint32_t m_StaleHandleErrors = 0;
        uint32_t m_GrowthEvents = 0;

        mutable std::mutex m_SceneMutex;
    };



} // namespace eng::renderer
