#include "Runtime/Public/Editor/EditorLayer.h"
#include "Core/Logging/Logger.h"
#include "RenderingEngine/Runtime/engine/EngineLoop.h"
#include "RenderingEngine/Core/Engine/EngineResources.h"
#include "RenderingEngine/Platform/window/Window.h"
#include "ECS/Coordinator.h"
#include "ECS/ECSComponents.h"
#include "ECS/Public/IECSWorld.h"
#include "RenderingEngine/Vulkan/VulkanSwapChain.h"
#include "Runtime/Public/AssetRegistry.h"
#include "Physics/Public/PhysicsDebugDraw.h"
#include "Physics/Public/PhysicsWorld.h"
#include "RenderingEngine/Renderer/SceneRenderer.h"
#include "RenderingEngine/Renderer/scene/Camera.h"

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#endif

#include "ThirdParty/imgui/imgui.h"
#include "ThirdParty/imgui/imgui_internal.h"
#include "ThirdParty/imgui/backends/imgui_impl_win32.h"
#include "ThirdParty/imgui/backends/imgui_impl_vulkan.h"

#include <algorithm>
#include <string>
#include <cstring>
#include <cmath>
#include <filesystem>
#include "Runtime/Public/OmnixMaterialFormat.h"

#include "Scene/SceneManager.h"
#include "Scene/Scene.h"
#include "Scene/SceneObject.h"

namespace {
    struct DiagnosticsPhysicsState {
        bool lastRaycastHit = false;
        bool lastRaycastPerformed = false;
        Entity lastRaycastEntity = 0;
        int lastOverlapCount = 0;
        bool lastOverlapPerformed = false;
    };
    static DiagnosticsPhysicsState g_DiagPhysState;
}

namespace eng::runtime {

    EditorLayer::EditorLayer() {}
    
    EditorLayer::~EditorLayer() {
        Shutdown();
    }

    bool EditorLayer::Initialize(RuntimeContext* context) {
        m_Context = context;
        if (!m_Context) {
            CORE_LOG_ERROR("[EditorLayer] Context is null!");
            return false;
        }

        auto* engineLoop = dynamic_cast<eng::runtime::EngineLoop*>(m_Context->renderer);
        if (!engineLoop) {
            CORE_LOG_ERROR("[EditorLayer] Renderer is not an instance of EngineLoop!");
            return false;
        }

        VkDevice device = engineLoop->GetSharedResources().device;
        VkPhysicalDevice physicalDevice = engineLoop->GetSharedResources().physicalDevice;

        // Initialize panel subsystems
        m_HierarchyPanel.Initialize(m_Context);
        m_InspectorPanel.Initialize(m_Context);
        m_ConsolePanel.Initialize(m_Context);
        m_ViewportPanel.Initialize(m_Context);
        m_AssetBrowserPanel.Initialize(m_Context);

        // 1. Initialize Dear ImGui Context
        IMGUI_CHECKVERSION();
        ImGui::CreateContext();
        ImGuiIO& io = ImGui::GetIO(); (void)io;
        
        std::filesystem::create_directories("Config/Editor");
        io.IniFilename = "Config/Editor/imgui.ini";

        io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;     // Enable Keyboard Controls
        io.ConfigFlags |= ImGuiConfigFlags_DockingEnable;           // Enable Docking
        io.ConfigFlags &= ~ImGuiConfigFlags_ViewportsEnable;        // Disable Multi-Viewports to prevent Vulkan surface errors

        // Premium Dark Theme Setup
        ImGui::StyleColorsDark();

        // Style tweaks for a premium look
        ImGuiStyle& style = ImGui::GetStyle();
        style.WindowRounding = 5.0f;
        style.FrameRounding = 4.0f;
        style.GrabRounding = 4.0f;
        style.PopupRounding = 4.0f;
        style.Colors[ImGuiCol_WindowBg] = ImVec4(0.09f, 0.09f, 0.10f, 1.00f);
        style.Colors[ImGuiCol_Header] = ImVec4(0.20f, 0.20f, 0.25f, 1.00f);
        style.Colors[ImGuiCol_HeaderHovered] = ImVec4(0.26f, 0.26f, 0.35f, 1.00f);
        style.Colors[ImGuiCol_HeaderActive] = ImVec4(0.30f, 0.30f, 0.40f, 1.00f);
        style.Colors[ImGuiCol_Button] = ImVec4(0.18f, 0.18f, 0.22f, 1.00f);
        style.Colors[ImGuiCol_ButtonHovered] = ImVec4(0.25f, 0.25f, 0.30f, 1.00f);
        style.Colors[ImGuiCol_ButtonActive] = ImVec4(0.30f, 0.30f, 0.40f, 1.00f);

        // 2. Create dedicated descriptor pool for ImGui
        VkDescriptorPoolSize pool_sizes[] =
        {
            { VK_DESCRIPTOR_TYPE_SAMPLER, 1000 },
            { VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1000 },
            { VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, 1000 },
            { VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, 1000 },
            { VK_DESCRIPTOR_TYPE_UNIFORM_TEXEL_BUFFER, 1000 },
            { VK_DESCRIPTOR_TYPE_STORAGE_TEXEL_BUFFER, 1000 },
            { VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1000 },
            { VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1000 },
            { VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC, 1000 },
            { VK_DESCRIPTOR_TYPE_STORAGE_BUFFER_DYNAMIC, 1000 },
            { VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT, 1000 }
        };
        VkDescriptorPoolCreateInfo pool_info = {};
        pool_info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
        pool_info.flags = VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT;
        pool_info.maxSets = 1000 * IM_ARRAYSIZE(pool_sizes);
        pool_info.poolSizeCount = (uint32_t)IM_ARRAYSIZE(pool_sizes);
        pool_info.pPoolSizes = pool_sizes;
        if (vkCreateDescriptorPool(device, &pool_info, nullptr, &m_ImGuiDescriptorPool) != VK_SUCCESS) {
            CORE_LOG_ERROR("[EditorLayer] Failed to create ImGui descriptor pool!");
            return false;
        }

        // 3. Create custom UIPass render pass
        VkAttachmentDescription attachment{};
        attachment.format = engineLoop->GetSharedResources().swapChainImageFormat;
        attachment.samples = VK_SAMPLE_COUNT_1_BIT;
        attachment.loadOp = VK_ATTACHMENT_LOAD_OP_LOAD; // Load existing rendering output
        attachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
        attachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
        attachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
        attachment.initialLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;
        attachment.finalLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;

        VkAttachmentReference color_attachment{};
        color_attachment.attachment = 0;
        color_attachment.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

        VkSubpassDescription subpass{};
        subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
        subpass.colorAttachmentCount = 1;
        subpass.pColorAttachments = &color_attachment;

        VkSubpassDependency dependency{};
        dependency.srcSubpass = VK_SUBPASS_EXTERNAL;
        dependency.dstSubpass = 0;
        dependency.srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
        dependency.dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
        dependency.srcAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
        dependency.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;

        VkRenderPassCreateInfo rp_info{};
        rp_info.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
        rp_info.attachmentCount = 1;
        rp_info.pAttachments = &attachment;
        rp_info.subpassCount = 1;
        rp_info.pSubpasses = &subpass;
        rp_info.dependencyCount = 1;
        rp_info.pDependencies = &dependency;

        if (vkCreateRenderPass(device, &rp_info, nullptr, &m_UIRenderPass) != VK_SUCCESS) {
            CORE_LOG_ERROR("[EditorLayer] Failed to create UI Render Pass!");
            return false;
        }

        // 4. Initialize Platform Backends
        HWND hwnd = static_cast<HWND>(engineLoop->GetWindow()->GetNativeHandle());
        if (!ImGui_ImplWin32_Init(hwnd)) {
            CORE_LOG_ERROR("[EditorLayer] Failed to initialize ImGui Win32 backend!");
            return false;
        }

        // Disable PlatformHasViewports to prevent ImGui Vulkan backend from asserting on Platform_CreateVkSurface
        ImGui::GetIO().BackendFlags &= ~ImGuiBackendFlags_PlatformHasViewports;

        ImGui_ImplVulkan_InitInfo init_info = {};
        init_info.Instance = engineLoop->GetSharedResources().instance;
        init_info.PhysicalDevice = physicalDevice;
        init_info.Device = device;
        init_info.QueueFamily = engineLoop->GetSharedResources().graphicsQueueFamily;
        init_info.Queue = engineLoop->GetSharedResources().graphicsQueue;
        init_info.DescriptorPool = m_ImGuiDescriptorPool;
        
        // Robustly determine swapchain image count
        uint32_t imageCount = 0;
        if (engineLoop->GetSwapChain()) {
            imageCount = static_cast<uint32_t>(engineLoop->GetSwapChain()->GetImages().size());
        }
        if (imageCount == 0) {
            imageCount = static_cast<uint32_t>(engineLoop->GetSharedResources().swapChainImages.size());
        }
        if (imageCount == 0) {
            imageCount = 3; // Fallback default (triple-buffering is common)
        }

        init_info.MinImageCount = 2; // ImGui Vulkan requires MinImageCount >= 2
        init_info.ImageCount = std::max(init_info.MinImageCount, imageCount);

        init_info.PipelineInfoMain.RenderPass = m_UIRenderPass;
        init_info.PipelineInfoMain.Subpass = 0;
        init_info.PipelineInfoMain.MSAASamples = VK_SAMPLE_COUNT_1_BIT;

        CORE_LOG_INFO("[EditorLayer] ImGui Vulkan Init: ImageCount = %u, MinImageCount = %u, swapChainImages size = %u", 
                      init_info.ImageCount, init_info.MinImageCount, imageCount);

        if (!ImGui_ImplVulkan_Init(&init_info)) {
            CORE_LOG_ERROR("[EditorLayer] Failed to initialize ImGui Vulkan backend!");
            return false;
        }

        // Set up offscreen rendering for the viewport
        auto* sceneRenderer = engineLoop->GetSceneRenderer();
        if (sceneRenderer) {
            sceneRenderer->SetOffscreenRenderingEnabled(true);
        }

        // 5. Connect the UIPass callback
        engineLoop->GetSharedResources().uiCallback = [this](VkCommandBuffer cmd, uint32_t imageIndex) {
            this->RenderUI(cmd, imageIndex);
        };

        CORE_LOG_INFO("[EditorLayer] Initialized successfully");
        return true;
    }

    void EditorLayer::BeginFrame() {
        if (m_Context && m_Context->ecs) {
            m_Selection.Validate(m_Context->ecs->getCoordinator());
        }

        ImGui_ImplVulkan_NewFrame();
        ImGui_ImplWin32_NewFrame();
        ImGui::NewFrame();
    }

    void EditorLayer::Render() {
        // 1. Check if viewport panel size has changed, and trigger recreation if so
        auto* engineLoop = m_Context ? dynamic_cast<eng::runtime::EngineLoop*>(m_Context->renderer) : nullptr;
        if (engineLoop && engineLoop->GetSceneRenderer()) {
            auto* sceneRenderer = engineLoop->GetSceneRenderer();
            uint32_t currentWidth = sceneRenderer->GetOffscreenWidth();
            uint32_t currentHeight = sceneRenderer->GetOffscreenHeight();

            uint32_t targetWidth = static_cast<uint32_t>(m_LastViewportWidth);
            uint32_t targetHeight = static_cast<uint32_t>(m_LastViewportHeight);

            if (targetWidth > 0 && targetHeight > 0 && (targetWidth != currentWidth || targetHeight != currentHeight)) {
                VkDevice device = engineLoop->GetSharedResources().device;
                vkDeviceWaitIdle(device);

                // Reset all command pools to release command buffer references to old descriptor sets before destroying them
                for (uint32_t i = 0; i < engineLoop->GetSharedResources().MAX_FRAMES_IN_FLIGHT; ++i) {
                    vkResetCommandPool(device,
                                       engineLoop->GetSharedResources().commandPools.at(i),
                                       VK_COMMAND_POOL_RESET_RELEASE_RESOURCES_BIT);
                }

                sceneRenderer->CreateOffscreenResources(targetWidth, targetHeight);
                CORE_LOG_INFO("[EditorLayer] Recreated viewport offscreen targets: {}x{}", targetWidth, targetHeight);
            }
        }

        // Update the Editor Camera
        float dt = ImGui::GetIO().DeltaTime;
        m_EditorCamera.Update(dt, m_ViewportPanel.IsHovered(), m_ViewportPanel.IsFocused());

        // Handle focusing on selected entity with 'F' key
        if ((m_ViewportPanel.IsHovered() || m_ViewportPanel.IsFocused()) && ImGui::IsKeyPressed(ImGuiKey_F)) {
            Entity selectedEntity = m_Selection.GetSelectedEntity();
            if (selectedEntity != 0 && m_Context && m_Context->ecs && m_Context->ecs->getCoordinator().IsEntityAlive(selectedEntity)) {
                auto& coordinator = m_Context->ecs->getCoordinator();
                auto transformType = coordinator.GetComponentType<TransformComponent>();
                if (coordinator.GetSignature(selectedEntity).test(transformType)) {
                    const auto& transform = coordinator.GetComponent<TransformComponent>(selectedEntity);
                    glm::vec3 entityPos(transform.position.x, transform.position.y, transform.position.z);
                    
                    // Focus framing: move camera to look at the entity from 5 units back
                    m_EditorCamera.position = entityPos - m_EditorCamera.getForward() * 5.0f;
                    m_EditorCamera.LookAt(entityPos);
                }
            }
        }

        // If in Edit Mode, copy EditorCamera view parameters to SceneRenderer camera
        if (m_SimulationState == EditorSimulationState::Edit && m_Context && m_Context->renderer) {
            auto* engineLoop = dynamic_cast<eng::runtime::EngineLoop*>(m_Context->renderer);
            if (engineLoop && engineLoop->GetSceneRenderer()) {
                auto* sceneRenderer = engineLoop->GetSceneRenderer();
                sceneRenderer->camera.position = m_EditorCamera.position;
                sceneRenderer->camera.target = m_EditorCamera.position + m_EditorCamera.getForward();
                sceneRenderer->camera.up = m_EditorCamera.getUp();
                sceneRenderer->camera.fovY = m_EditorCamera.fovY;
                sceneRenderer->camera.nearPlane = m_EditorCamera.nearPlane;
                sceneRenderer->camera.farPlane = m_EditorCamera.farPlane;
            }
        }

        RenderDockspace();

        // Sync colliders toggle state from the ViewportPanel toolbar checkbox
        m_ShowColliders = m_ViewportPanel.ShowCollidersEnabled();

        if (m_ShowColliders && m_Context && m_Context->ecs) {
            auto* engineLoop = dynamic_cast<eng::runtime::EngineLoop*>(m_Context->renderer);
            if (engineLoop) {
                auto* sceneRenderer = engineLoop->GetSceneRenderer();
                if (sceneRenderer) {
                    auto& camera = sceneRenderer->getCamera();
                    glm::mat4 view = camera.getViewMatrix();
                    VkExtent2D extent = engineLoop->GetSharedResources().swapChainExtent;
                    if (extent.width > 0 && extent.height > 0) {
                        float aspectRatio = (float)extent.width / (float)extent.height;
                        glm::mat4 proj = camera.getProjMatrix(aspectRatio);
                        eng::physics::PhysicsDebugDraw::Render(
                            m_Context->ecs->getCoordinator(),
                            view,
                            proj,
                            (float)extent.width,
                            (float)extent.height
                        );
                    }
                }
            }
        }

        ImGui::Render();
    }

    void EditorLayer::EndFrame() {
        // Multi-viewports can be updated here if enabled later.
    }

    void EditorLayer::Shutdown() {
        if (!m_Context) return;

        auto* engineLoop = dynamic_cast<eng::runtime::EngineLoop*>(m_Context->renderer);
        if (engineLoop) {
            engineLoop->GetSharedResources().uiCallback = nullptr;
            VkDevice device = engineLoop->GetSharedResources().device;
            
            vkDeviceWaitIdle(device);

            ImGui_ImplVulkan_Shutdown();
            ImGui_ImplWin32_Shutdown();
            ImGui::DestroyContext();

            CleanupFramebuffers();

            if (m_UIRenderPass != VK_NULL_HANDLE) {
                vkDestroyRenderPass(device, m_UIRenderPass, nullptr);
                m_UIRenderPass = VK_NULL_HANDLE;
            }

            if (m_ImGuiDescriptorPool != VK_NULL_HANDLE) {
                vkDestroyDescriptorPool(device, m_ImGuiDescriptorPool, nullptr);
                m_ImGuiDescriptorPool = VK_NULL_HANDLE;
            }
        }
        
        m_Context = nullptr;
        CORE_LOG_INFO("[EditorLayer] Shutdown complete");
    }

    void EditorLayer::RenderUI(VkCommandBuffer cmd, uint32_t imageIndex) {
        if (!m_Context || !m_Context->renderer) return;

        auto* engineLoop = dynamic_cast<eng::runtime::EngineLoop*>(m_Context->renderer);
        if (!engineLoop) return;

        VkExtent2D currentExtent = engineLoop->GetSharedResources().swapChainExtent;
        if (m_UIFramebuffers.empty() || currentExtent.width != m_CurrentExtent.width || currentExtent.height != m_CurrentExtent.height) {
            RecreateFramebuffers(currentExtent);
        }

        if (m_UIFramebuffers.empty() || imageIndex >= m_UIFramebuffers.size()) {
            return;
        }

        VkRenderPassBeginInfo info{};
        info.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
        info.renderPass = m_UIRenderPass;
        info.framebuffer = m_UIFramebuffers[imageIndex];
        info.renderArea.extent = m_CurrentExtent;
        info.renderArea.offset = {0, 0};
        
        info.clearValueCount = 0;
        info.pClearValues = nullptr;

        vkCmdBeginRenderPass(cmd, &info, VK_SUBPASS_CONTENTS_INLINE);

        ImDrawData* drawData = ImGui::GetDrawData();
        if (drawData) {
            ImGui_ImplVulkan_RenderDrawData(drawData, cmd);
        }

        vkCmdEndRenderPass(cmd);
    }

    void EditorLayer::RecreateFramebuffers(VkExtent2D extent) {
        auto* engineLoop = dynamic_cast<eng::runtime::EngineLoop*>(m_Context->renderer);
        if (!engineLoop) return;

        VkDevice device = engineLoop->GetSharedResources().device;
        const auto& imageViews = engineLoop->GetSharedResources().swapChainImageViews;

        CleanupFramebuffers();

        m_CurrentExtent = extent;
        m_UIFramebuffers.resize(imageViews.size());

        for (size_t i = 0; i < imageViews.size(); ++i) {
            VkImageView attachments[] = { imageViews[i] };
            VkFramebufferCreateInfo framebufferInfo{};
            framebufferInfo.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
            framebufferInfo.renderPass = m_UIRenderPass;
            framebufferInfo.attachmentCount = 1;
            framebufferInfo.pAttachments = attachments;
            framebufferInfo.width = extent.width;
            framebufferInfo.height = extent.height;
            framebufferInfo.layers = 1;

            if (vkCreateFramebuffer(device, &framebufferInfo, nullptr, &m_UIFramebuffers[i]) != VK_SUCCESS) {
                CORE_LOG_ERROR("[EditorLayer] Failed to create UI Framebuffer!");
            }
        }
    }

    void EditorLayer::CleanupFramebuffers() {
        if (m_Context && m_Context->renderer) {
            auto* engineLoop = dynamic_cast<eng::runtime::EngineLoop*>(m_Context->renderer);
            if (engineLoop) {
                VkDevice device = engineLoop->GetSharedResources().device;
                for (auto framebuffer : m_UIFramebuffers) {
                    if (framebuffer != VK_NULL_HANDLE) {
                        vkDestroyFramebuffer(device, framebuffer, nullptr);
                    }
                }
            }
        }
        m_UIFramebuffers.clear();
    }

    void EditorLayer::RenderDockspace() {
        static bool opt_fullscreen = true;
        static bool opt_padding = false;
        static ImGuiDockNodeFlags dockspace_flags = ImGuiDockNodeFlags_None;

        ImGuiWindowFlags window_flags = ImGuiWindowFlags_MenuBar | ImGuiWindowFlags_NoDocking;
        if (opt_fullscreen)
        {
            const ImGuiViewport* viewport = ImGui::GetMainViewport();
            ImGui::SetNextWindowPos(viewport->WorkPos);
            ImGui::SetNextWindowSize(viewport->WorkSize);
            ImGui::SetNextWindowViewport(viewport->ID);
            ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 0.0f);
            ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.0f);
            window_flags |= ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove;
            window_flags |= ImGuiWindowFlags_NoBringToFrontOnFocus | ImGuiWindowFlags_NoNavFocus;
        }

        if (!opt_padding)
            ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0.0f, 0.0f));
        
        static bool open = true;

        auto* sceneMgr = dynamic_cast<SceneManager*>(m_Context->scenes);
        static Scene* lastActiveScene = nullptr;
        static bool firstFrame = true;
        if (firstFrame) {
            if (sceneMgr) {
                lastActiveScene = sceneMgr->GetActiveScene();
            }
            firstFrame = false;
        }
        if (sceneMgr) {
            Scene* currentActiveScene = sceneMgr->GetActiveScene();
            if (currentActiveScene != lastActiveScene) {
                lastActiveScene = currentActiveScene;
                m_Selection.Clear();
                if (m_RestoreDirtyStateAfterLoad) {
                    if (m_EditDirtyBeforePlay) {
                        m_DirtyState.MarkSceneDirty();
                    } else {
                        m_DirtyState.ClearSceneDirty();
                    }
                    m_RestoreDirtyStateAfterLoad = false;
                } else {
                    m_DirtyState.ClearSceneDirty();
                }
            }
        }

        std::string sceneName = "UntitledScene";
        std::string scenePath = "";
        if (sceneMgr && sceneMgr->GetActiveScene()) {
            sceneName = sceneMgr->GetActiveScene()->GetName();
            scenePath = sceneMgr->GetActiveScene()->GetFilePath();
        }

        std::string workspaceTitle = "Omnix Editor Workspace - " + sceneName;
        if (!scenePath.empty()) {
            workspaceTitle += " (" + scenePath + ")";
        }
        if (m_DirtyState.IsSceneDirty()) {
            workspaceTitle += " *";
        }
        workspaceTitle += "###OmnixEditorWorkspace";

        ImGui::Begin(workspaceTitle.c_str(), &open, window_flags);

        if (!opt_padding)
            ImGui::PopStyleVar();

        if (opt_fullscreen)
            ImGui::PopStyleVar(2);

        // Update the Win32 native window title as well
        if (m_Context && m_Context->renderer) {
            auto* engineLoop = dynamic_cast<eng::runtime::EngineLoop*>(m_Context->renderer);
            if (engineLoop && engineLoop->GetWindow()) {
                HWND hwnd = static_cast<HWND>(engineLoop->GetWindow()->GetNativeHandle());
                if (hwnd) {
                    std::string windowTitle = "Omnix Engine (Editor) - " + sceneName;
                    if (!scenePath.empty()) {
                        windowTitle += " (" + scenePath + ")";
                    }
                    if (m_DirtyState.IsSceneDirty()) {
                        windowTitle += " *";
                    }
                    SetWindowTextA(hwnd, windowTitle.c_str());
                }
            }
        }

        // Submit the DockSpace
        ImGuiIO& io = ImGui::GetIO();
        if (io.ConfigFlags & ImGuiConfigFlags_DockingEnable)
        {
            ImGuiID dockspace_id = ImGui::GetID("EditorDockSpace");
            ImGui::DockSpace(dockspace_id, ImVec2(0.0f, 0.0f), dockspace_flags);

            // Establish a default clean docking layout if not already configured
            if (m_ResetLayout || ImGui::DockBuilderGetNode(dockspace_id) == NULL)
            {
                m_ResetLayout = false;
                ImGui::DockBuilderRemoveNode(dockspace_id); // Clear out previous configuration
                ImGui::DockBuilderAddNode(dockspace_id, ImGuiDockNodeFlags_DockSpace);
                ImGui::DockBuilderSetNodeSize(dockspace_id, ImGui::GetMainViewport()->WorkSize);

                ImGuiID dock_main_id = dockspace_id;
                ImGuiID dock_id_left = ImGui::DockBuilderSplitNode(dock_main_id, ImGuiDir_Left, 0.20f, NULL, &dock_main_id);
                ImGuiID dock_id_right = ImGui::DockBuilderSplitNode(dock_main_id, ImGuiDir_Right, 0.25f, NULL, &dock_main_id);
                ImGuiID dock_id_bottom = ImGui::DockBuilderSplitNode(dock_main_id, ImGuiDir_Down, 0.25f, NULL, &dock_main_id);

                ImGui::DockBuilderDockWindow("Scene Hierarchy", dock_id_left);
                ImGui::DockBuilderDockWindow("Inspector", dock_id_right);
                ImGui::DockBuilderDockWindow("Asset Browser", dock_id_bottom);
                ImGui::DockBuilderDockWindow("Console", dock_id_bottom);
                ImGui::DockBuilderDockWindow("Play Mode Diagnostics", dock_id_bottom);
                ImGui::DockBuilderDockWindow("Viewport", dock_main_id);

                ImGui::DockBuilderFinish(dockspace_id);
            }
        }

        // Static parameters for popups
        static char newSceneName[128] = "Untitled";
        static char openPath[256] = "Assets/Scenes/";
        static char savePath[256] = "Assets/Scenes/new_scene.omnixscene";

        enum class PendingAction { None, New, Open, Reload, Exit };
        static PendingAction pendingAction = PendingAction::None;

        auto triggerNewScene = [&]() {
            if (m_DirtyState.IsSceneDirty()) {
                pendingAction = PendingAction::New;
                ImGui::OpenPopup("Unsaved Changes");
            } else {
                ImGui::OpenPopup("New Scene Name");
            }
        };

        auto triggerOpenScene = [&]() {
            if (m_DirtyState.IsSceneDirty()) {
                pendingAction = PendingAction::Open;
                ImGui::OpenPopup("Unsaved Changes");
            } else {
                ImGui::OpenPopup("Open Scene");
            }
        };

        auto triggerSaveScene = [&]() {
            if (sceneMgr && sceneMgr->GetActiveScene()) {
                std::string currentPath = sceneMgr->GetActiveScene()->GetFilePath();
                if (currentPath.empty()) {
                    ImGui::OpenPopup("Save Scene As");
                } else {
                    if (sceneMgr->SaveActiveScene(currentPath)) {
                        m_DirtyState.ClearSceneDirty();
                    }
                }
            }
        };

        auto triggerSaveAsScene = [&]() {
            ImGui::OpenPopup("Save Scene As");
        };

        auto triggerReloadScene = [&]() {
            if (m_DirtyState.IsSceneDirty()) {
                pendingAction = PendingAction::Reload;
                ImGui::OpenPopup("Unsaved Changes");
            } else {
                if (sceneMgr) {
                    sceneMgr->ReloadCurrentScene();
                    m_DirtyState.ClearSceneDirty();
                    m_Selection.Clear();
                }
            }
        };

        auto triggerExit = [&]() {
            if (m_DirtyState.IsSceneDirty()) {
                pendingAction = PendingAction::Exit;
                ImGui::OpenPopup("Unsaved Changes");
            } else {
                if (m_Context && m_Context->renderer) {
                    auto* engineLoop = dynamic_cast<eng::runtime::EngineLoop*>(m_Context->renderer);
                    if (engineLoop) {
                        engineLoop->RequestExit();
                    }
                }
            }
        };

        bool isPlaying = (m_SimulationState == EditorSimulationState::Play);

        // Don't trigger shortcuts if user is typing in some text input field or playing
        if (!io.WantCaptureKeyboard && !isPlaying) {
            bool ctrl = io.KeyCtrl;
            if (ctrl) {
                if (ImGui::IsKeyPressed(ImGuiKey_N)) {
                    triggerNewScene();
                }
                if (ImGui::IsKeyPressed(ImGuiKey_O)) {
                    triggerOpenScene();
                }
                if (ImGui::IsKeyPressed(ImGuiKey_S)) {
                    triggerSaveScene();
                }
            }
        }

        if (ImGui::BeginMenuBar())
        {
            if (ImGui::BeginMenu("File"))
            {
                if (ImGui::MenuItem("New Scene", "Ctrl+N", false, !isPlaying)) {
                    triggerNewScene();
                }
                if (ImGui::MenuItem("Open Scene...", "Ctrl+O", false, !isPlaying)) {
                    triggerOpenScene();
                }
                if (ImGui::MenuItem("Save Scene", "Ctrl+S", false, !isPlaying)) {
                    triggerSaveScene();
                }
                if (ImGui::MenuItem("Save Scene As...", NULL, false, !isPlaying)) {
                    triggerSaveAsScene();
                }
                if (ImGui::MenuItem("Reload Scene", NULL, false, !isPlaying)) {
                    triggerReloadScene();
                }
                ImGui::Separator();
                if (ImGui::MenuItem("Exit", "Alt+F4")) {
                    triggerExit();
                }
                ImGui::EndMenu();
            }
            if (ImGui::BeginMenu("Edit")) { ImGui::EndMenu(); }
            if (ImGui::BeginMenu("View")) {
                bool show = m_ShowColliders;
                if (ImGui::MenuItem("Show Colliders", nullptr, &show)) {
                    m_ShowColliders = show;
                    m_ViewportPanel.SetShowColliders(show);
                }
                
                auto* engineLoop = m_Context ? dynamic_cast<eng::runtime::EngineLoop*>(m_Context->renderer) : nullptr;
                if (engineLoop && engineLoop->GetSceneRenderer()) {
                    auto* sceneRenderer = engineLoop->GetSceneRenderer();
                    bool useDefaultLighting = sceneRenderer->m_UseEditorDefaultLighting;
                    if (ImGui::MenuItem("Use Editor Default Lighting", nullptr, &useDefaultLighting)) {
                        sceneRenderer->m_UseEditorDefaultLighting = useDefaultLighting;
                    }
                }
                ImGui::EndMenu();
            }
            if (ImGui::BeginMenu("Window")) {
                if (ImGui::MenuItem("Reset Layout")) {
                    m_ResetLayout = true;
                }
                ImGui::EndMenu();
            }

            // Add Play/Stop simulation buttons in the menu bar itself!
            ImGui::Separator();
            ImGui::Spacing();

            if (m_SimulationState == EditorSimulationState::Edit) {
                if (ImGui::Button("▶ Play")) {
                    if (m_DirtyState.IsSceneDirty()) {
                        ImGui::OpenPopup("Unsaved Changes Before Play");
                    } else {
                        EnterPlayMode();
                    }
                }
                ImGui::SameLine();
                ImGui::TextColored(ImVec4(0.8f, 0.8f, 0.8f, 1.0f), "Mode: EDIT MODE");
            } else {
                if (ImGui::Button("■ Stop")) {
                    ExitPlayMode();
                }
                ImGui::SameLine();
                ImGui::TextColored(ImVec4(0.2f, 0.8f, 0.2f, 1.0f), "Mode: PLAY MODE");
            }

            ImGui::EndMenuBar();
        }

        // Render Panel Views
        m_HierarchyPanel.Render(m_Selection, m_DirtyState);
        m_InspectorPanel.Render(m_Selection, m_DirtyState);
        m_ConsolePanel.Render();

        // Retrieve offscreen viewport texture for current frame
        auto* engineLoop = m_Context ? dynamic_cast<eng::runtime::EngineLoop*>(m_Context->renderer) : nullptr;
        VkDescriptorSet viewportTexture = VK_NULL_HANDLE;
        if (engineLoop && engineLoop->GetSceneRenderer()) {
            auto* sceneRenderer = engineLoop->GetSceneRenderer();
            uint32_t currentFrame = sceneRenderer->frameIndex;
            viewportTexture = sceneRenderer->GetOffscreenTexture(currentFrame);
        }

        float panelWidth = 0.0f;
        float panelHeight = 0.0f;
        m_ViewportPanel.Render(viewportTexture, panelWidth, panelHeight);

        // Save the viewport size for recreation check at the start of the next frame
        m_LastViewportWidth = panelWidth;
        m_LastViewportHeight = panelHeight;

        m_AssetBrowserPanel.Render(m_Selection, m_DirtyState, [this](AssetHandle meshHandle) {
            this->CreateEntityFromMesh(meshHandle);
        });

        // Render Play Mode Diagnostics
        {
            ImGui::Begin("Play Mode Diagnostics");
            ImGui::Text("Simulation Mode: %s", (m_SimulationState == EditorSimulationState::Play) ? "PLAY MODE" : "EDIT MODE");
            ImGui::Text("Simulation State: %s", (m_Context->editorSimulationState == EditorSimulationState::Play || m_Context->mode == RuntimeMode::Game) ? "Enabled" : "Disabled");
            
            int activeSceneEntityCount = 0;
            if (sceneMgr && sceneMgr->GetActiveScene()) {
                activeSceneEntityCount = sceneMgr->GetActiveScene()->GetAllSceneObjects().size();
            }
            ImGui::Text("Active Scene Entity Count: %d", activeSceneEntityCount);
            
            uint32_t livingCount = 0;
            if (m_Context->ecs) {
                livingCount = m_Context->ecs->getCoordinator().GetLivingEntityCount();
            }
            ImGui::Text("ECS Living Entity Count: %u", livingCount);

            if (m_SimulationState == EditorSimulationState::Play) {
                auto now = std::chrono::high_resolution_clock::now();
                double duration = std::chrono::duration<double>(now - m_PlaySessionStart).count();
                ImGui::Text("Play Session Duration: %.2f seconds", duration);
            } else {
                ImGui::Text("Play Session Duration: 0.00 seconds");
            }
            
            ImGui::Text("Play/Stop Cycle Count: %d", m_PlayStopCycleCount);
            
            // Physics diagnostics
            ImGui::Separator();
            ImGui::Text("--- PhysX Physics Diagnostics ---");
            if (m_Context->physicsWorld) {
                auto* pw = m_Context->physicsWorld;
                ImGui::Text("PhysX Initialized: %s", pw->IsInitialized() ? "Yes" : "No");
                
                bool simEnabled = (m_SimulationState == EditorSimulationState::Play || m_Context->mode == RuntimeMode::Game);
                ImGui::Text("Simulation Enabled: %s", simEnabled ? "Yes" : "No");
                ImGui::Text("Active Static Actors: %zu", pw->GetStaticActorCount());

                int ecsColliderCount = 0;
                if (m_Context->ecs) {
                    auto& coordinator = m_Context->ecs->getCoordinator();
                    for (Entity entity : coordinator.GetActiveEntities()) {
                        auto signature = coordinator.GetSignature(entity);
                        if (signature.test(coordinator.GetComponentType<StaticBodyComponent>())) {
                            if (signature.test(coordinator.GetComponentType<BoxColliderComponent>()) ||
                                signature.test(coordinator.GetComponentType<SphereColliderComponent>()) ||
                                signature.test(coordinator.GetComponentType<CapsuleColliderComponent>())) {
                                ecsColliderCount++;
                            }
                        }
                    }
                }
                ImGui::Text("ECS Static Collider Count: %d", ecsColliderCount);
                ImGui::Text("Fixed Timestep: %.4f s (%.1f FPS)", pw->GetFixedTimestep(), 1.0f / pw->GetFixedTimestep());
                ImGui::Text("Steps This Frame: %d", pw->GetStepsThisFrame());

                if (g_DiagPhysState.lastRaycastPerformed) {
                    ImGui::Text("Last Raycast: %s", g_DiagPhysState.lastRaycastHit ? "Hit" : "Miss");
                    if (g_DiagPhysState.lastRaycastHit) {
                        ImGui::Text("Last Raycast Entity: %u", (unsigned int)g_DiagPhysState.lastRaycastEntity);
                    } else {
                        ImGui::Text("Last Raycast Entity: N/A");
                    }
                } else {
                    ImGui::Text("Last Raycast: None yet");
                    ImGui::Text("Last Raycast Entity: N/A");
                }

                if (g_DiagPhysState.lastOverlapPerformed) {
                    ImGui::Text("Last Overlap Count: %d", g_DiagPhysState.lastOverlapCount);
                } else {
                    ImGui::Text("Last Overlap Count: N/A");
                }

                ImGui::Spacing();
                
                if (ImGui::Button("Test Raycast Downward")) {
                    eng::physics::RaycastHit rHit;
                    Vector3 origin = {0.0f, 10.0f, 0.0f};
                    Vector3 direction = {0.0f, -1.0f, 0.0f};
                    float maxDist = 50.0f;
                    
                    if (pw->Raycast(origin, direction, maxDist, rHit)) {
                        CORE_LOG_INFO("[Physics Test] Raycast HIT entity ID: {}, Distance: {:.2f}, Position: ({:.2f}, {:.2f}, {:.2f}), Normal: ({:.2f}, {:.2f}, {:.2f})",
                            rHit.entity, rHit.distance, rHit.position.x, rHit.position.y, rHit.position.z,
                            rHit.normal.x, rHit.normal.y, rHit.normal.z);
                        g_DiagPhysState.lastRaycastHit = true;
                        g_DiagPhysState.lastRaycastEntity = rHit.entity;
                        // Add to debug visualizer
                        eng::physics::PhysicsDebugDraw::AddDebugRaycast(origin, direction, rHit.distance, true, rHit.position, rHit.normal);
                    } else {
                        CORE_LOG_INFO("[Physics Test] Raycast MISSED everything!");
                        g_DiagPhysState.lastRaycastHit = false;
                        g_DiagPhysState.lastRaycastEntity = 0;
                        // Add to debug visualizer
                        eng::physics::PhysicsDebugDraw::AddDebugRaycast(origin, direction, maxDist, false, {0,0,0}, {0,0,0});
                    }
                    g_DiagPhysState.lastRaycastPerformed = true;
                }

                ImGui::SameLine();
                if (ImGui::Button("Test Overlap Sphere at Origin")) {
                    std::vector<Entity> overlapped;
                    Vector3 center = {0.0f, 0.0f, 0.0f};
                    float radius = 5.0f;
                    if (pw->OverlapSphere(center, radius, overlapped)) {
                        CORE_LOG_INFO("[Physics Test] Overlap Sphere HIT {} entities:", overlapped.size());
                        for (size_t i = 0; i < overlapped.size(); ++i) {
                            CORE_LOG_INFO("  - Entity ID: {}", overlapped[i]);
                        }
                        g_DiagPhysState.lastOverlapCount = (int)overlapped.size();
                    } else {
                        CORE_LOG_INFO("[Physics Test] Overlap Sphere MISSED everything!");
                        g_DiagPhysState.lastOverlapCount = 0;
                    }
                    g_DiagPhysState.lastOverlapPerformed = true;
                }

                ImGui::Spacing();
                if (ImGui::Button("Rebuild Static Actors")) {
                    if (m_Context->ecs) {
                        pw->RegisterStaticColliders(m_Context->ecs->getCoordinator());
                        CORE_LOG_INFO("[Physics Test] Rebuilt all static actors.");
                    }
                }

                ImGui::SameLine();
                if (ImGui::Button("Clear Physics Scene")) {
                    pw->ClearScene();
                    eng::physics::PhysicsDebugDraw::ClearDebugVisuals();
                    CORE_LOG_INFO("[Physics Test] Cleared all static actors from physics scene.");
                }
            } else {
                ImGui::Text("PhysX Initialized: No (PhysicsWorld Context pointer is null)");
            }
            ImGui::End();
        }

        // --------------------------------------------------------------------
        // PLAY MODE POPUPS
        // --------------------------------------------------------------------
        if (ImGui::BeginPopupModal("Unsaved Changes Before Play", NULL, ImGuiWindowFlags_AlwaysAutoResize)) {
            ImGui::Text("Your scene has unsaved changes.\nPlay Mode will run a temporary copy of the current unsaved state.\nChanges made during Play Mode will be discarded when stopped.");
            ImGui::Separator();

            if (ImGui::Button("Play Anyway", ImVec2(100, 0))) {
                ImGui::CloseCurrentPopup();
                EnterPlayMode();
            }
            ImGui::SameLine();
            if (ImGui::Button("Cancel", ImVec2(100, 0))) {
                ImGui::CloseCurrentPopup();
            }
            ImGui::EndPopup();
        }

        // --------------------------------------------------------------------
        // POPUP MODALS FOR SCENE OPERATIONS
        // --------------------------------------------------------------------
        if (ImGui::BeginPopupModal("Unsaved Changes", NULL, ImGuiWindowFlags_AlwaysAutoResize)) {
            ImGui::Text("You have unsaved changes.\nDo you want to save them before proceeding?");
            ImGui::Separator();

            if (ImGui::Button("Save", ImVec2(80, 0))) {
                if (sceneMgr && sceneMgr->GetActiveScene()) {
                    std::string currentPath = sceneMgr->GetActiveScene()->GetFilePath();
                    if (currentPath.empty()) {
                        ImGui::CloseCurrentPopup();
                        ImGui::OpenPopup("Save Scene As");
                    } else {
                        if (sceneMgr->SaveActiveScene(currentPath)) {
                            m_DirtyState.ClearSceneDirty();
                            ImGui::CloseCurrentPopup();
                            
                            if (pendingAction == PendingAction::New) {
                                ImGui::OpenPopup("New Scene Name");
                            } else if (pendingAction == PendingAction::Open) {
                                ImGui::OpenPopup("Open Scene");
                            } else if (pendingAction == PendingAction::Reload) {
                                sceneMgr->ReloadCurrentScene();
                                m_DirtyState.ClearSceneDirty();
                                m_Selection.Clear();
                            } else if (pendingAction == PendingAction::Exit) {
                                if (m_Context && m_Context->renderer) {
                                    auto* engineLoop = dynamic_cast<eng::runtime::EngineLoop*>(m_Context->renderer);
                                    if (engineLoop) engineLoop->RequestExit();
                                }
                            }
                            pendingAction = PendingAction::None;
                        }
                    }
                } else {
                    ImGui::CloseCurrentPopup();
                }
            }
            ImGui::SameLine();
            if (ImGui::Button("Discard", ImVec2(80, 0))) {
                ImGui::CloseCurrentPopup();
                if (pendingAction == PendingAction::New) {
                    ImGui::OpenPopup("New Scene Name");
                } else if (pendingAction == PendingAction::Open) {
                    ImGui::OpenPopup("Open Scene");
                } else if (pendingAction == PendingAction::Reload) {
                    if (sceneMgr) {
                        sceneMgr->ReloadCurrentScene();
                        m_DirtyState.ClearSceneDirty();
                        m_Selection.Clear();
                    }
                } else if (pendingAction == PendingAction::Exit) {
                    if (m_Context && m_Context->renderer) {
                        auto* engineLoop = dynamic_cast<eng::runtime::EngineLoop*>(m_Context->renderer);
                        if (engineLoop) engineLoop->RequestExit();
                    }
                }
                pendingAction = PendingAction::None;
            }
            ImGui::SameLine();
            if (ImGui::Button("Cancel", ImVec2(80, 0))) {
                ImGui::CloseCurrentPopup();
                pendingAction = PendingAction::None;
            }
            ImGui::EndPopup();
        }

        if (ImGui::BeginPopupModal("New Scene Name", NULL, ImGuiWindowFlags_AlwaysAutoResize)) {
            ImGui::Text("Enter new scene name:");
            ImGui::InputText("##scene_name", newSceneName, IM_ARRAYSIZE(newSceneName));
            ImGui::Separator();

            if (ImGui::Button("Create", ImVec2(80, 0))) {
                if (sceneMgr) {
                    sceneMgr->CreateNewScene(newSceneName);
                    m_DirtyState.ClearSceneDirty();
                    m_Selection.Clear();
                }
                ImGui::CloseCurrentPopup();
            }
            ImGui::SameLine();
            if (ImGui::Button("Cancel", ImVec2(80, 0))) {
                ImGui::CloseCurrentPopup();
            }
            ImGui::EndPopup();
        }

        if (ImGui::BeginPopupModal("Open Scene", NULL, ImGuiWindowFlags_AlwaysAutoResize)) {
            ImGui::Text("Enter scene file path:");
            ImGui::InputText("##open_path", openPath, IM_ARRAYSIZE(openPath));
            ImGui::Separator();

            if (ImGui::Button("Open", ImVec2(80, 0))) {
                if (sceneMgr) {
                    sceneMgr->LoadScene(openPath);
                }
                ImGui::CloseCurrentPopup();
            }
            ImGui::SameLine();
            if (ImGui::Button("Cancel", ImVec2(80, 0))) {
                ImGui::CloseCurrentPopup();
            }
            ImGui::EndPopup();
        }

        if (ImGui::BeginPopupModal("Save Scene As", NULL, ImGuiWindowFlags_AlwaysAutoResize)) {
            ImGui::Text("Enter output file path (.omnixscene or .json):");
            ImGui::InputText("##save_path", savePath, IM_ARRAYSIZE(savePath));
            ImGui::Separator();

            if (ImGui::Button("Save", ImVec2(80, 0))) {
                if (sceneMgr) {
                    if (sceneMgr->SaveActiveScene(savePath)) {
                        m_DirtyState.ClearSceneDirty();
                        ImGui::CloseCurrentPopup();

                        if (pendingAction == PendingAction::New) {
                            ImGui::OpenPopup("New Scene Name");
                        } else if (pendingAction == PendingAction::Open) {
                            ImGui::OpenPopup("Open Scene");
                        } else if (pendingAction == PendingAction::Reload) {
                            sceneMgr->ReloadCurrentScene();
                            m_DirtyState.ClearSceneDirty();
                            m_Selection.Clear();
                        } else if (pendingAction == PendingAction::Exit) {
                            if (m_Context && m_Context->renderer) {
                                auto* engineLoop = dynamic_cast<eng::runtime::EngineLoop*>(m_Context->renderer);
                                if (engineLoop) engineLoop->RequestExit();
                            }
                        }
                        pendingAction = PendingAction::None;
                    }
                } else {
                    ImGui::CloseCurrentPopup();
                }
            }
            ImGui::SameLine();
            if (ImGui::Button("Cancel", ImVec2(80, 0))) {
                ImGui::CloseCurrentPopup();
                pendingAction = PendingAction::None;
            }
            ImGui::EndPopup();
        }

        ImGui::End();
    }

    bool EditorLayer::EnterPlayMode() {
        if (m_SimulationState == EditorSimulationState::Play) return false;

        auto* sceneMgr = dynamic_cast<SceneManager*>(m_Context->scenes);
        if (!sceneMgr) return false;

        // 1. Sync ECS changes to SceneObject tree
        sceneMgr->SyncECSToScene();

        // 2. Save pre-play dirty state
        m_EditDirtyBeforePlay = m_DirtyState.IsSceneDirty();

        if (!sceneMgr->GetActiveScene()) {
            sceneMgr->CreateNewScene("TempPlayScene");
        }

        // 3. Start play session timing and cycle count
        m_PlayStopCycleCount++;
        m_PlaySessionStart = std::chrono::high_resolution_clock::now();

        // 4. Clear selection list
        m_Selection.Clear();

        // 5. Clone active scene and active ECS in-memory
        Scene* activeScene = sceneMgr->GetActiveScene();
        m_EditSceneBackup = activeScene;

        auto simulationWorld = m_Context->ecs->Clone();

        std::unordered_map<Entity, Entity> entityMap;
        Scene* simulationScene = activeScene->Clone(
            m_Context->ecs->getCoordinator(),
            simulationWorld->getCoordinator(),
            entityMap
        );

        m_EditWorldBackup = m_Context->swapECS(std::move(simulationWorld));

        sceneMgr->SetActiveScene(simulationScene);

        // 6. Switch simulation state to Play
        m_SimulationState = EditorSimulationState::Play;
        m_Context->editorSimulationState = EditorSimulationState::Play;

        if (m_Context->physicsWorld) {
            m_Context->physicsWorld->ClearScene();
            m_Context->physicsWorld->RegisterStaticColliders(m_Context->ecs->getCoordinator());
            eng::physics::PhysicsDebugDraw::ClearDebugVisuals();
        }

        CORE_LOG_INFO("[Editor] Entering Play Mode. In-memory simulation started.");
        return true;
    }

    bool EditorLayer::ExitPlayMode() {
        if (m_SimulationState != EditorSimulationState::Play) return false;

        auto* sceneMgr = dynamic_cast<SceneManager*>(m_Context->scenes);
        if (!sceneMgr) return false;

        // 1. Stop simulation
        m_Context->editorSimulationState = EditorSimulationState::Edit;

        // 2. Clear selection list
        m_Selection.Clear();

        // 3. Restore the original edit scene and discard simulated one
        if (sceneMgr->GetActiveScene()) {
            delete sceneMgr->GetActiveScene();
        }
        sceneMgr->SetActiveScene(m_EditSceneBackup);
        m_EditSceneBackup = nullptr;

        // 4. Restore the original edit world and discard the simulated one
        if (m_EditWorldBackup) {
            m_Context->swapECS(std::move(m_EditWorldBackup));
            m_EditWorldBackup = nullptr;
        }

        // 5. Set simulation state to Edit
        m_SimulationState = EditorSimulationState::Edit;

        if (m_Context->physicsWorld) {
            m_Context->physicsWorld->ClearScene();
            m_Context->physicsWorld->RegisterStaticColliders(m_Context->ecs->getCoordinator());
            eng::physics::PhysicsDebugDraw::ClearDebugVisuals();
        }

        // 6. Restore dirty state to pre-play value after scene is swapped
        m_RestoreDirtyStateAfterLoad = true;

        CORE_LOG_INFO("[Editor] Exiting Play Mode. In-memory scene restored.");
        return true;
    }

    void EditorLayer::CreateEntityFromMesh(AssetHandle meshHandle) {
        if (!m_Context || !m_Context->ecs || !m_Context->assetRegistry) {
            CORE_LOG_ERROR("[Editor] Cannot create entity: Context, ECS, or AssetRegistry is null!");
            return;
        }

        auto& coordinator = m_Context->ecs->getCoordinator();
        Entity newEntity = coordinator.CreateEntity();

        const AssetMetadata* meta = m_Context->assetRegistry->GetMetadata(meshHandle);
        std::string entityName = "New Entity";
        if (meta) {
            entityName = std::filesystem::path(meta->sourcePath).stem().string();
        }

        coordinator.AddComponent<NameComponent>(newEntity, NameComponent(entityName));
        coordinator.AddComponent<TransformComponent>(newEntity, TransformComponent());
        coordinator.AddComponent<MeshRendererComponent>(newEntity, MeshRendererComponent());
        coordinator.AddComponent<RenderableMeshComponent>(newEntity, RenderableMeshComponent(meshHandle));

        // Create default.omnixmat if missing on disk, and register it
        std::filesystem::create_directories("Assets/Materials");
        std::string defaultMatPath = "Assets/Materials/default.omnixmat";
        if (!std::filesystem::exists(defaultMatPath)) {
            OmnixMaterial dmat;
            dmat.name = "default";
            dmat.header.blendMode = 0;
            dmat.header.cullMode = 0;
            dmat.header.depthTest = 1;
            SerializeMaterial(dmat, defaultMatPath);
        }

        AssetHandle defaultMatHandle = m_Context->assetRegistry->RegisterAsset(defaultMatPath, AssetType::Material);
        coordinator.AddComponent<MaterialComponent>(newEntity, MaterialComponent(defaultMatHandle));

        m_Selection.Select(newEntity);
        m_DirtyState.MarkSceneDirty();

        // Frame camera on the new entity immediately!
        glm::vec3 entityPos = glm::vec3(0.0f, 0.0f, 0.0f);
        m_EditorCamera.position = entityPos - m_EditorCamera.getForward() * 5.0f;
        m_EditorCamera.LookAt(entityPos);

        CORE_LOG_INFO("[Editor] Successfully created Entity %u from mesh asset: %s", newEntity, entityName.c_str());
    }

} // namespace eng::runtime
