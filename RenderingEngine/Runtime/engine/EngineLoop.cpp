#include "Core/pch.h"
/*******************************************************************************************************************
 * @file  EngineLoop.cpp
 * @brief Implementation of the main engine loop coordinator.
 *******************************************************************************************************************/

#ifndef NOMINMAX
#define NOMINMAX
#endif
#include "Runtime/engine/EngineLoop.h"
#include "Vulkan/VulkanInstance.h"
#include "Vulkan/VulkanDevice.h"
#include "Vulkan/VulkanSwapChain.h"

#include "Rendering/Core/Renderer.h"
#include "ECS/ECSComponents.h"
#include "Core/Profiling/Profiler.h"
#include "Core/Vulkan/VkUtils.h"
#include "Core/Engine/Log.h"
#include "Core/Engine/VmaUsage.h"
#include "Core/World.h"
#include "Runtime/Resources/AssetCache.h"
#include "Runtime/Render_Scene/SceneBuilder.h"
#include "Runtime/Visibility/VisibilitySystem.h"
#include "Runtime/frame/FrameScheduler.h"
#include "Renderer/renderer.h"
#include <thread>
#include <algorithm>
#include <limits>

namespace eng::runtime {

    EngineLoop::EngineLoop()
    {
        ENG_LOG_INFO("Creating EngineLoop instance");
    }

    EngineLoop::~EngineLoop()
    {
        if (m_RHI) {
            m_RHI->WaitIdle();
        }
        
        if (m_Surface != VK_NULL_HANDLE && m_VulkanInstance) {
            vkDestroySurfaceKHR(m_VulkanInstance->GetHandle(), m_Surface, nullptr);
        }

        ENG_LOG_INFO("Destroying EngineLoop instance");
    }

    eng::core::Result EngineLoop::Initialize()
    {
        ENG_LOG_INFO("Initializing EngineLoop...");

        // Start timing
        m_LastFrameTime = std::chrono::steady_clock::now();

        // Initialize subsystems in dependency order
        ENG_RETURN_IF_FAILED(InitPlatform());
        ENG_RETURN_IF_FAILED(InitRHI());
        ENG_RETURN_IF_FAILED(InitRuntime());
        ENG_RETURN_IF_FAILED(InitRenderer());

        // Final setup
        m_Running.store(true, std::memory_order_relaxed);
        m_ExitRequested.store(false, std::memory_order_relaxed);

        ENG_LOG_INFO("EngineLoop initialized successfully");
        return eng::core::Result(); // Success
    }

    void EngineLoop::Run()
    {
        ENG_LOG_INFO("Starting main engine loop");

        // Initialize timer
        auto timerResult = m_Timer.Start();
        if (timerResult.IsFailure()) {
            ENG_LOG_ERROR("Failed to start timer");
            return;
        }

        while (IsRunning()) {
            Tick();
            
            // Check for exit request
            if (m_ExitRequested.load(std::memory_order_relaxed)) {
                break;
            }
        }

        ENG_LOG_INFO("Engine loop ended");
    }

    void EngineLoop::Tick()
    {
        if (m_Window->GetWidth() == 0 || m_Window->GetHeight() == 0) {
            ProcessInput();
            std::this_thread::sleep_for(std::chrono::milliseconds(16));
            return;
        }
        ENG_PROFILE_SCOPE("EngineFrame");
        double deltaTime = m_Timer.Tick();
        BeginFrame(deltaTime);
        Render();
        EndFrame();
    }

    void EngineLoop::BeginFrame(double deltaTime)
    {
        m_FrameStats.lastFrameTime = deltaTime;
        UpdateFrameStatistics(deltaTime);
        ProcessInput();
        WaitForFramePacing();

        if (m_FrameScheduler) {
            auto frameResult = m_FrameScheduler->BeginFrame(m_FrameContext, deltaTime);
            if (frameResult.IsFailure()) {
                ENG_LOG_ERROR("Failed to begin frame");
            }
        }
    }

    void EngineLoop::Render()
    {
        if (m_FrameScheduler) {
            if (m_RenderCallback) {
                m_RenderCallback(m_SharedResources, *m_FrameContext.world);
            } else {
                // Do not update world if already updated externally, but default pipeline updates here
                UpdateWorld(0.0f);
                BuildRenderScene();
                PerformCulling();
                BuildAndExecuteGraph();
            }
        }
    }

    void EngineLoop::EndFrame()
    {
        if (m_FrameScheduler) {
            auto frameResult = m_FrameScheduler->EndFrame(m_FrameContext);
            if (frameResult.IsFailure()) {
                ENG_LOG_ERROR("Failed to end frame");
            }
            ++m_FrameStats.frameCount;
        }
    }

    void EngineLoop::Shutdown()
    {
        ENG_LOG_INFO("Shutting down EngineLoop...");

        m_Running.store(false, std::memory_order_relaxed);

        if (m_RHI) {
            auto device = static_cast<eng::vulkan::VulkanDevice*>(m_RHI.get());
            VkDevice vkDevice = device->GetHandle();

            vkDeviceWaitIdle(vkDevice);
            
            if (m_SceneRenderer) {
                m_SceneRenderer->Shutdown();
                m_SceneRenderer.reset();
            }



            // Destroy sync objects cleanly via EngineResources
            m_SharedResources.destroySyncObjects();

            if (m_CommandPool) vkDestroyCommandPool(vkDevice, m_CommandPool, nullptr);

            if (m_SharedResources.allocator != VK_NULL_HANDLE) {
                if (m_SharedResources.pipelineLayout != VK_NULL_HANDLE) {
                    vkDestroyPipelineLayout(vkDevice, m_SharedResources.pipelineLayout, nullptr);
                    m_SharedResources.pipelineLayout = VK_NULL_HANDLE;
                }
                m_SharedResources.destroyMaterialDescriptorResources();
                m_SharedResources.destroyLightingDescriptorResources();
                m_SharedResources.destroyPerFrameResources();
                m_SharedResources.destroyCommandPools();
                vmaDestroyAllocator(m_SharedResources.allocator);
                m_SharedResources.allocator = VK_NULL_HANDLE;
            }

            for (auto framebuffer : m_Framebuffers) {
                vkDestroyFramebuffer(vkDevice, framebuffer, nullptr);
            }
            if (m_RenderPass) vkDestroyRenderPass(vkDevice, m_RenderPass, nullptr);

            m_SwapChain.reset();
        }

        // Shutdown in reverse order
        m_Renderer.reset();
        m_RHI.reset();
        m_FrameScheduler.reset();
        m_VisibilitySystem.reset();
        m_SceneBuilder.reset();
        m_AssetCache.reset();
        m_PrivateWorld.reset();
        m_World = nullptr;
        m_Window.reset();

        ENG_LOG_INFO("EngineLoop shutdown complete");
    }

    // ------------------------------------------------------------------------------------------------
    // Private Implementation
    // ------------------------------------------------------------------------------------------------

    eng::core::Result EngineLoop::InitPlatform()
    {
        ENG_LOG_INFO("Initializing platform...");

        // Create window
        auto windowResult = eng::platform::Window::Create(
            m_Config.windowTitle,
            m_Config.windowWidth,
            m_Config.windowHeight,
            m_Config.enableFullscreen ?
            eng::platform::WindowMode::Fullscreen :
            eng::platform::WindowMode::Windowed,
            m_Window
        );

        if (windowResult.IsFailure()) {
            ENG_LOG_ERROR("Failed to create window");
            return windowResult;
        }

        ENG_LOG_INFO("Platform initialized successfully");
        return eng::core::Result(); // Success
    }

    eng::core::Result EngineLoop::InitRHI()
    {
        ENG_LOG_INFO("Initializing RHI (Vulkan)...");

        // 1. Create Vulkan Instance
        m_VulkanInstance = std::make_unique<eng::vulkan::VulkanInstance>();
        auto result = m_VulkanInstance->Initialize(m_Config.windowTitle, true);
        if (result.IsFailure()) {
            ENG_LOG_ERROR("Failed to initialize Vulkan instance");
            return result;
        }

        // 2. Create Surface
        result = m_VulkanInstance->CreateSurface(m_Window->GetNativeHandle(), m_Surface);
        if (result.IsFailure()) {
            ENG_LOG_ERROR("Failed to create Vulkan surface");
            return result;
        }

        // 3. Create Vulkan Device
        auto device = std::make_unique<eng::vulkan::VulkanDevice>();
        result = device->Initialize(m_VulkanInstance->GetHandle(), m_Surface);
        if (result.IsFailure()) {
            ENG_LOG_ERROR("Failed to initialize Vulkan device");
            return result;
        }

        // Store device in m_RHI (cast to eng::rhi::Device)
        m_RHI = std::move(device);

        ENG_LOG_INFO("RHI (Vulkan) initialized successfully");
        return eng::core::Result(); // Success
    }

    eng::core::Result EngineLoop::InitRuntime()
    {
        ENG_LOG_INFO("Initializing runtime systems...");

    // -------------------------------------------------------------
    // 1️⃣ World handling – use an externally supplied world if present
    // -------------------------------------------------------------
    if (m_ExternalWorld) {
        m_World = m_ExternalWorld;
        ENG_LOG_INFO("Using externally supplied World (non-owning)");
    } else {
        m_PrivateWorld = std::make_unique<World>();
        m_World = m_PrivateWorld.get();
        ENG_LOG_INFO("Using private World (owned by EngineLoop)");
    }

    // -------------------------------------------------------------
    // 2️⃣ Remaining runtime objects – unchanged
    // -------------------------------------------------------------
    m_AssetCache       = std::make_unique<AssetCache>(m_RHI.get());
    m_SceneBuilder    = std::make_unique<SceneBuilder>(m_AssetCache.get());
    m_VisibilitySystem = std::make_unique<VisibilitySystem>();
    m_FrameScheduler   = std::make_unique<FrameScheduler>(m_RHI.get());
    m_FrameScheduler->SetTargetFPS(m_Config.targetFPS);

    // -------------------------------------------------------------
    // 3️⃣ Frame context – always points to the (now possibly external) world
    // -------------------------------------------------------------
    m_FrameContext.world = m_World;
    m_FrameContext.renderScene = nullptr; // filled later in Tick()
    m_FrameContext.visibleSet  = nullptr;
    m_FrameContext.resources   = nullptr;
    m_FrameContext.metrics     = nullptr;

    ENG_LOG_INFO("Runtime systems initialized successfully");
    return eng::core::Result(); // Success
    }

    eng::core::Result EngineLoop::InitRenderer()
    {
        ENG_LOG_INFO("Initializing renderer (Vulkan clear screen)...");

        auto device = static_cast<eng::vulkan::VulkanDevice*>(m_RHI.get());
        VkDevice vkDevice = device->GetHandle();

        // 1. Initialize SwapChain
        m_SwapChain = std::make_unique<eng::vulkan::VulkanSwapChain>();
        if (m_SwapChain->Initialize(device, m_Surface, m_Config.windowWidth, m_Config.windowHeight).IsFailure()) {
            ENG_LOG_ERROR("Failed to initialize SwapChain");
            return eng::core::Result(eng::core::ResultCode::Failure);
        }

        // 2. Create Render Pass
        VkAttachmentDescription colorAttachment{};
        colorAttachment.format = m_SwapChain->GetImageFormat();
        colorAttachment.samples = VK_SAMPLE_COUNT_1_BIT;
        colorAttachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
        colorAttachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
        colorAttachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
        colorAttachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
        colorAttachment.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
        colorAttachment.finalLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;

        VkAttachmentReference colorAttachmentRef{};
        colorAttachmentRef.attachment = 0;
        colorAttachmentRef.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

        VkSubpassDescription subpass{};
        subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
        subpass.colorAttachmentCount = 1;
        subpass.pColorAttachments = &colorAttachmentRef;

        VkSubpassDependency dependency{};
        dependency.srcSubpass = VK_SUBPASS_EXTERNAL;
        dependency.dstSubpass = 0;
        dependency.srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
        dependency.srcAccessMask = 0;
        dependency.dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
        dependency.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;

        VkRenderPassCreateInfo renderPassInfo{};
        renderPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
        renderPassInfo.attachmentCount = 1;
        renderPassInfo.pAttachments = &colorAttachment;
        renderPassInfo.subpassCount = 1;
        renderPassInfo.pSubpasses = &subpass;
        renderPassInfo.dependencyCount = 1;
        renderPassInfo.pDependencies = &dependency;

        if (vkCreateRenderPass(vkDevice, &renderPassInfo, nullptr, &m_RenderPass) != VK_SUCCESS) {
            ENG_LOG_ERROR("Failed to create Render Pass");
            return eng::core::Result(eng::core::ResultCode::Failure);
        }

        // 3. Create Framebuffers
        const auto& imageViews = m_SwapChain->GetImageViews();
        m_Framebuffers.resize(imageViews.size());
        for (size_t i = 0; i < imageViews.size(); i++) {
            VkImageView attachments[] = { imageViews[i] };
            VkFramebufferCreateInfo framebufferInfo{};
            framebufferInfo.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
            framebufferInfo.renderPass = m_RenderPass;
            framebufferInfo.attachmentCount = 1;
            framebufferInfo.pAttachments = attachments;
            framebufferInfo.width = m_SwapChain->GetExtent().width;
            framebufferInfo.height = m_SwapChain->GetExtent().height;
            framebufferInfo.layers = 1;

            if (vkCreateFramebuffer(vkDevice, &framebufferInfo, nullptr, &m_Framebuffers[i]) != VK_SUCCESS) {
                ENG_LOG_ERROR("Failed to create Framebuffer");
                return eng::core::Result(eng::core::ResultCode::Failure);
            }
        }

        // 4. Create Command Pool
        VkCommandPoolCreateInfo poolInfo{};
        poolInfo.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
        poolInfo.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
        poolInfo.queueFamilyIndex = device->GetGraphicsQueueFamily();

        if (vkCreateCommandPool(vkDevice, &poolInfo, nullptr, &m_CommandPool) != VK_SUCCESS) {
            ENG_LOG_ERROR("Failed to create Command Pool");
            return eng::core::Result(eng::core::ResultCode::Failure);
        }

        // 5. Allocate Command Buffers
        m_CommandBuffers.resize(m_MaxFramesInFlight);
        VkCommandBufferAllocateInfo allocInfo{};
        allocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
        allocInfo.commandPool = m_CommandPool;
        allocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
        allocInfo.commandBufferCount = (uint32_t)m_CommandBuffers.size();

        if (vkAllocateCommandBuffers(vkDevice, &allocInfo, m_CommandBuffers.data()) != VK_SUCCESS) {
            ENG_LOG_ERROR("Failed to allocate Command Buffers");
            return eng::core::Result(eng::core::ResultCode::Failure);
        }

        // 6. Create Sync Objects (Delegated to EngineResources)
        // We no longer resize or create them here.

        // Sync objects are handled by m_SharedResources.createSyncObjects() below

        // Populate shared resources for SceneRenderer
        m_SharedResources.instance = m_VulkanInstance->GetHandle();
        m_SharedResources.device = vkDevice;
        m_SharedResources.physicalDevice = device->GetPhysicalDevice();
        m_SharedResources.graphicsQueue = device->GetGraphicsQueue();
        m_SharedResources.presentQueue = device->GetPresentQueue();
        m_SharedResources.graphicsQueueFamily = device->GetGraphicsQueueFamily();
        m_SharedResources.swapChain = m_SwapChain->GetHandle();
        m_SharedResources.swapChainImageFormat = m_SwapChain->GetImageFormat();
        m_SharedResources.swapChainExtent = m_SwapChain->GetExtent();
        m_SharedResources.swapChainImageViews = m_SwapChain->GetImageViews();
        m_SharedResources.swapChainImages = m_SwapChain->GetImages();
        m_SharedResources.swapChainFramebuffers = m_Framebuffers;
        m_SharedResources.renderPass = m_RenderPass;
        m_SharedResources.imageAvailableSemaphores = m_ImageAvailableSemaphores;
        m_SharedResources.renderFinishedSemaphores = m_RenderFinishedSemaphores;
        m_SharedResources.inFlightFences = m_InFlightFences;

        // Initialize sync objects
        m_SharedResources.createSyncObjects();

        // Initialize VMA Allocator
        VmaAllocatorCreateInfo allocatorInfo{};
        allocatorInfo.vulkanApiVersion = VK_API_VERSION_1_0;
        allocatorInfo.physicalDevice = device->GetPhysicalDevice();
        allocatorInfo.device = vkDevice;
        allocatorInfo.instance = m_VulkanInstance->GetHandle();

        VK_CHECK(vmaCreateAllocator(&allocatorInfo, &m_SharedResources.allocator));
        
        m_SharedResources.createMaterialDescriptorResources();
        m_SharedResources.createLightingDescriptorResources();
        m_SharedResources.createPerFrameResources();
        m_SharedResources.createPipelineLayout();
        m_SharedResources.createCommandPools();
        m_SharedResources.createCommandBuffers();

        if (USE_SCENE_RENDERER) {
            m_SceneRenderer = std::make_unique<eng::renderer::Renderer>(m_SharedResources);
            if (m_ExternalWorld) {
                m_SceneRenderer->SetWorld(m_ExternalWorld);
            }
            m_SceneRenderer->Initialize();

            m_SceneRenderer->recreateSwapChainCallback = [this]() {
                this->RecreateSwapChain();
            };

            if (std::string(CUSTOM_MODEL_PATH) != "") {
                m_SceneRenderer->loadModel(CUSTOM_MODEL_PATH);
            }
        }

        ENG_LOG_INFO("Renderer initialized successfully");
        return eng::core::Result(); // Success
    }

    void EngineLoop::UpdateWorld(float deltaTime)
    {
        ENG_PROFILE_SCOPE("UpdateWorld");

        if (m_World) {
            m_World->Update(deltaTime);
        }
    }

    void EngineLoop::BuildRenderScene()
    {
        ENG_PROFILE_SCOPE("BuildRenderScene");

        if (m_SceneBuilder && m_FrameContext.world && m_FrameContext.resources) {
            // Get main camera (this would come from the world in practice)
            // Camera mainCamera = m_World->GetMainCamera();

            // For now, use a placeholder
            struct CameraPlaceholder {
                glm::vec3 position{ 0.0f, 0.0f, 0.0f };
            } mainCamera;

            auto result = m_SceneBuilder->Build(
                *m_FrameContext.world,
                mainCamera,
                m_FrameContext.resources->GetLinearAllocator(),
                *m_FrameContext.renderScene
            );

            if (result.IsFailure()) {
                ENG_LOG_WARN("Failed to build render scene");
            }
        }
    }

    void EngineLoop::PerformCulling()
    {
        ENG_PROFILE_SCOPE("PerformCulling");

        if (m_VisibilitySystem && m_FrameContext.renderScene && m_FrameContext.resources) {
            // Get camera (placeholder)
            struct CameraPlaceholder {
                glm::vec3 position{ 0.0f, 0.0f, 0.0f };
                glm::mat4 view{ 1.0f };
                glm::mat4 projection{ 1.0f };
            } mainCamera;

            auto result = m_VisibilitySystem->CullAndSort(
                *m_FrameContext.renderScene,
                mainCamera,
                m_FrameContext.resources->GetLinearAllocator(),
                *m_FrameContext.visibleSet
            );

            if (result.IsFailure()) {
                ENG_LOG_WARN("Failed to perform visibility culling");
            }
        }
    }

    void EngineLoop::BuildAndExecuteGraph()
    {
        ENG_PROFILE_SCOPE("BuildAndExecuteGraph");

        if (USE_SCENE_RENDERER && m_SceneRenderer) {
            m_SceneRenderer->BeginFrame();

            CameraComponent cameraComp;
            bool foundCamera = false;
            if (m_ExternalWorld) {
                auto& coordinator = m_ExternalWorld->getCoordinator();
                auto camType = coordinator.GetComponentType<CameraComponent>();
                for (std::uint32_t ent : coordinator.GetActiveEntities()) {
                    if (ent == 0 || !coordinator.IsEntityAlive(ent)) continue;
                    const auto& sig = coordinator.GetSignature(ent);
                    if (sig.test(camType)) {
                        const auto& cc = coordinator.GetComponent<CameraComponent>(ent);
                        if (cc.isPrimary) {
                            cameraComp = cc;
                            foundCamera = true;
                            break;
                        }
                    }
                }
            }
            if (!foundCamera && m_SceneRenderer) {
                cameraComp.fov = glm::degrees(m_SceneRenderer->camera.fovY);
                cameraComp.nearPlane = m_SceneRenderer->camera.nearPlane;
                cameraComp.farPlane = m_SceneRenderer->camera.farPlane;
            }

            m_SceneRenderer->RenderFrame(*m_ExternalWorld, cameraComp);
            m_SceneRenderer->EndFrame();
            
            m_CurrentFrame = (m_CurrentFrame + 1) % m_MaxFramesInFlight;
            return;
        }

        if (!m_SwapChain || m_RenderPass == VK_NULL_HANDLE) return;

        auto device = static_cast<eng::vulkan::VulkanDevice*>(m_RHI.get());
        VkDevice vkDevice = device->GetHandle();

        vkWaitForFences(vkDevice, 1, &m_InFlightFences[m_CurrentFrame], VK_TRUE, UINT64_MAX);

        uint32_t imageIndex;
        VkResult result = vkAcquireNextImageKHR(vkDevice, m_SwapChain->GetHandle(), UINT64_MAX, m_ImageAvailableSemaphores[m_CurrentFrame], VK_NULL_HANDLE, &imageIndex);
        if (result != VK_ERROR_OUT_OF_DATE_KHR) {
            VK_CHECK(result);
        }

        if (result == VK_ERROR_OUT_OF_DATE_KHR) {
            return; // Needs swapchain recreation, ignoring for simple clear screen
        }

        if (m_ImagesInFlight[imageIndex] != VK_NULL_HANDLE) {
            vkWaitForFences(vkDevice, 1, &m_ImagesInFlight[imageIndex], VK_TRUE, UINT64_MAX);
        }
        m_ImagesInFlight[imageIndex] = m_InFlightFences[m_CurrentFrame];

        vkResetFences(vkDevice, 1, &m_InFlightFences[m_CurrentFrame]);

        // Use pool reset from renderer
        // vkResetCommandPool(vkDevice, m_PyramidRenderer->commandPools[m_CurrentFrame], VK_COMMAND_POOL_RESET_RELEASE_RESOURCES_BIT);
        // Since we are using m_CommandBuffers from EngineLoop for now, let's keep it simple but add VK_CHECK where needed.
        vkResetCommandBuffer(m_CommandBuffers[m_CurrentFrame], 0);

        VkCommandBufferBeginInfo beginInfo{};
        beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;

        VK_CHECK(vkBeginCommandBuffer(m_CommandBuffers[m_CurrentFrame], &beginInfo));

        VkRenderPassBeginInfo renderPassInfo{};
        renderPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
        renderPassInfo.renderPass = m_RenderPass;
        renderPassInfo.framebuffer = m_Framebuffers[imageIndex];
        renderPassInfo.renderArea.offset = {0, 0};
        renderPassInfo.renderArea.extent = m_SwapChain->GetExtent();

        // Cornflower Blue
        VkClearValue clearColor = {{{0.39f, 0.58f, 0.93f, 1.0f}}};
        renderPassInfo.clearValueCount = 1;
        renderPassInfo.pClearValues = &clearColor;

        vkCmdBeginRenderPass(m_CommandBuffers[m_CurrentFrame], &renderPassInfo, VK_SUBPASS_CONTENTS_INLINE);
        
        // Legacy path: pyramid renderer removed

        vkCmdEndRenderPass(m_CommandBuffers[m_CurrentFrame]);

        VK_CHECK(vkEndCommandBuffer(m_CommandBuffers[m_CurrentFrame]));

        VkSubmitInfo submitInfo{};
        submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;

        VkSemaphore waitSemaphores[] = {m_ImageAvailableSemaphores[m_CurrentFrame]};
        VkPipelineStageFlags waitStages[] = {VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT};
        submitInfo.waitSemaphoreCount = 1;
        submitInfo.pWaitSemaphores = waitSemaphores;
        submitInfo.pWaitDstStageMask = waitStages;

        submitInfo.commandBufferCount = 1;
        submitInfo.pCommandBuffers = &m_CommandBuffers[m_CurrentFrame];

        VkSemaphore signalSemaphores[] = {m_RenderFinishedSemaphores[imageIndex]};
        submitInfo.signalSemaphoreCount = 1;
        submitInfo.pSignalSemaphores = signalSemaphores;

        VK_CHECK(vkQueueSubmit(device->GetGraphicsQueue(), 1, &submitInfo, m_InFlightFences[m_CurrentFrame]));

        VkPresentInfoKHR presentInfo{};
        presentInfo.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;

        presentInfo.waitSemaphoreCount = 1;
        presentInfo.pWaitSemaphores = signalSemaphores;

        VkSwapchainKHR swapChains[] = {m_SwapChain->GetHandle()};
        presentInfo.swapchainCount = 1;
        presentInfo.pSwapchains = swapChains;
        presentInfo.pImageIndices = &imageIndex;

        VK_CHECK(vkQueuePresentKHR(device->GetPresentQueue(), &presentInfo));

        m_CurrentFrame = (m_CurrentFrame + 1) % m_MaxFramesInFlight;
    }

    void EngineLoop::ProcessInput()
    {
        ENG_PROFILE_SCOPE("ProcessInput");

        if (m_Window) {
            // Poll window events
            auto result = m_Window->PollEvents();
            if (result.IsFailure()) {
                // Window requested close
                RequestExit();
            }

            // Process input events (keyboard, mouse, gamepad)
            // This would typically integrate with your input manager
            // eng::platform::GetInputManager().Update();
        }
    }

    void EngineLoop::WaitForFramePacing()
    {
        if (m_Config.targetFPS > 0) {
            double targetFrameTime = 1.0 / static_cast<double>(m_Config.targetFPS);
            if (m_FrameStats.lastFrameTime < targetFrameTime) {
                double sleepTime = targetFrameTime - m_FrameStats.lastFrameTime;
                std::this_thread::sleep_for(
                    std::chrono::duration<double>(sleepTime)
                );
            }
        }
    }

    void EngineLoop::UpdateFrameStatistics(double frameTime)
    {
        m_FrameStats.averageFrameTime = (m_FrameStats.averageFrameTime * 0.9) + (frameTime * 0.1);
        m_FrameStats.minFrameTime = (std::min)(m_FrameStats.minFrameTime, frameTime);
        m_FrameStats.maxFrameTime = (std::max)(m_FrameStats.maxFrameTime, frameTime);
        ++m_FrameStats.framesSinceLastReport;

        // Log statistics every 60 frames
        if (m_FrameStats.framesSinceLastReport >= 60) {
            double avgMS = m_FrameStats.averageFrameTime * 1000.0;
            double minMS = m_FrameStats.minFrameTime * 1000.0;
            double maxMS = m_FrameStats.maxFrameTime * 1000.0;
            double fps = 1.0 / m_FrameStats.averageFrameTime;

            ENG_LOG_DEBUG("Frame Stats: Avg {:.2f}ms ({:.1f} FPS), Min {:.2f}ms, Max {:.2f}ms",
                avgMS, fps, minMS, maxMS);

            // Reset min/max for next interval
            m_FrameStats.minFrameTime = 1000.0;
            m_FrameStats.maxFrameTime = 0.0;
            m_FrameStats.framesSinceLastReport = 0;
        }
    }

    void EngineLoop::SetAssetRegistry(eng::runtime::AssetRegistry* registry)
    {
        if (m_SceneRenderer) {
            m_SceneRenderer->SetAssetRegistry(registry);
        }
    }

    void EngineLoop::RecreateSwapChain()
    {
        auto device = static_cast<eng::vulkan::VulkanDevice*>(m_RHI.get());
        VkDevice vkDevice = device->GetHandle();

        vkDeviceWaitIdle(vkDevice);

        for (auto framebuffer : m_Framebuffers) {
            if (framebuffer != VK_NULL_HANDLE) {
                vkDestroyFramebuffer(vkDevice, framebuffer, nullptr);
            }
        }
        m_Framebuffers.clear();

        m_SwapChain->Shutdown();

        uint32_t width = m_Window->GetWidth();
        uint32_t height = m_Window->GetHeight();
        if (width == 0 || height == 0) {
            return;
        }

        if (m_SwapChain->Initialize(device, m_Surface, width, height).IsFailure()) {
            ENG_LOG_ERROR("Failed to initialize SwapChain during recreation");
            return;
        }

        const auto& imageViews = m_SwapChain->GetImageViews();
        m_Framebuffers.resize(imageViews.size());
        for (size_t i = 0; i < imageViews.size(); i++) {
            VkImageView attachments[] = { imageViews[i] };
            VkFramebufferCreateInfo framebufferInfo{};
            framebufferInfo.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
            framebufferInfo.renderPass = m_RenderPass;
            framebufferInfo.attachmentCount = 1;
            framebufferInfo.pAttachments = attachments;
            framebufferInfo.width = m_SwapChain->GetExtent().width;
            framebufferInfo.height = m_SwapChain->GetExtent().height;
            framebufferInfo.layers = 1;

            if (vkCreateFramebuffer(vkDevice, &framebufferInfo, nullptr, &m_Framebuffers[i]) != VK_SUCCESS) {
                ENG_LOG_ERROR("Failed to create Framebuffer during recreation");
            }
        }

        m_SharedResources.swapChain = m_SwapChain->GetHandle();
        m_SharedResources.swapChainImageFormat = m_SwapChain->GetImageFormat();
        m_SharedResources.swapChainExtent = m_SwapChain->GetExtent();
        m_SharedResources.swapChainImageViews = m_SwapChain->GetImageViews();
        m_SharedResources.swapChainImages = m_SwapChain->GetImages();
        m_SharedResources.swapChainFramebuffers = m_Framebuffers;

        m_SharedResources.recreateSwapChain();

        if (m_SceneRenderer) {
            m_SceneRenderer->setupRenderGraph();
        }
        
        ENG_LOG_INFO("Vulkan Swapchain successfully recreated. New Extent: {}x{}", m_SharedResources.swapChainExtent.width, m_SharedResources.swapChainExtent.height);
    }

} // namespace eng::runtime
