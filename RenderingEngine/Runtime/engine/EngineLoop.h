/*******************************************************************************************************************
 * @file  EngineLoop.h
 * @brief Main engine loop coordinator implementing IRenderer interface.
 *******************************************************************************************************************/

#pragma once

#include "RenderingEngine/Public/IRenderer.h"
#include "RenderingEngine/Public/RendererTypes.h"
#include "core/types/Result.h"
#include "core/log/Log.h"
#include "platform/platform.h"
#include "Core/Engine/EngineResources.h" // Needed for inline callback and member structure

#include <memory>
#include <atomic>
#include <chrono>
#include <functional>
#include <vector>
#include <string>
#include "runtime/frame/FrameContext.h"

#define USE_SCENE_RENDERER 1
#define CUSTOM_MODEL_PATH ""

#ifdef _WIN32
#define VK_USE_PLATFORM_WIN32_KHR
#endif
#include <vulkan/vulkan.h>

// Forward declarations for namespaces
namespace eng::platform {
    class Window;
}

namespace eng::vulkan {
    class VulkanInstance;
    class VulkanSwapChain;
    class VulkanDevice;
}

namespace eng::rhi {
    class Device;
}

namespace eng::renderer {
    class Renderer;
    class PyramidRenderer;
    class SceneRenderer;
}

namespace eng::runtime {
    class World;
    class SceneBuilder;
    class VisibilitySystem;
    class FrameScheduler;
    class AssetCache;
    class AssetRegistry;

    class EngineLoop : public eng::renderer::IRenderer {
    public:
        EngineLoop();
        ~EngineLoop() override;

        [[nodiscard]] eng::core::Result Initialize() override;
        void Run();
        void Tick() override;
        void Shutdown() override;

        void BeginFrame(double deltaTime) override;
        void Render() override;
        void EndFrame() override;
        void RequestExit();
        bool IsRunning() const;

        void SetExternalWorld(eng::runtime::World* externalWorld) noexcept override {
            m_ExternalWorld = externalWorld;
        }

        void SetAssetRegistry(eng::runtime::AssetRegistry* registry);
        void RecreateSwapChain();

        // Accessors for Editor Layer
        eng::platform::Window* GetWindow() const { return m_Window.get(); }
        eng::vulkan::VulkanInstance* GetVulkanInstance() const { return m_VulkanInstance.get(); }
        eng::rhi::Device* GetRHI() const { return m_RHI.get(); }
        eng::vulkan::VulkanSwapChain* GetSwapChain() const { return m_SwapChain.get(); }
        eng::renderer::EngineResources& GetSharedResources() { return const_cast<eng::renderer::EngineResources&>(m_SharedResources); }
        VkRenderPass GetRenderPass() const { return m_RenderPass; }
        uint32_t GetMaxFramesInFlight() const { return m_MaxFramesInFlight; }
        eng::renderer::SceneRenderer* GetSceneRenderer() const { return m_SceneRenderer.get(); }

        // Register a per-frame render callback.
        using RenderCallback = std::function<void(const eng::renderer::EngineResources&,
                                                const eng::runtime::World&)>;
        void RegisterRenderCallback(RenderCallback cb) noexcept {
            m_RenderCallback = std::move(cb);
        }

    private:
        [[nodiscard]] eng::core::Result InitPlatform();
        [[nodiscard]] eng::core::Result InitRHI();
        [[nodiscard]] eng::core::Result InitRuntime();
        [[nodiscard]] eng::core::Result InitRenderer();

        void UpdateWorld(float deltaTime);
        void BuildRenderScene();
        void PerformCulling();
        void BuildAndExecuteGraph();
        void ProcessInput();

        void WaitForFramePacing();
        void UpdateFrameStatistics(double frameTime);

        std::unique_ptr<eng::platform::Window> m_Window;
        eng::platform::Timer m_Timer;

        World* m_World = nullptr;
        std::unique_ptr<World> m_PrivateWorld;
        std::unique_ptr<SceneBuilder> m_SceneBuilder;
        std::unique_ptr<VisibilitySystem> m_VisibilitySystem;
        std::unique_ptr<FrameScheduler> m_FrameScheduler;
        std::unique_ptr<AssetCache> m_AssetCache;

        std::unique_ptr<eng::vulkan::VulkanInstance> m_VulkanInstance;
        VkSurfaceKHR m_Surface = VK_NULL_HANDLE;
        std::unique_ptr<eng::rhi::Device> m_RHI;
        std::unique_ptr<eng::vulkan::VulkanSwapChain> m_SwapChain;
        std::unique_ptr<eng::renderer::Renderer> m_Renderer;
        std::unique_ptr<eng::renderer::PyramidRenderer> m_PyramidRenderer;
        std::unique_ptr<eng::renderer::SceneRenderer> m_SceneRenderer;
        eng::renderer::EngineResources m_SharedResources;

        VkRenderPass m_RenderPass = VK_NULL_HANDLE;
        std::vector<VkFramebuffer> m_Framebuffers;
        VkCommandPool m_CommandPool = VK_NULL_HANDLE;
        std::vector<VkCommandBuffer> m_CommandBuffers;
        std::vector<VkSemaphore> m_ImageAvailableSemaphores;
        std::vector<VkSemaphore> m_RenderFinishedSemaphores;
        std::vector<VkFence> m_InFlightFences;
        std::vector<VkFence> m_ImagesInFlight;
        uint32_t m_CurrentFrame = 0;
        uint32_t m_MaxFramesInFlight = 3;
        ::eng::runtime::FrameContext m_FrameContext;

        std::atomic<bool> m_Running{ false };
        std::atomic<bool> m_ExitRequested{ false };

        struct Config {
            std::string windowTitle = "Engine Application";
            uint32_t windowWidth = 1920;
            uint32_t windowHeight = 1080;
            bool enableVSync = true;
            bool enableFullscreen = false;
            uint32_t targetFPS = 0;
        } m_Config;

        struct FrameStats {
            uint64_t frameCount = 0;
            double lastFrameTime = 0.0;
            double averageFrameTime = 0.0;
            double minFrameTime = 1000.0;
            double maxFrameTime = 0.0;
            uint32_t framesSinceLastReport = 0;
        } m_FrameStats;

        std::chrono::steady_clock::time_point m_LastFrameTime;

        // USER-HOOK STATE
        eng::runtime::World* m_ExternalWorld = nullptr; // injected by Application
        RenderCallback m_RenderCallback;                // optional per-frame user renderer
    };

    inline void EngineLoop::RequestExit() { m_ExitRequested.store(true, std::memory_order_relaxed); }
    inline bool EngineLoop::IsRunning() const { return m_Running.load(std::memory_order_relaxed) && !m_ExitRequested.load(std::memory_order_relaxed); }

} // namespace eng::runtime
