#pragma once
#include "Core/Engine/Log.h"
#include "Core/Engine/Timer.h"
#include "Core/Engine/DebugLabels.h"
#include "Core/Vulkan/VkUtils.h"
#include "Core/Engine/EngineResources.h"
#include "scene/Scene.h"
#include "scene/RenderQueue.h"
#include "scene/Camera.h"
#include "scene/GlobalUBO.h"
#include "graph/RenderGraph.h"
#include "gltf/GltfModel.h"
#include "RenderingEngine/Renderer/LightingUBO.h"
#include <unordered_map>
#include <unordered_set>

namespace eng::runtime {
    class World;
    class AssetRegistry;
}

namespace eng::renderer {

class SceneRenderer {
public:
    explicit SceneRenderer(EngineResources& eng)
        : resources(eng) {}

    // -----------------------------------------------------------------
    // High‑level API used by the application
    void init();      // creates a dummy scene (pyramid) – called after Vulkan init
    void cleanup();   // destroys scene, meshes, etc.
    void drawFrame(); // same entry‑point signature as PyramidRenderer::drawFrame()
    void onWindowResized(); // forwards to swapchain recreation if needed
    
    // Load a 3D model and add it to the scene
    void loadModel(const std::string& path);

    // -----------------------------------------------------------------
    // Access to the scene for user code (e.g. load assets later)
    RenderSceneCache& getScene() { return scene; }
    Camera&      getCamera() { return camera; }

    void initPipelines();
    void setupRenderGraph();
    void buildRenderQueue();
    void buildPyramidMesh(); // convenience – creates the same geometry the legacy renderer uses
    void updateGlobalUBO();  // Phase 5
    void updateLightingUBO();

    // Light setters
    void setDirectionalLight(const glm::vec3& dir, const glm::vec3& col, float intensity = 1.0f)
    {
        lightDirection = glm::normalize(dir);
        lightColor = col;
        lightIntensity = intensity;
    }

    EngineResources& resources; // non‑owning reference to shared Vulkan objects

    // Pipelines (real for geometry, stubs for others)
    VkPipeline      shadowPipeline       = VK_NULL_HANDLE;
    VkPipeline      geometryPipeline     = VK_NULL_HANDLE; // same as resources.graphicsPipeline
    VkPipeline      lightingPipeline    = VK_NULL_HANDLE;
    VkPipeline      postProcessPipeline = VK_NULL_HANDLE;

    // Directional light properties
    glm::vec3    lightDirection = glm::vec3(-0.5f, -1.0f, -0.3f);
    glm::vec3    lightColor     = glm::vec3(1.0f, 1.0f, 1.0f);
    float        lightIntensity = 1.0f;
    bool         m_UseEditorDefaultLighting = true;
    uint32_t     m_ShadingMode = 0; // 0 = Lit, 1 = Unlit
    glm::vec3    ambientColor = glm::vec3(0.10f, 0.12f, 0.16f);
    float        ambientIntensity = 0.35f;

    // -----------------------------------------------------------------
    RenderSceneCache scene;          // owns meshes, materials, render objects
    RenderQueue renderQueue;    // <-- NEW
    RenderGraph renderGraph;    // <-- NEW (Phase 3)
    Camera      camera;         // <-- NEW (Phase 5)
    uint32_t frameIndex = 0;
    uint32_t currentSwapchainImageIndex = 0;
    eng::FrameTimer timer;

    std::vector<std::unique_ptr<GltfModel>> gltfModels;

    // Viewport Offscreen Rendering API
    void SetOffscreenRenderingEnabled(bool enabled);
    bool IsOffscreenRenderingEnabled() const { return m_OffscreenRenderingEnabled; }
    void CreateOffscreenResources(uint32_t width, uint32_t height);
    void DestroyOffscreenResources();
    VkDescriptorSet GetOffscreenTexture(uint32_t frameIndex) const;

    uint32_t GetOffscreenWidth() const { return m_OffscreenWidth; }
    uint32_t GetOffscreenHeight() const { return m_OffscreenHeight; }

    void SetWorld(eng::runtime::World* world) { m_World = world; }
    eng::runtime::World* m_World = nullptr;

    void SetAssetRegistry(eng::runtime::AssetRegistry* registry) { m_AssetRegistry = registry; }
    eng::runtime::AssetRegistry* m_AssetRegistry = nullptr;

    std::function<void()> recreateSwapChainCallback;

private:
    bool m_OffscreenRenderingEnabled = false;
    uint32_t m_OffscreenWidth = 1280;
    uint32_t m_OffscreenHeight = 720;
    VkRenderPass m_OffscreenRenderPass = VK_NULL_HANDLE;
    std::vector<VkImage> m_OffscreenImages;
    std::vector<VmaAllocation> m_OffscreenAllocations;
    std::vector<VkImageView> m_OffscreenImageViews;
    std::vector<VkFramebuffer> m_OffscreenFramebuffers;
    VkSampler m_OffscreenSampler = VK_NULL_HANDLE;
    std::vector<VkDescriptorSet> m_OffscreenImGuiTextures;

public:

    std::unordered_map<uint64_t, Mesh*> m_EcsMeshCache;
    std::unordered_map<uint64_t, Material*> m_EcsMaterialCache;
    std::unordered_set<uint64_t> m_EcsWarningHandles;
    uint32_t m_EcsAssignedMeshCount = 0;
    uint32_t m_EcsFallbackMeshCount = 0;

    Mesh* m_DefaultMesh = nullptr;
    Material* m_DefaultMaterial = nullptr;
    uint32_t m_StaticRenderCount = 0;
    uint32_t m_EcsRenderCount = 0;
    uint32_t m_TotalRenderCount = 0;

    LightData getLastLightData() const { return m_LastLightData; }
    bool isFallbackLightingActive() const { return m_LastFallbackActive; }

    LightData m_LastLightData = {};
    bool m_LastFallbackActive = true;
};

} // namespace eng::renderer
