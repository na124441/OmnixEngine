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

#include "Rendering/Lighting/ShadowAtlas.h"
#include "Rendering/Core/RenderScene.h"
#include "Rendering/GPUScene/GPUScene.h"
#include "Rendering/Visibility/FrustumCullPass.h"
#include "Rendering/Visibility/IndirectCommandBuildPass.h"
#include "Rendering/Visibility/HZBPass.h"
#include "Rendering/Visibility/OcclusionCullPass.h"
#include "Rendering/Visibility/RVGClusterCullPass.h"
#include "Rendering/Core/RenderTargetManager.h"
#include "Rendering/Core/FramebufferManager.h"
#include "Rendering/Core/FrameContext.h"
#include "Rendering/Core/RenderTypes.h"
#include "Rendering/Core/RenderStats.h"
#include "Rendering/Editor/EditorViewportRenderer.h"
#include "Rendering/Editor/SelectionOutlinePass.h"
#include "Rendering/Editor/EditorOverlayPass.h"
#include "RenderingEngine/Renderer/scene/RenderQueue.h"
#include "RenderingEngine/Renderer/scene/Camera.h"
#include "RenderingEngine/Renderer/LightingUBO.h"
#include "Rendering/Graph/RenderGraph.h"
#include "RenderingEngine/Renderer/gltf/GltfModel.h"
#include "Rendering/Radiance/RadianceSettings.h"
#include "Rendering/Radiance/RadianceGPUData.h"
#include "Rendering/Geometry/CapabilityTiers.h"
#include "Rendering/Geometry/Arena/GeometryArena.h"

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

        // Color Grading
        float contrast = 1.0f;
        float saturation = 1.0f;
        float whiteBalanceTemp = 0.0f;
        float whiteBalanceTint = 0.0f;
        glm::vec3 lift = glm::vec3(0.0f);
        glm::vec3 gammaVal = glm::vec3(1.0f);
        glm::vec3 gain = glm::vec3(1.0f);

        // Fog Settings
        uint32_t enableFog = 0;
        float fogDensity = 0.015f;
        float fogHeightFalloff = 0.05f;
        float fogBaseHeight = 0.0f;
        glm::vec3 fogColor = glm::vec3(0.5f, 0.6f, 0.7f);
    };

    struct SSAOSettings {
        bool enabled = true;
        float radius = 0.5f;
        float bias = 0.025f;
        float intensity = 1.5f;
    };

    struct RenderDebugConfig {
        bool enableShadowPass         = true;   // Shadow atlas / cascade maps
        bool enableDepthPrepass       = true;   // Depth pre-pass before GBuffer
        bool enableSSAO               = false;  // SSAO — enable when SSAO resources are allocated
        bool enableLightCulling       = true;   // Tiled/clustered light culling
        bool enableDeferredLighting   = true;   // Main deferred shading pass
        bool enableTransparentPass    = true;   // Forward-transparent geometry
        bool enablePostProcessing     = true;   // Tonemapping, color grading, fog
        bool enableEditorOverlay      = true;   // Editor gizmos / grid overlay

        bool disableBackfaceCulling   = false;
        bool forceDefaultMaterial     = false;
        bool showGBufferAlbedo        = false;
        bool disableFallback          = true;
    };

    class Renderer {
    public:
        struct FrameDiagnostics {
            uint32_t frameNumber = 0;
            uint32_t entitiesExtracted = 0;
            uint32_t opaqueItems = 0;
            uint32_t transparentItems = 0;
            uint32_t instancesUploaded = 0;
            uint32_t drawCalls = 0;
            uint32_t triangles = 0;
            uint32_t rejectedItems = 0;
            uint32_t validationErrors = 0;
        };

        FrameDiagnostics m_FrameDiagnostics;
        static FrameDiagnostics* GetCurrentDiagnostics();    friend bool RunGPUSceneTests(EngineResources& eng, GPUScene& scene, Renderer* renderer) noexcept;
    public:
        enum class VisibilityMode : uint32_t
        {
            CPUDriven = 0,
            GPUFrustumOnly = 1,
            GPUFrustumIndirect = 2,
            GPUFrustumOcclusion = 3,
            VisibilityBuffer = 4
        };

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
        bool CreateGBufferPipeline();
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
        void SetOffscreenRenderingEnabled(bool enabled);
        bool IsOffscreenRenderingEnabled() const { return m_ViewportRenderer.isOffscreenRenderingEnabled(); }
        void BakeReflectionProbes(eng::runtime::IECSWorld& world);
        void RenderSceneOffscreen(eng::runtime::IECSWorld& world, const glm::mat4& customView, const glm::mat4& customProj, const glm::vec3& customCamPos);
        void CopyImageToCPU(VkImage image, uint32_t width, uint32_t height, std::vector<float>& outPixels);
        void CreateOffscreenResources(uint32_t width, uint32_t height);
        void DestroyOffscreenResources() { m_ViewportRenderer.destroyOffscreenResources(); }
        VkDescriptorSet GetOffscreenTexture(uint32_t frameIdx) const { return m_ViewportRenderer.getOffscreenTexture(frameIdx); }
        VkDescriptorSet GetShadowTexture(uint32_t frameIdx) const;
        glm::mat4 getLastLightSpaceMatrix() const { return m_LastLightSpaceMatrix; }

        uint32_t GetOffscreenWidth() const { return m_ViewportRenderer.getOffscreenWidth(); }
        uint32_t GetOffscreenHeight() const { return m_ViewportRenderer.getOffscreenHeight(); }
        VkRenderPass GetOffscreenRenderPass() const { return m_OffscreenViewportRenderPass; }
        uint32_t PickEntity(uint32_t x, uint32_t y);
        bool CaptureScreenshot(const std::string& filename);

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
        VkDescriptorSet GetHZBTexture(uint32_t frameIdx) const;
        uint32_t GetHZBMipCount(uint32_t frameIdx) const;
        const RenderStats& GetRenderStats() const { return m_RenderStats; }
        void RequestRenderDocCapture() { m_RenderDocCaptureRequested = true; }
        Omnix::Radiance::RadianceSettings& GetRadianceSettings() { return m_RadianceSettings; }
        const Omnix::Radiance::RadianceSettings& GetRadianceSettings() const { return m_RadianceSettings; }

        RenderDebugConfig& GetDebugConfig() { return m_DebugConfig; }
        const RenderDebugConfig& GetDebugConfig() const { return m_DebugConfig; }

        const CapabilityReport& GetCapabilityReport() const { return m_CapabilityReport; }
        void SetDeveloperTierOverride(uint32_t tier) {
            if (tier != 0xFFFFFFFF && tier > 3) {
                LOG_ERROR("SetDeveloperTierOverride: Invalid tier override " + std::to_string(tier) + ". Tier must be 0, 1, 2, or 3.");
                return;
            }
            if (tier != 0xFFFFFFFF && tier > m_CapabilityReport.selectedTier) {
                LOG_ERROR("SetDeveloperTierOverride: Cannot force higher tier " + std::to_string(tier) + " than supported by hardware (Tier " + std::to_string(m_CapabilityReport.selectedTier) + ")");
                return;
            }
            m_DeveloperTierOverride = tier;
            LOG_INFO("SetDeveloperTierOverride: Developer tier override set to " + (tier == 0xFFFFFFFF ? "NONE" : "Tier " + std::to_string(tier)));
        }
        uint32_t GetActiveCapabilityTier() const {
            return m_DeveloperTierOverride != 0xFFFFFFFF ? m_DeveloperTierOverride : m_CapabilityReport.selectedTier;
        }

        const std::vector<uint32_t>& GetGpuVisibleInstances() const { return m_GpuVisibleInstances; }
        const std::vector<uint32_t>& GetGpuFinalVisibleInstances() const { return m_GpuFinalVisibleInstances; }
        uint32_t GetGpuFinalVisibleCount() const { return m_GpuFinalVisibleCount; }
        uint32_t GetGpuOcclusionCulledCount() const { return m_GpuOcclusionCulledCount; }
        uint32_t GetCpuRefVisibleCount() const { return m_CpuRefVisibleCount; }
        uint32_t GetGpuVisibleMeshCount() const { return m_GpuVisibleMeshCount; }
        VisibilityMode GetVisibilityMode() const { return m_VisibilityMode; }
        void SetVisibilityMode(VisibilityMode mode) { m_VisibilityMode = mode; }
        uint32_t GetGpuIndirectDrawCount() const { return m_GpuIndirectDrawCount; }
        uint32_t GetTotalInstanceCount() const { return m_TotalInstanceCount; }
        const GPUFrustum& GetCpuFrustum() const { return m_CpuFrustum; }
        void populateVisibilityDebugDraw(uint32_t selectedEntityId);

        // Member variables
        EngineResources& resources;
        
        VkPipeline      shadowPipeline       = VK_NULL_HANDLE;
        VkPipeline      geometryPipeline     = VK_NULL_HANDLE;
        VkPipeline      lightingPipeline    = VK_NULL_HANDLE;
        VkPipeline      postProcessPipeline = VK_NULL_HANDLE;

        uint32_t m_SelectedEntityID = 0;
        uint32_t m_FrameCount = 0;
        bool m_LocalViewActive = false;
        uint32_t m_LocalViewEntityID = 0;

        glm::vec3    lightDirection = glm::vec3(-0.5f, -1.0f, -0.3f);
        glm::vec3    lightColor     = glm::vec3(1.0f, 1.0f, 1.0f);
        float        lightIntensity = 1.0f;
        bool         m_UsePreviewLighting = false;
        bool         m_UseEditorDefaultLighting = true;
        VisibilityMode m_VisibilityMode = VisibilityMode::CPUDriven;
        bool         m_CPUFrustumCulling = true;

        struct VisibilityDebugSettings {
            bool showBounds = false;
            bool showFrustumVisible = false;
            bool showFrustumCulled = false;
            bool showOcclusionCulled = false;
            bool showFinalVisible = false;
            bool showDrawCount = true;
            bool showCullingStats = true;
        } m_VisibilityDebugSettings;

        struct RVGLODSettings {
            float lodBias = 0.0f;
            float targetPixelError = 2.0f;
            uint32_t maxTraversalDepth = 16;
            uint32_t debugMode = 0; // 0: Normal, 1: Hierarchy Level, 2: Geometric Error, 3: Projected Error, 4: Selected Nodes
            bool forceRoot = false;
            bool forceFullDetail = false;
            bool freezeSelection = false;
        } m_RVGLODSettings;

        FrustumCullPass m_FrustumCullPass;
        IndirectCommandBuildPass m_IndirectCommandBuildPass;
        HZBPass m_HZBPass;
        OcclusionCullPass m_OcclusionCullPass;
        RVGClusterCullPass m_RVGClusterCullPass;
        SelectionOutlinePass m_SelectionOutlinePass;
        EditorOverlayPass m_EditorOverlayPass;
        ViewportOverlaySettings m_OverlaySettings;
        mutable std::vector<VkDescriptorSet> m_HZBImGuiTextures;
        mutable std::vector<VkImageView> m_HZBLastViews;
        uint32_t     m_GpuVisibleMeshCount = 0;
        uint32_t     m_GpuFinalVisibleCount = 0;
        uint32_t     m_GpuOcclusionCulledCount = 0;
        uint32_t     m_GpuIndirectDrawCount = 0;
        std::vector<uint32_t> m_GpuVisibleInstances;
        std::vector<uint32_t> m_GpuFinalVisibleInstances;
        uint32_t     m_CpuRefVisibleCount = 0;
        uint32_t     m_TotalInstanceCount = 0;
        GPUFrustum   m_CpuFrustum = {};
        uint32_t     m_ShadingMode = 0;
        glm::vec3    ambientColor = glm::vec3(0.10f, 0.12f, 0.16f);
        float        ambientIntensity = 0.35f;

        RenderSceneCache scene;
        GPUScene gpuScene;
        RenderQueue renderQueue;
        RenderGraph renderGraph;
        Camera      camera;
        uint32_t frameIndex = 0;
        uint32_t m_CurrentFrameCount = 0;
        uint32_t currentSwapchainImageIndex = 0;
        bool m_SwapchainNeedsRecreation = false;
        bool m_IsInitialized = false;
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

        CapabilityReport m_CapabilityReport;
        uint32_t m_DeveloperTierOverride = 0xFFFFFFFF; // 0xFFFFFFFF for no override

        std::function<void()> recreateSwapChainCallback;

    public:
        eng::runtime::World* m_World = nullptr;
        const ::Scene* m_ActiveScene = nullptr;
        eng::runtime::AssetRegistry* m_AssetRegistry = nullptr;

    private:
        void recreateDepthResources(uint32_t width, uint32_t height);
        void updateGBufferDescriptorSets();
        void updateVisibilityResolveDescriptorSets();
        void updateVisibilityMeshDescriptorSets();
        void updateSoftwareRasterizerDescriptorSets();
        void updateRenderStats();

        GraphicsPipelineInfo        m_DepthPipeline;
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

        // ViewportColor resources
        VkRenderPass                    m_ViewportColorRenderPass = VK_NULL_HANDLE;
        std::vector<FramebufferHandle>  m_ViewportColorFbHandles;
        std::vector<VkFramebuffer>      m_ViewportColorFramebuffers;

        // G10: Visibility Buffer rendering resources
        VkRenderPass                m_VisibilityRenderPass = VK_NULL_HANDLE;
        std::vector<VkFramebuffer>  m_VisibilityFramebuffers;
        std::vector<VkFramebuffer>  m_OffscreenVisibilityFramebuffers;
        
        std::vector<RenderTargetHandle> m_VisibilityInstanceHandles;
        std::vector<RenderTargetHandle> m_VisibilityClusterHandles;
        std::vector<RenderTargetHandle> m_VisibilityPrimitiveHandles;
        std::vector<RenderTargetHandle> m_VisibilityDepthHandles;
        
        std::vector<FramebufferHandle>  m_VisibilityFbHandles;
        std::vector<FramebufferHandle>  m_OffscreenVisibilityFbHandles;

        std::vector<VkImage>        m_VisibilityInstanceImages;
        std::vector<VmaAllocation>  m_VisibilityInstanceAllocations;
        std::vector<VkImageView>    m_VisibilityInstanceImageViews;

        std::vector<VkImage>        m_VisibilityClusterImages;
        std::vector<VmaAllocation>  m_VisibilityClusterAllocations;
        std::vector<VkImageView>    m_VisibilityClusterImageViews;

        std::vector<VkImage>        m_VisibilityPrimitiveImages;
        std::vector<VmaAllocation>  m_VisibilityPrimitiveAllocations;
        std::vector<VkImageView>    m_VisibilityPrimitiveImageViews;

        std::vector<VkImage>        m_VisibilityDepthImages;
        std::vector<VmaAllocation>  m_VisibilityDepthAllocations;
        std::vector<VkImageView>    m_VisibilityDepthImageViews;

        VkPipeline                  m_VisibilityPipeline = VK_NULL_HANDLE;
        VkPipelineLayout            m_VisibilityPipelineLayout = VK_NULL_HANDLE;
        VkPipeline                  m_VisibilityMeshPipeline = VK_NULL_HANDLE;
        VkPipelineLayout            m_VisibilityMeshPipelineLayout = VK_NULL_HANDLE;
        PFN_vkCmdDrawMeshTasksEXT   m_pfnCmdDrawMeshTasksEXT = nullptr;
        VkDescriptorSetLayout       m_VisibilityMeshDescriptorSetLayout = VK_NULL_HANDLE;
        VkDescriptorPool            m_VisibilityMeshDescriptorPool = VK_NULL_HANDLE;
        std::vector<VkDescriptorSet> m_VisibilityMeshDescriptorSets;

        VkPipeline                  m_SoftwareRasterizerPipeline = VK_NULL_HANDLE;
        VkPipelineLayout            m_SoftwareRasterizerPipelineLayout = VK_NULL_HANDLE;
        VkDescriptorSetLayout       m_SoftwareRasterizerDescriptorSetLayout = VK_NULL_HANDLE;
        VkDescriptorPool            m_SoftwareRasterizerDescriptorPool = VK_NULL_HANDLE;
        std::vector<VkDescriptorSet> m_SoftwareRasterizerDescriptorSets;

        VkPipeline                  m_VisibilityResolvePipeline = VK_NULL_HANDLE;
        VkPipelineLayout            m_VisibilityResolvePipelineLayout = VK_NULL_HANDLE;
        VkDescriptorSetLayout       m_VisibilityResolveDescriptorSetLayout = VK_NULL_HANDLE;
        VkDescriptorPool            m_VisibilityResolveDescriptorPool = VK_NULL_HANDLE;
        std::vector<VkDescriptorSet> m_VisibilityResolveDescriptorSets;
        
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

        // ObjectID rendering resources
        std::vector<RenderTargetHandle> m_ObjectIDHandles;
        std::vector<VkImage>            m_ObjectIDImages;
        std::vector<VmaAllocation>      m_ObjectIDAllocations;
        std::vector<VkImageView>        m_ObjectIDImageViews;

        // GBufferVelocity resources
        std::vector<RenderTargetHandle> m_GBufferVelocityHandles;
        std::vector<VkImage>            m_GBufferVelocityImages;
        std::vector<VmaAllocation>      m_GBufferVelocityAllocations;
        std::vector<VkImageView>        m_GBufferVelocityImageViews;

        // Offscreen viewport rendering resources
        VkRenderPass                    m_OffscreenViewportRenderPass = VK_NULL_HANDLE;
        std::vector<VkFramebuffer>      m_OffscreenViewportFramebuffers;
        std::vector<FramebufferHandle>  m_OffscreenViewportFbHandles;

        // Indirect pipeline resources
        VkPipeline                      m_DepthIndirectPipeline = VK_NULL_HANDLE;
        VkPipelineLayout                m_DepthIndirectPipelineLayout = VK_NULL_HANDLE;
        VkPipeline                      m_GBufferIndirectPipeline = VK_NULL_HANDLE;
        VkPipelineLayout                m_GBufferIndirectPipelineLayout = VK_NULL_HANDLE;

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

        // SSR compute pipeline
        VkDescriptorSetLayout       m_SSRDescriptorSetLayout     = VK_NULL_HANDLE;
        VkDescriptorPool            m_SSRDescriptorPool          = VK_NULL_HANDLE;
        std::vector<VkDescriptorSet> m_SSRDescriptorSets;
        VkPipelineLayout            m_SSRPipelineLayout          = VK_NULL_HANDLE;
        VkPipeline                  m_SSRPipeline                = VK_NULL_HANDLE;

        // HDR Color target resources
        std::vector<VkImage>        m_HDRColorImages;
        std::vector<VmaAllocation>  m_HDRColorAllocations;
        std::vector<VkImageView>    m_HDRColorImageViews;
        std::vector<VkImage>        m_HDRColorComposedImages;
        std::vector<VmaAllocation>  m_HDRColorComposedAllocations;
        std::vector<VkImageView>    m_HDRColorComposedImageViews;
        std::vector<VkFramebuffer>  m_HDRColorFramebuffers;
        VkRenderPass                m_HDRRenderPass = VK_NULL_HANDLE;

        // PostProcess resources
        VkDescriptorSetLayout       m_PostProcessDescriptorSetLayout = VK_NULL_HANDLE;
        VkDescriptorPool            m_PostProcessDescriptorPool      = VK_NULL_HANDLE;
        std::vector<VkDescriptorSet> m_PostProcessDescriptorSets;
        VkPipelineLayout            m_PostProcessPipelineLayout      = VK_NULL_HANDLE;
        VkPipeline                  m_PostProcessPipeline            = VK_NULL_HANDLE;
        VkPipeline                  m_OffscreenPostProcessPipeline   = VK_NULL_HANDLE;

        // Auto Exposure compute resources
        VkDescriptorSetLayout       m_ExposureDescriptorSetLayout   = VK_NULL_HANDLE;
        VkDescriptorPool            m_ExposureDescriptorPool        = VK_NULL_HANDLE;
        std::vector<VkDescriptorSet> m_ExposureDescriptorSets;
        VkPipelineLayout            m_ExposurePipelineLayout        = VK_NULL_HANDLE;
        VkPipeline                  m_ExposurePipeline              = VK_NULL_HANDLE;
        VkBuffer                    m_ExposureBuffer                = VK_NULL_HANDLE;
        VmaAllocation               m_ExposureBufferAllocation      = VK_NULL_HANDLE;

        void initExposurePipeline();
        void destroyExposurePipeline();
        void updateExposureDescriptorSets();

        // TAA Compute resources
        VkDescriptorSetLayout       m_TAADescriptorSetLayout        = VK_NULL_HANDLE;
        VkDescriptorPool            m_TAADescriptorPool            = VK_NULL_HANDLE;
        std::vector<VkDescriptorSet> m_TAADescriptorSets;
        VkPipelineLayout            m_TAAPipelineLayout            = VK_NULL_HANDLE;
        VkPipeline                  m_TAAPipeline                  = VK_NULL_HANDLE;

        VkImage                     m_TAAHistoryImages[2]          = { VK_NULL_HANDLE, VK_NULL_HANDLE };
        VmaAllocation               m_TAAHistoryAllocations[2]     = { nullptr, nullptr };
        VkImageView                 m_TAAHistoryImageViews[2]      = { VK_NULL_HANDLE, VK_NULL_HANDLE };
        uint32_t                    m_TAAHistoryIndex              = 0;
        bool                        m_TAAInitialized               = false; ///< true after the first TAA frame completes

        void initTAAPipeline();
        void destroyTAAPipeline();
        void recreateTAAResources(uint32_t width, uint32_t height);
        void updateTAADescriptorSets();

        EditorViewportRenderer m_ViewportRenderer;
        VkRenderPass                m_SwapchainRenderPass = VK_NULL_HANDLE;

        GraphicsPipelineInfo        m_GBufferPipeline;
        VkPipelineLayout            m_GBufferPipelineLayout = VK_NULL_HANDLE;

        // Shadow mapping resources and methods
        GraphicsPipelineInfo        m_ShadowPipeline;
        VkPipelineLayout            m_ShadowPipelineLayout = VK_NULL_HANDLE;
        VkRenderPass                m_ShadowRenderPass = VK_NULL_HANDLE;
        std::vector<VkImage>        m_ShadowImages;
        std::vector<VmaAllocation>  m_ShadowAllocations;
        std::vector<VkImageView>    m_ShadowImageViews;
        std::vector<VkFramebuffer>  m_ShadowFramebuffers;
        std::vector<VkImage>        m_ShadowImagesCascades[4];
        std::vector<VmaAllocation>  m_ShadowAllocationsCascades[4];
        std::vector<VkImageView>    m_ShadowImageViewsCascades[4];
        std::vector<VkFramebuffer>  m_ShadowFramebuffersCascades[4];
        std::vector<RenderTargetHandle> m_ShadowHandlesCascades[4];
        std::vector<FramebufferHandle> m_ShadowFbHandlesCascades[4];
        glm::mat4                   m_LastLightSpaceMatrices[4];
        glm::vec4                   m_CascadeSplits;
        VkSampler                   m_ShadowSampler = VK_NULL_HANDLE;

        // Shadow Atlas
        std::vector<VkImage>            m_ShadowAtlasImages;
        std::vector<VmaAllocation>      m_ShadowAtlasAllocations;
        std::vector<VkImageView>        m_ShadowAtlasImageViews;
        std::vector<VkFramebuffer>      m_ShadowAtlasFramebuffers;
        std::vector<RenderTargetHandle> m_ShadowAtlasHandles;
        std::vector<FramebufferHandle>  m_ShadowAtlasFbHandles;
        uint32_t                    m_CurrentShadowResolution = 2048;
        glm::mat4                   m_LastLightSpaceMatrix{1.0f};
        mutable std::vector<VkDescriptorSet> m_ShadowImGuiTextures;

        VkPipeline                  m_GridPipeline = VK_NULL_HANDLE;
        VkPipelineLayout            m_GridPipelineLayout = VK_NULL_HANDLE;
        void initGridPipeline();
        void initSSRPipeline();
        void destroySSRPipeline();
        void destroyGridPipeline();

        void createShadowResources();
        void destroyShadowResources();

        RenderTargetManager m_RenderTargetManager;
        FramebufferManager m_FramebufferManager;
        GeometryArena       m_GeometryArena;
        std::vector<RenderTargetHandle> m_DepthHandles;
        std::vector<RenderTargetHandle> m_GBufferAHandles;
        std::vector<RenderTargetHandle> m_GBufferBHandles;
        std::vector<RenderTargetHandle> m_GBufferCHandles;
        std::vector<RenderTargetHandle> m_GBufferDHandles;
        std::vector<RenderTargetHandle> m_HDRColorHandles;
        std::vector<RenderTargetHandle> m_HDRColorComposedHandles;
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
        RenderDebugConfig m_DebugConfig;
        std::string m_LastEnvironmentPath = "";

        // TAA Jitter state
        uint32_t m_JitterIndex = 0;
        glm::vec2 m_CurrentJitter = glm::vec2(0.0f);
        glm::mat4 m_PrevViewProjection = glm::mat4(1.0f);
        glm::mat4 m_CurrentViewProjection = glm::mat4(1.0f);
    };

} // namespace eng::renderer
