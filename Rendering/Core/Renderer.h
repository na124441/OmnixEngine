#pragma once
#include <vector>
#include <memory>
#include <unordered_map>
#include <unordered_set>
#include <functional>
#include <filesystem>
#include <chrono>
#include <glm/glm.hpp>
#include <vulkan/vulkan.h>

#include "Core/Engine/Log.h"
#include "Core/Engine/Timer.h"
#include "Core/Engine/DebugLabels.h"
#include "Core/Vulkan/VkUtils.h"
#include "Core/Engine/EngineResources.h"

#include "Rendering/Core/RenderScene.h"
#include "Rendering/Scene/GPUScene.h"
#include "Rendering/Core/RenderTargetManager.h"
#include "Rendering/Core/FramebufferManager.h"
#include "Rendering/Core/FrameContext.h"
#include "Rendering/Core/RenderTypes.h"
#include "Rendering/Core/RenderStats.h"
#include "Rendering/Editor/EditorViewportRenderer.h"
#include "RenderingEngine/Renderer/scene/RenderQueue.h"
#include "RenderingEngine/Renderer/scene/Camera.h"
#include "RenderingEngine/Renderer/LightingUBO.h"
#include "Rendering/Graph/RenderGraph.h"
#include "RenderingEngine/Renderer/gltf/GltfModel.h"
#include "Rendering/Radiance/RadianceSettings.h"
#include "Rendering/Radiance/RadianceGPUData.h"

struct CameraComponent;

namespace eng::runtime {
    class World;
    class IECSWorld;
    class AssetRegistry;
}

namespace eng::renderer {
    struct CascadedShadowData {
        glm::mat4 lightViewProj[4];
        float cascadeSplits[4];
        uint32_t cascadeCount;
    };

    using ECSWorld = eng::runtime::IECSWorld;

    enum class ExposureMode : uint32_t {
        Manual = 0,
        Auto = 1
    };

    struct PostProcessSettings {
        float exposure = 1.0f;
        float gamma = 2.2f;
        float bloomThreshold = 1.0f;
        float bloomIntensity = 0.0f;
        ExposureMode exposureMode = ExposureMode::Manual;
        bool enableTonemapping = true;
        bool enableGammaCorrection = true;
        bool debugBeforePostProcess = false;
    };

    struct SSAOSettings {
        bool enabled = true;
        float radius = 0.5f;
        float bias = 0.025f;
        float intensity = 1.5f;
    };

    class Renderer {
    public:
        explicit Renderer(EngineResources& eng)
            : resources(eng) {}

        ~Renderer();

        void Initialize();
        void Shutdown();
        void BeginFrame();
        void RenderFrame(ECSWorld& world, const CameraComponent& camera);
        void EndFrame();

        void drawFrame(); // Compatibility entry point
        void RenderFrame(const FrameContext& context); // Compatibility entry point
        void onWindowResized();

        void loadModel(const std::string& path);

        RenderSceneCache& getScene() { return scene; }
        Camera&      getCamera() { return camera; }

        void initPipelines();
        void recreateOffscreenPostProcessPipeline();
        void setupRenderGraph();
        void buildRenderQueue();
        void buildPyramidMesh();
        void updateGlobalUBO();
        void updateLightingUBO();
        void UpdateRadianceFrameUBO(
            Omnix::Radiance::RadianceFrameUBO& ubo,
            const CameraData& camera,
            uint32_t viewportWidth,
            uint32_t viewportHeight,
            float timeSeconds
        );

        void setDirectionalLight(const glm::vec3& dir, const glm::vec3& col, float intensity = 1.0f)
        {
            lightDirection = glm::normalize(dir);
            lightColor = col;
            lightIntensity = intensity;
        }

        // Viewport Offscreen Rendering API
        void SetOffscreenRenderingEnabled(bool enabled) { m_ViewportRenderer.setOffscreenRenderingEnabled(enabled); }
        bool IsOffscreenRenderingEnabled() const { return m_ViewportRenderer.isOffscreenRenderingEnabled(); }
        void CreateOffscreenResources(uint32_t width, uint32_t height) { m_ViewportRenderer.createOffscreenResources(width, height); }
        void DestroyOffscreenResources() { m_ViewportRenderer.destroyOffscreenResources(); }
        VkDescriptorSet GetOffscreenTexture(uint32_t frameIdx) const { return m_ViewportRenderer.getOffscreenTexture(frameIdx); }
        VkDescriptorSet GetShadowTexture(uint32_t frameIdx) const;
        glm::mat4 getLastLightSpaceMatrix() const { return m_LastLightSpaceMatrix; }

        uint32_t GetOffscreenWidth() const { return m_ViewportRenderer.getOffscreenWidth(); }
        uint32_t GetOffscreenHeight() const { return m_ViewportRenderer.getOffscreenHeight(); }
        VkRenderPass GetOffscreenRenderPass() const { return m_ViewportRenderer.getOffscreenRenderPass(); }
        uint32_t PickEntity(uint32_t x, uint32_t y);

        void SetWorld(eng::runtime::World* world) { m_World = world; }
        void SetActiveScene(const ::Scene* scene) { m_ActiveScene = scene; }
        void SetAssetRegistry(eng::runtime::AssetRegistry* registry) { m_AssetRegistry = registry; }

        LightData getLastLightData() const { return m_LastLightData; }
        bool isFallbackLightingActive() const { return m_LastFallbackActive; }
        PostProcessSettings& GetPostProcessSettings() { return m_PostProcessSettings; }
        const PostProcessSettings& GetPostProcessSettings() const { return m_PostProcessSettings; }
        SSAOSettings& GetSSAOSettings() { return m_SSAOSettings; }
        const SSAOSettings& GetSSAOSettings() const { return m_SSAOSettings; }
        VkDescriptorSet GetSSAOBlurredTexture(uint32_t frameIdx) const;
        const RenderStats& GetRenderStats() const { return m_RenderStats; }
        void RequestRenderDocCapture() { m_RenderDocCaptureRequested = true; }
        Omnix::Radiance::RadianceSettings& GetRadianceSettings() { return m_RadianceSettings; }
        const Omnix::Radiance::RadianceSettings& GetRadianceSettings() const { return m_RadianceSettings; }

        // Member variables
        EngineResources& resources;
        
        VkPipeline      shadowPipeline       = VK_NULL_HANDLE;
        VkPipeline      geometryPipeline     = VK_NULL_HANDLE;
        VkPipeline      lightingPipeline    = VK_NULL_HANDLE;
        VkPipeline      postProcessPipeline = VK_NULL_HANDLE;

        uint32_t m_SelectedEntityID = 0;
        bool m_LocalViewActive = false;
        uint32_t m_LocalViewEntityID = 0;

        glm::vec3    lightDirection = glm::vec3(-0.5f, -1.0f, -0.3f);
        glm::vec3    lightColor     = glm::vec3(1.0f, 1.0f, 1.0f);
        float        lightIntensity = 1.0f;
        bool         m_UseEditorDefaultLighting = true;
        uint32_t     m_ShadingMode = 0;
        glm::vec3    ambientColor = glm::vec3(0.10f, 0.12f, 0.16f);
        float        ambientIntensity = 0.35f;

        RenderSceneCache scene;
        GPUScene gpuScene;
        RenderQueue renderQueue;
        RenderGraph renderGraph;
        Camera      camera;
        uint32_t frameIndex = 0;
        uint32_t currentSwapchainImageIndex = 0;
        bool m_SwapchainNeedsRecreation = false;
        FrameContext activeFrameContext;
        RenderScene activeRenderScene;
        eng::FrameTimer timer;

        std::vector<std::unique_ptr<GltfModel>> gltfModels;

        std::unordered_map<uint64_t, Mesh*> m_EcsMeshCache;
        std::unordered_map<uint64_t, Material*> m_EcsMaterialCache;
        std::unordered_map<uint64_t, std::filesystem::file_time_type> m_MaterialWriteTimes;
        std::unordered_set<uint64_t> m_EcsWarningHandles;
        uint32_t m_EcsAssignedMeshCount = 0;
        uint32_t m_EcsFallbackMeshCount = 0;

        Mesh* m_DefaultMesh = nullptr;
        Material* m_DefaultMaterial = nullptr;
        uint32_t m_StaticRenderCount = 0;
        uint32_t m_EcsRenderCount = 0;
        uint32_t m_TotalRenderCount = 0;
        RenderQueue                 transparentRenderQueue;
        uint32_t                    m_TransparentRenderCount = 0;

        LightData m_LastLightData = {};
        bool m_LastFallbackActive = true;
        PostProcessSettings m_PostProcessSettings;
        SSAOSettings m_SSAOSettings;
        float m_AutoExposure = 1.0f;
        RenderStats m_RenderStats;
        bool m_RenderDocCaptureRequested = false;

        std::function<void()> recreateSwapChainCallback;

    public:
        eng::runtime::World* m_World = nullptr;
        const ::Scene* m_ActiveScene = nullptr;
        eng::runtime::AssetRegistry* m_AssetRegistry = nullptr;

    private:
        void recreateDepthResources(uint32_t width, uint32_t height);
        void updateGBufferDescriptorSets();
        void updateRenderStats();

        VkPipeline                  m_DepthPipeline      = VK_NULL_HANDLE;
        VkRenderPass                m_DepthRenderPass    = VK_NULL_HANDLE;
        std::vector<VkFramebuffer>  m_DepthFramebuffers;
        std::vector<VkFramebuffer>  m_OffscreenDepthFramebuffers;
        std::vector<VkImage>        m_DepthImages;
        std::vector<VmaAllocation>  m_DepthAllocations;
        std::vector<VkImageView>    m_DepthImageViews;
        uint32_t                    m_DepthWidth         = 0;
        uint32_t                    m_DepthHeight        = 0;

        VkRenderPass                m_TransparentRenderPass = VK_NULL_HANDLE;
        std::vector<VkFramebuffer>  m_TransparentFramebuffers;
        std::vector<VkFramebuffer>  m_OffscreenTransparentFramebuffers;

        VkRenderPass                m_GeometryRenderPass = VK_NULL_HANDLE;
        std::vector<VkFramebuffer>  m_GeometryFramebuffers;

        // GBuffer resources
        VkRenderPass                m_GBufferRenderPass  = VK_NULL_HANDLE;
        std::vector<VkFramebuffer>  m_GBufferFramebuffers;
        std::vector<VkFramebuffer>  m_OffscreenGBufferFramebuffers;
        
        std::vector<VkImage>        m_GBufferAImages;
        std::vector<VmaAllocation>  m_GBufferAAllocations;
        std::vector<VkImageView>    m_GBufferAImageViews;

        std::vector<VkImage>        m_GBufferBImages;
        std::vector<VmaAllocation>  m_GBufferBAllocations;
        std::vector<VkImageView>    m_GBufferBImageViews;

        std::vector<VkImage>        m_GBufferCImages;
        std::vector<VmaAllocation>  m_GBufferCAllocations;
        std::vector<VkImageView>    m_GBufferCImageViews;

        std::vector<VkImage>        m_GBufferDImages;
        std::vector<VmaAllocation>  m_GBufferDAllocations;
        std::vector<VkImageView>    m_GBufferDImageViews;

        VkDescriptorSetLayout       m_GBufferDescriptorSetLayout = VK_NULL_HANDLE;
        VkDescriptorPool            m_GBufferDescriptorPool      = VK_NULL_HANDLE;
        std::vector<VkDescriptorSet> m_GBufferDescriptorSets;
        VkSampler                   m_GBufferSampler             = VK_NULL_HANDLE;

        VkPipelineLayout            m_DeferredPipelineLayout     = VK_NULL_HANDLE;
        VkPipeline                  m_DeferredLightingPipeline   = VK_NULL_HANDLE;
        VkPipeline                  m_OffscreenDeferredLightingPipeline = VK_NULL_HANDLE;

        // Light culling compute pipeline
        VkPipelineLayout            m_LightCullingPipelineLayout = VK_NULL_HANDLE;
        VkPipeline                  m_LightCullingPipeline       = VK_NULL_HANDLE;

        // HDR Color target resources
        std::vector<VkImage>        m_HDRColorImages;
        std::vector<VmaAllocation>  m_HDRColorAllocations;
        std::vector<VkImageView>    m_HDRColorImageViews;
        std::vector<VkFramebuffer>  m_HDRColorFramebuffers;
        VkRenderPass                m_HDRRenderPass = VK_NULL_HANDLE;

        // PostProcess resources
        VkDescriptorSetLayout       m_PostProcessDescriptorSetLayout = VK_NULL_HANDLE;
        VkDescriptorPool            m_PostProcessDescriptorPool      = VK_NULL_HANDLE;
        std::vector<VkDescriptorSet> m_PostProcessDescriptorSets;
        VkPipelineLayout            m_PostProcessPipelineLayout      = VK_NULL_HANDLE;
        VkPipeline                  m_PostProcessPipeline            = VK_NULL_HANDLE;
        VkPipeline                  m_OffscreenPostProcessPipeline   = VK_NULL_HANDLE;

        EditorViewportRenderer m_ViewportRenderer;
        VkRenderPass                m_SwapchainRenderPass = VK_NULL_HANDLE;

        // Shadow mapping resources and methods
        VkPipeline                  m_ShadowPipeline = VK_NULL_HANDLE;
        VkPipelineLayout            m_ShadowPipelineLayout = VK_NULL_HANDLE;
        VkRenderPass                m_ShadowRenderPass = VK_NULL_HANDLE;
        std::vector<VkImage>        m_ShadowImages;
        std::vector<VmaAllocation>  m_ShadowAllocations;
        std::vector<VkImageView>    m_ShadowImageViews;
        std::vector<VkFramebuffer>  m_ShadowFramebuffers;
        VkSampler                   m_ShadowSampler = VK_NULL_HANDLE;
        uint32_t                    m_CurrentShadowResolution = 2048;
        glm::mat4                   m_LastLightSpaceMatrix{1.0f};
        mutable std::vector<VkDescriptorSet> m_ShadowImGuiTextures;

        VkPipeline                  m_GridPipeline = VK_NULL_HANDLE;
        VkPipelineLayout            m_GridPipelineLayout = VK_NULL_HANDLE;
        void initGridPipeline();
        void destroyGridPipeline();

        void createShadowResources();
        void destroyShadowResources();

        RenderTargetManager m_RenderTargetManager;
        FramebufferManager m_FramebufferManager;
        std::vector<RenderTargetHandle> m_DepthHandles;
        std::vector<RenderTargetHandle> m_GBufferAHandles;
        std::vector<RenderTargetHandle> m_GBufferBHandles;
        std::vector<RenderTargetHandle> m_GBufferCHandles;
        std::vector<RenderTargetHandle> m_GBufferDHandles;
        std::vector<RenderTargetHandle> m_HDRColorHandles;
        std::vector<RenderTargetHandle> m_ShadowHandles;
        std::vector<RenderTargetHandle> m_LDRColorHandles;
        std::vector<RenderTargetHandle> m_ViewportColorHandles;
        std::vector<RenderTargetHandle> m_SSAOHandles;
        std::vector<RenderTargetHandle> m_SSAOBlurredHandles;
        std::vector<VkImage>        m_SSAOBlurredImages;
        std::vector<VmaAllocation>  m_SSAOBlurredAllocations;
        std::vector<VkImageView>    m_SSAOBlurredImageViews;
        std::vector<FramebufferHandle> m_SSAOFbHandles;
        std::vector<VkFramebuffer>  m_SSAOFramebuffers;
        std::vector<FramebufferHandle> m_SSAOBlurredFbHandles;
        std::vector<VkFramebuffer>  m_SSAOBlurredFramebuffers;

        VkPipeline                  m_SSAOPipeline = VK_NULL_HANDLE;
        VkPipelineLayout            m_SSAOPipelineLayout = VK_NULL_HANDLE;
        VkDescriptorSetLayout       m_SSAODescriptorSetLayout = VK_NULL_HANDLE;
        VkDescriptorPool            m_SSAODescriptorPool = VK_NULL_HANDLE;
        std::vector<VkDescriptorSet> m_SSAODescriptorSets;

        VkPipeline                  m_SSAOBlurPipeline = VK_NULL_HANDLE;
        VkPipelineLayout            m_SSAOBlurPipelineLayout = VK_NULL_HANDLE;
        VkDescriptorSetLayout       m_SSAOBlurDescriptorSetLayout = VK_NULL_HANDLE;
        VkDescriptorPool            m_SSAOBlurDescriptorPool = VK_NULL_HANDLE;
        std::vector<VkDescriptorSet> m_SSAOBlurDescriptorSets;

        VkImage                     m_SSAONoiseImage = VK_NULL_HANDLE;
        VmaAllocation               m_SSAONoiseAllocation = VK_NULL_HANDLE;
        VkImageView                 m_SSAONoiseImageView = VK_NULL_HANDLE;
        VkSampler                   m_SSAONoiseSampler = VK_NULL_HANDLE;

        std::vector<VkBuffer>       m_SSAOConstantBuffers;
        std::vector<VmaAllocation>  m_SSAOConstantAllocations;
        VkRenderPass                m_SSAORenderPass = VK_NULL_HANDLE;
        std::vector<glm::vec4>      m_SSAOKernel;
        mutable std::vector<VkDescriptorSet> m_SSAOBlurredImGuiTextures;

        void createSSAOResources();
        void destroySSAOResources();

        std::vector<FramebufferHandle> m_DepthFbHandles;
        std::vector<FramebufferHandle> m_OffscreenDepthFbHandles;
        std::vector<FramebufferHandle> m_GeometryFbHandles;
        std::vector<FramebufferHandle> m_GBufferFbHandles;
        std::vector<FramebufferHandle> m_OffscreenGBufferFbHandles;
        std::vector<FramebufferHandle> m_HDRColorFbHandles;
        std::vector<FramebufferHandle> m_TransparentFbHandles;
        std::vector<FramebufferHandle> m_OffscreenTransparentFbHandles;
        std::vector<FramebufferHandle> m_ShadowFbHandles;

        void ValidateStartupState();
        void ValidateSwapchain();
        void ValidateRenderTargets();
        void ValidateFramebuffers();
        void ValidatePipelines();
        void ValidateDescriptorLayouts();
        void ValidateRenderGraph();

        std::chrono::steady_clock::time_point m_CpuFrameStart{};
        std::chrono::steady_clock::time_point m_StartTime = std::chrono::steady_clock::now();

        Omnix::Radiance::RadianceSettings m_RadianceSettings;
    };

} // namespace eng::renderer
