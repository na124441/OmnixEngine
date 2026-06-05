#include "Runtime/Public/Editor/EditorLayer.h"
#include "Runtime/Public/Editor/EditorTheme.h"
#include "Runtime/Public/Gameplay/VerticalSliceGameMode.h"
#include "Runtime/Public/Gameplay/PlayerStateComponent.h"
#include "Runtime/Public/Gameplay/UI/GameplayHUD.h"
#include "Runtime/Public/Gameplay/GameplayEvent.h"
#include "Runtime/Public/Gameplay/GameplayEventBus.h"
#include "Runtime/Public/Gameplay/Objectives/ObjectiveSystem.h"
#include "Runtime/Public/Gameplay/StateObjects/ObjectActivationSystem.h"
#include "Runtime/Public/Gameplay/CheckpointSystem.h"
#include "Runtime/Public/Gameplay/Save/GameplaySaveSystem.h"
#include "Runtime/Public/Audio/AudioSystem.h"
#include "Runtime/Public/Editor/EditorLayout.h"
#include "Runtime/Public/Editor/EditorEntityCommands.h"
#include "Runtime/Public/Editor/PlatformFileDialog.h"
#include "Runtime/Public/Editor/AssetImportService.h"
#include "Core/Logging/Logger.h"
#include "Core/World.h"
#include "RenderingEngine/Runtime/engine/EngineLoop.h"
#include "RenderingEngine/Core/Engine/EngineResources.h"
#include "RenderingEngine/Platform/window/Window.h"
#include "ECS/Coordinator.h"
#include "ECS/ECSComponents.h"
#include "ECS/TriggerSystem.h"
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
#include "Scene/SceneValidator.h"
#include "Scene/PrefabRegistry.h"

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
        EditorTheme::ApplyDarkTheme();

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
        m_ShowInteractPrompt = false;

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
                CORE_LOG_INFO("[EditorLayer] Recreated viewport offscreen targets: %ux%u", targetWidth, targetHeight);
            }
        }

        // Update the Editor Camera
        float dt = ImGui::GetIO().DeltaTime;
        if (m_SimulationState == EditorSimulationState::Edit) {
            auto* engineLoop = m_Context ? dynamic_cast<eng::runtime::EngineLoop*>(m_Context->renderer) : nullptr;
            bool wasDragging = m_EditorCamera.m_IsDraggingRMB;
            m_EditorCamera.Update(dt, m_ViewportPanel.IsHovered(), m_ViewportPanel.IsFocused());
            bool isDragging = m_EditorCamera.m_IsDraggingRMB;

            if (engineLoop && engineLoop->GetWindow()) {
                if (isDragging && !wasDragging) {
                    engineLoop->GetWindow()->SetCursorMode(eng::platform::CursorMode::Disabled);
                } else if (!isDragging && wasDragging) {
                    engineLoop->GetWindow()->SetCursorMode(eng::platform::CursorMode::Normal);
                }
            }
        }

        // Handle focusing on selected entity with 'F' key
        if (m_SimulationState == EditorSimulationState::Edit && (m_ViewportPanel.IsHovered() || m_ViewportPanel.IsFocused()) && ImGui::IsKeyPressed(ImGuiKey_F)) {
            Entity selectedEntity = m_Selection.GetSelectedEntity();
            if (selectedEntity != 0 && m_Context && m_Context->ecs && m_Context->ecs->getCoordinator().IsEntityAlive(selectedEntity)) {
                auto& coordinator = m_Context->ecs->getCoordinator();
                auto transformType = coordinator.GetComponentType<TransformComponent>();
                if (coordinator.GetSignature(selectedEntity).test(transformType)) {
                    const auto& transform = coordinator.GetComponent<TransformComponent>(selectedEntity);
                    glm::vec3 entityPos(transform.position.x, transform.position.y, transform.position.z);
                    
                    // Focus framing: move camera to look at the entity using FrameEntity
                    float boundsRadius = 1.0f;
                    m_EditorCamera.FrameEntity(entityPos, boundsRadius);
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

        // Gameplay hotkeys (F1 - F4) in Play/Pause simulation state
        if ((m_SimulationState == EditorSimulationState::Play || m_SimulationState == EditorSimulationState::Pause) && m_GameMode && m_Context) {
            if (ImGui::IsKeyPressed(ImGuiKey_F1)) {
                m_GameMode->CompleteLevel();
            }
            if (ImGui::IsKeyPressed(ImGuiKey_F2)) {
                m_GameMode->FailLevel();
            }
            if (ImGui::IsKeyPressed(ImGuiKey_F3)) {
                m_GameMode->RestartLevel();
                ExitPlayMode();
                EnterPlayMode();
            }
            if (ImGui::IsKeyPressed(ImGuiKey_F4)) {
                if (m_GameMode->GetState() == GameSessionState::Playing) {
                    m_GameMode->PauseLevel();
                    m_SimulationState = EditorSimulationState::Pause;
                    m_Context->editorSimulationState = EditorSimulationState::Pause;
                } else if (m_GameMode->GetState() == GameSessionState::Paused) {
                    m_GameMode->ResumeLevel();
                    m_SimulationState = EditorSimulationState::Play;
                    m_Context->editorSimulationState = EditorSimulationState::Play;
                }
            }
        }

        // If in Play Mode, handle cursor capturing, update character controller look, and override SceneRenderer camera parameters
        if (m_SimulationState == EditorSimulationState::Play && m_Context && m_Context->renderer) {
            auto* engineLoop = dynamic_cast<eng::runtime::EngineLoop*>(m_Context->renderer);
            if (engineLoop && engineLoop->GetWindow()) {
                if (ImGui::IsKeyPressed(ImGuiKey_Escape)) {
                    m_CursorCaptured = false;
                    engineLoop->GetWindow()->SetCursorMode(eng::platform::CursorMode::Normal);
                } else if (m_ViewportPanel.IsHovered() && ImGui::IsMouseClicked(0)) {
                    m_CursorCaptured = true;
                    engineLoop->GetWindow()->SetCursorMode(eng::platform::CursorMode::Disabled);
                }
            }

            if (m_Context->ecs) {
                auto* world = dynamic_cast<World*>(m_Context->ecs);
                if (world) {
                    if (auto playerControllerSys = world->GetSystem<PlayerControllerSystem>()) {
                        playerControllerSys->UpdateCameraLook(m_Context->ecs->getCoordinator(), m_CursorCaptured);
                        
                        if (engineLoop && engineLoop->GetSceneRenderer()) {
                            auto* sceneRenderer = engineLoop->GetSceneRenderer();
                            auto& coordinator = m_Context->ecs->getCoordinator();
                            Entity playerEnt = playerControllerSys->GetPlayerEntity();
                            if (playerEnt != 0 && coordinator.IsEntityAlive(playerEnt)) {
                                const auto& transform = coordinator.GetComponent<TransformComponent>(playerEnt);
                                const auto& ccc = coordinator.GetComponent<CharacterControllerComponent>(playerEnt);
                                const auto& cameraComp = coordinator.GetComponent<CameraComponent>(playerEnt);

                                float yawRad = glm::radians(ccc.yaw);
                                float pitchRad = glm::radians(ccc.pitch);
                                
                                glm::vec3 forwardDir(
                                    std::cos(pitchRad) * std::cos(yawRad),
                                    std::sin(pitchRad),
                                    std::cos(pitchRad) * std::sin(yawRad)
                                );
                                forwardDir = glm::normalize(forwardDir);

                                glm::vec3 rightDir = glm::normalize(glm::vec3(-std::sin(yawRad), 0.0f, std::cos(yawRad)));
                                glm::vec3 forwardDirXZ = glm::normalize(glm::vec3(std::cos(yawRad), 0.0f, std::sin(yawRad)));
                                glm::vec3 rotatedOffset = 
                                    cameraComp.localOffset.x * rightDir + 
                                    cameraComp.localOffset.y * glm::vec3(0.0f, 1.0f, 0.0f) + 
                                    cameraComp.localOffset.z * forwardDirXZ;

                                glm::vec3 eyePos(transform.position.x, transform.position.y, transform.position.z);
                                eyePos += rotatedOffset;

                                sceneRenderer->camera.position = eyePos;
                                sceneRenderer->camera.target = eyePos + forwardDir;
                                
                                glm::vec3 upDir = glm::normalize(glm::cross(rightDir, forwardDir));
                                sceneRenderer->camera.up = upDir;

                                sceneRenderer->camera.fovY = glm::radians(cameraComp.fov);
                                sceneRenderer->camera.nearPlane = cameraComp.nearPlane;
                                sceneRenderer->camera.farPlane = cameraComp.farPlane;
                            }
                        }
                    }


                }
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
                    float vpWidth = m_ViewportPanel.GetViewportWidth();
                    float vpHeight = m_ViewportPanel.GetViewportHeight();
                    if (vpWidth > 0 && vpHeight > 0) {
                        float aspectRatio = vpWidth / vpHeight;
                        glm::mat4 proj = camera.getProjMatrix(aspectRatio);
                        eng::physics::PhysicsDebugDraw::Render(
                            m_Context->ecs->getCoordinator(),
                            view,
                            proj,
                            vpWidth,
                            vpHeight,
                            m_ViewportPanel.GetViewportScreenX(),
                            m_ViewportPanel.GetViewportScreenY()
                        );
                    }
                }
            }
        }

        // Draw gameplay HUD in Play Mode
        if (m_SimulationState == EditorSimulationState::Play && m_GameMode) {
            auto* gameplayHUD = m_GameMode->GetGameplayHUD();
            if (gameplayHUD) {
                float vpX = m_ViewportPanel.GetViewportScreenX();
                float vpY = m_ViewportPanel.GetViewportScreenY();
                float vpW = m_ViewportPanel.GetViewportWidth();
                float vpH = m_ViewportPanel.GetViewportHeight();
                gameplayHUD->Render(vpX, vpY, vpW, vpH);
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

        // Static parameters for popups and lambdas defined at top for scope access
        static char newSceneName[128] = "Untitled";
        static char openPath[256] = "Assets/Scenes/";
        static char savePath[256] = "Assets/Scenes/new_scene.omnixscene";

        enum class PendingAction { None, New, Open, Reload, Exit };
        static PendingAction pendingAction = PendingAction::None;

        auto* sceneMgr = dynamic_cast<SceneManager*>(m_Context->scenes);

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

        sceneMgr = dynamic_cast<SceneManager*>(m_Context->scenes);
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
        // Draw top toolbar
        ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(8.0f, 6.0f));
        ImGui::PushStyleColor(ImGuiCol_ChildBg, ImVec4(0.06f, 0.06f, 0.07f, 1.00f));
        ImGui::PushStyleColor(ImGuiCol_Border, ImVec4(0.18f, 0.18f, 0.20f, 1.00f));
        
        float toolbarHeight = ImGui::GetFrameHeightWithSpacing() + 4.0f;
        if (ImGui::BeginChild("##ToolbarChild", ImVec2(0.0f, toolbarHeight), true, ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_MenuBar)) 
        {
            if (ImGui::BeginMenuBar()) 
            {
                ImGui::SetCursorPosY(ImGui::GetCursorPosY() + 2.0f);
                
                // Save/Load
                if (ImGui::Button("Save")) {
                    triggerSaveScene();
                }
                ImGui::SameLine();
                if (ImGui::Button("Load")) {
                    triggerOpenScene();
                }
                ImGui::SameLine();
                ImGui::SeparatorEx(ImGuiSeparatorFlags_Vertical);
                ImGui::SameLine();
                
                // Play / Stop
                bool isPlayMode = (m_SimulationState == EditorSimulationState::Play);
                if (isPlayMode) {
                    ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.70f, 0.20f, 0.20f, 1.00f));
                    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.80f, 0.30f, 0.30f, 1.00f));
                    ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(0.90f, 0.40f, 0.40f, 1.00f));
                } else {
                    ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.20f, 0.55f, 0.20f, 1.00f));
                    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.25f, 0.65f, 0.25f, 1.00f));
                    ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(0.30f, 0.75f, 0.30f, 1.00f));
                }
                if (ImGui::Button(isPlayMode ? "Stop" : "Play")) {
                    if (isPlayMode) {
                        m_SimulationState = EditorSimulationState::Edit;
                        ExitPlayMode();
                    } else {
                        m_SimulationState = EditorSimulationState::Play;
                        EnterPlayMode();
                    }
                }
                ImGui::PopStyleColor(3);
                
                ImGui::SameLine();
                ImGui::SeparatorEx(ImGuiSeparatorFlags_Vertical);
                ImGui::SameLine();
                
                if (ImGui::Button("Import Asset")) {
                    CORE_LOG_INFO("[Editor] Import Asset clicked.");
                    std::string selectedFile = eng::editor::PlatformFileDialog::ShowOpenDialog("Wavefront OBJ (*.obj)\0*.obj\0All Files (*.*)\0*.*\0");
                    if (!selectedFile.empty()) {
                        std::string relPath = eng::runtime::AssetImportService::ImportModel(selectedFile, m_Context->assetRegistry);
                        if (!relPath.empty()) {
                            m_Context->assetRegistry->ScanProjectAssets();
                            AssetHandle handle = GenerateAssetUUID(relPath, AssetType::Mesh);
                            if (handle.IsValid()) {
                                CreateEntityFromMesh(handle);
                            }
                        }
                    }
                }
                ImGui::SameLine();
                if (ImGui::Button("Create Test Cube")) {
                    AssetHandle cubeHandle = GenerateAssetUUID("Assets/Models/cube.obj", AssetType::Mesh);
                    AssetHandle woodHandle = GenerateAssetUUID("Assets/Materials/wood.omnixmat", AssetType::Material);
                    
                    auto& coordinator = m_Context->ecs->getCoordinator();
                    Entity newEntity = coordinator.CreateEntity();
                    
                    coordinator.AddComponent<NameComponent>(newEntity, NameComponent("Test Cube"));
                    
                    TransformComponent tc;
                    tc.position = Vector3(0.0f, 0.5f, 0.0f);
                    tc.scale = Vector3(2.0f, 2.0f, 2.0f);
                    tc.dirty = true;
                    coordinator.AddComponent<TransformComponent>(newEntity, tc);
                    
                    coordinator.AddComponent<MeshRendererComponent>(newEntity, MeshRendererComponent());
                    coordinator.AddComponent<RenderableMeshComponent>(newEntity, RenderableMeshComponent(cubeHandle));
                    coordinator.AddComponent<MaterialComponent>(newEntity, MaterialComponent(woodHandle));
                    
                    m_Selection.Select(newEntity);
                    m_DirtyState.MarkSceneDirty();
                    
                    // Frame selected automatically
                    m_EditorCamera.FrameEntity(glm::vec3(0.0f, 0.5f, 0.0f), 2.0f);
                    m_EditorCamera.LookAt(glm::vec3(0.0f, 0.5f, 0.0f));
                    
                    CORE_LOG_INFO("[Editor] Successfully created Test Cube!");
                }
                ImGui::SameLine();
                // One-click setup: floor plane + PlayerStart so Play mode works immediately
                if (ImGui::Button("Create Play Setup")) {
                    auto& coord = m_Context->ecs->getCoordinator();

                    // --- Floor entity (large flat cube used as ground) ---
                    AssetHandle cubeHandle = GenerateAssetUUID("Assets/Models/cube.obj", AssetType::Mesh);
                    Entity floorEnt = coord.CreateEntity();
                    coord.AddComponent<NameComponent>(floorEnt, NameComponent("Floor"));

                    TransformComponent floorTc;
                    floorTc.position = Vector3(0.0f, -0.25f, 0.0f);  // centred at origin, sits at Y=0
                    floorTc.scale    = Vector3(20.0f, 0.5f, 20.0f);  // 20x0.5x20 flat slab
                    floorTc.dirty    = true;
                    coord.AddComponent<TransformComponent>(floorEnt, floorTc);

                    coord.AddComponent<MeshRendererComponent>(floorEnt, MeshRendererComponent());
                    coord.AddComponent<RenderableMeshComponent>(floorEnt, RenderableMeshComponent(cubeHandle));

                    // Default material
                    std::filesystem::create_directories("Assets/Materials");
                    std::string defaultMatPath = "Assets/Materials/default.omnixmat";
                    if (!std::filesystem::exists(defaultMatPath)) {
                        OmnixMaterial dmat; dmat.name = "default";
                        dmat.header.blendMode = 0; dmat.header.cullMode = 0; dmat.header.depthTest = 1;
                        SerializeMaterial(dmat, defaultMatPath);
                    }
                    AssetHandle defMatH = m_Context->assetRegistry->RegisterAsset(defaultMatPath, AssetType::Material);
                    coord.AddComponent<MaterialComponent>(floorEnt, MaterialComponent(defMatH));

                    // Box collider matching the floor slab (half-extents = scale/2)
                    BoxColliderComponent floorCol;
                    floorCol.size   = { 20.0f, 0.5f, 20.0f };
                    floorCol.offset = { 0.0f, 0.0f, 0.0f };
                    coord.AddComponent<BoxColliderComponent>(floorEnt, floorCol);

                    // --- PlayerStart entity ---
                    Entity psEnt = coord.CreateEntity();
                    coord.AddComponent<NameComponent>(psEnt, NameComponent("PlayerStart"));
                    TransformComponent psTc;
                    psTc.position = Vector3(0.0f, 2.0f, 5.0f);  // spawn 2 m above floor, 5 m back
                    psTc.dirty    = true;
                    coord.AddComponent<TransformComponent>(psEnt, psTc);
                    coord.AddComponent<PlayerStartComponent>(psEnt, PlayerStartComponent());

                    m_DirtyState.MarkSceneDirty();
                    // Frame camera on the floor so it's immediately visible
                    m_EditorCamera.position = glm::vec3(0.0f, 6.0f, 14.0f);
                    m_EditorCamera.LookAt(glm::vec3(0.0f, 0.0f, 0.0f));

                    CORE_LOG_INFO("[Editor] Created play setup: Floor + PlayerStart.");
                }
                ImGui::SameLine();
                if (ImGui::Button("Create Mesh Entity")) {
                    AssetHandle selectedAsset = m_AssetBrowserPanel.GetSelectedAsset();
                    if (selectedAsset.IsValid()) {
                        CreateEntityFromMesh(selectedAsset);
                    } else {
                        auto& coordinator = m_Context->ecs->getCoordinator();
                        EditorEntityCommands::CreateEmpty(coordinator, m_DirtyState, m_Selection);
                    }
                }
                ImGui::SameLine();
                if (ImGui::Button("Validate Scene")) {
                    if (sceneMgr && sceneMgr->GetActiveScene()) {
                        std::string currentPath = sceneMgr->GetActiveScene()->GetFilePath();
                        if (!currentPath.empty()) {
                            SceneValidator validator;
                            auto report = validator.ValidateSceneFile(currentPath, m_Context->assetRegistry, &PrefabRegistry::Get());
                            auto* casted = dynamic_cast<SceneManager*>(sceneMgr);
                            if (casted) {
                                casted->SetLastValidationReport(report);
                                casted->TriggerValidationFailedModal();
                            }
                        }
                    }
                }

                ImGui::EndMenuBar();
            }
            ImGui::EndChild();
        }
        ImGui::PopStyleColor(2);
        ImGui::PopStyleVar();

        // Submit the DockSpace
        if (io.ConfigFlags & ImGuiConfigFlags_DockingEnable)
        {
            ImGuiID dockspace_id = ImGui::GetID("EditorDockSpace");
            ImGui::DockSpace(dockspace_id, ImVec2(0.0f, 0.0f), dockspace_flags);

            // Establish a default clean docking layout if not already configured
            if (m_ResetLayout || ImGui::DockBuilderGetNode(dockspace_id) == NULL)
            {
                m_ResetLayout = false;
                EditorLayout::BuildDefaultDockspace(dockspace_id);
            }
        }

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
                if (ImGui::MenuItem("Validate Scene", NULL, false, !isPlaying)) {
                    if (sceneMgr && sceneMgr->GetActiveScene()) {
                        std::string currentPath = sceneMgr->GetActiveScene()->GetFilePath();
                        if (!currentPath.empty()) {
                            SceneValidator validator;
                            auto report = validator.ValidateSceneFile(currentPath, m_Context->assetRegistry, &PrefabRegistry::Get());
                            auto* casted = dynamic_cast<SceneManager*>(sceneMgr);
                            if (casted) {
                                casted->SetLastValidationReport(report);
                                casted->TriggerValidationFailedModal();
                            }
                        } else {
                            CORE_LOG_WARN("[Editor] Active scene has no saved file path to validate.");
                        }
                    }
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

                bool showDiag = m_ShowDiagnostics;
                if (ImGui::MenuItem("Show Diagnostics", nullptr, &showDiag)) {
                    m_ShowDiagnostics = showDiag;
                    m_ViewportPanel.SetShowDiagnostics(showDiag);
                }
                
                bool showValidator = m_ShowGameplayValidatorWindow;
                if (ImGui::MenuItem("Gameplay Validator", nullptr, &showValidator)) {
                    m_ShowGameplayValidatorWindow = showValidator;
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
        DrawGameplayValidatorWindow();

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
        m_ViewportPanel.Render(viewportTexture, panelWidth, panelHeight, m_Selection, m_DirtyState);

        // Save the viewport size for recreation check at the start of the next frame
        m_LastViewportWidth = panelWidth;
        m_LastViewportHeight = panelHeight;

        m_AssetBrowserPanel.Render(m_Selection, m_DirtyState, [this](AssetHandle meshHandle) {
            this->CreateEntityFromMesh(meshHandle);
        });

        // Render Play Mode Diagnostics
        if (m_ShowDiagnostics)
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
                     // GameMode / Gameplay Diagnostics
            ImGui::Separator();
            ImGui::TextDisabled("GAME STATE");
            if (m_GameMode) {
                const auto& gs = m_GameMode->GetGameState();
                ImGui::Text("Scene: %s", gs.ActiveSceneName.c_str());
                
                const char* sessionStr = "Unknown";
                switch (gs.SessionState) {
                    case GameSessionState::None: sessionStr = "None"; break;
                    case GameSessionState::Starting: sessionStr = "Starting"; break;
                    case GameSessionState::Playing: sessionStr = "Playing"; break;
                    case GameSessionState::Paused: sessionStr = "Paused"; break;
                    case GameSessionState::Completed: sessionStr = "Completed"; break;
                    case GameSessionState::Failed: sessionStr = "Failed"; break;
                    case GameSessionState::Restarting: sessionStr = "Restarting"; break;
                }
                ImGui::Text("Session: %s", sessionStr);
                ImGui::Text("Objective: %s", gs.ActiveObjectiveID.empty() ? "None" : gs.ActiveObjectiveID.c_str());
                ImGui::Text("Checkpoint: %s", gs.CurrentCheckpointID.empty() ? "None" : gs.CurrentCheckpointID.c_str());
                ImGui::Text("Elapsed Time: %.1fs", gs.ElapsedGameplayTime);
                
                // Completed Objectives list
                if (!gs.CompletedObjectives.empty()) {
                    ImGui::Text("Completed Objectives:");
                    for (const auto& obj : gs.CompletedObjectives) {
                        ImGui::BulletText("%s", obj.c_str());
                    }
                }
            } else {
                ImGui::Text("Scene: None");
                ImGui::Text("Session: None");
                ImGui::Text("Objective: None");
                ImGui::Text("Checkpoint: None");
                ImGui::Text("Elapsed Time: 0.0s");
            }
            
            ImGui::Separator();
            ImGui::TextDisabled("PLAYER STATE");
            Entity playerEnt = m_GameMode ? m_GameMode->FindPlayerEntity() : INVALID_ENTITY;
            if (playerEnt != INVALID_ENTITY && m_Context && m_Context->ecs) {
                auto& coord = m_Context->ecs->getCoordinator();
                auto pscType = coord.GetComponentType<PlayerStateComponent>();
                if (coord.GetSignature(playerEnt).test(pscType)) {
                    const auto& psc = coord.GetComponent<PlayerStateComponent>(playerEnt);
                    ImGui::Text("Player Entity: %u", psc.ActivePlayer);
                    ImGui::Text("Health: %.0f", psc.Health);
                    ImGui::Text("Alive: %s", psc.IsAlive ? "true" : "false");
                    ImGui::Text("Movement: %s", psc.MovementEnabled ? "Enabled" : "Disabled");
                    if (psc.CurrentInteractionTarget == INVALID_ENTITY) {
                        ImGui::Text("Interaction Target: None");
                    } else {
                        ImGui::Text("Interaction Target: %u", psc.CurrentInteractionTarget);
                    }
                } else {
                    ImGui::TextColored(ImVec4(1.0f, 0.4f, 0.4f, 1.0f), "WARNING: Player entity has no PlayerStateComponent!");
                }
            } else {
                ImGui::TextColored(ImVec4(1.0f, 0.4f, 0.4f, 1.0f), "WARNING: No Player entity with PlayerTagComponent found!");
                ImGui::Text("Player Entity: None");
                ImGui::Text("Health: 0");
                ImGui::Text("Alive: false");
                ImGui::Text("Movement: Disabled");
                ImGui::Text("Interaction Target: None");
            }

            ImGui::Separator();
            ImGui::TextDisabled("INTERACTION");
            if (m_Context && m_Context->interactionPrompt.Visible) {
                ImGui::Text("Current Target: Entity %u", m_Context->interactionPrompt.Target);
                ImGui::Text("Prompt: %s", m_Context->interactionPrompt.Text.c_str());
                ImGui::Text("Type: %s", InteractionTypeToString(m_Context->interactionPrompt.Type).c_str());

                float distanceVal = 0.0f;
                bool isTargetEnabled = false;
                if (m_Context->ecs && m_Context->ecs->getCoordinator().IsEntityAlive(m_Context->interactionPrompt.Target)) {
                    auto& coord = m_Context->ecs->getCoordinator();
                    auto targetSig = coord.GetSignature(m_Context->interactionPrompt.Target);
                    bool hasPlayerTrans = false;
                    if (playerEnt != INVALID_ENTITY && coord.IsEntityAlive(playerEnt)) {
                        auto playerSig = coord.GetSignature(playerEnt);
                        hasPlayerTrans = playerSig.test(coord.GetComponentType<TransformComponent>());
                    }
                    if (targetSig.test(coord.GetComponentType<TransformComponent>()) && hasPlayerTrans) {
                        auto& targetTrans = coord.GetComponent<TransformComponent>(m_Context->interactionPrompt.Target);
                        auto& playerTrans = coord.GetComponent<TransformComponent>(playerEnt);
                        float dx = targetTrans.position.x - playerTrans.position.x;
                        float dy = targetTrans.position.y - playerTrans.position.y;
                        float dz = targetTrans.position.z - playerTrans.position.z;
                        distanceVal = std::sqrt(dx*dx + dy*dy + dz*dz);
                    }
                    if (targetSig.test(coord.GetComponentType<InteractableComponent>())) {
                        isTargetEnabled = coord.GetComponent<InteractableComponent>(m_Context->interactionPrompt.Target).Enabled;
                    }
                }
                ImGui::Text("Distance: %.1fm", distanceVal);
                ImGui::Text("Enabled: %s", isTargetEnabled ? "true" : "false");
                ImGui::Text("Input Key: E");
                ImGui::Text("Can Interact: %s", isTargetEnabled ? "true" : "false");
            } else {
                ImGui::Text("Current Target: None");
                ImGui::Text("Prompt: None");
                ImGui::Text("Type: None");
                ImGui::Text("Distance: 0.0m");
                ImGui::Text("Enabled: false");
                ImGui::Text("Input Key: E");
                ImGui::Text("Can Interact: false");
            }

            ImGui::Separator();
            ImGui::TextDisabled("GAMEPLAY EVENTS");
            if (m_Context->gameplayEventBus) {
                const auto& bus = *m_Context->gameplayEventBus;
                ImGui::Text("Frame Events: %u", bus.GetFrameEventCount());
                ImGui::Text("Total Events: %llu", bus.GetTotalEventCount());
                
                const auto& lastEvent = bus.GetLastEvent();
                if (lastEvent.Type != GameplayEventType::None) {
                    ImGui::Text("Last Event: %s", ToString(lastEvent.Type).c_str());
                    ImGui::Text("Last Source: Entity %u", lastEvent.Source);
                    ImGui::Text("Last Target: Entity %u", lastEvent.Target);
                    ImGui::Text("Last Objective: %s", lastEvent.ObjectiveID.c_str());
                    ImGui::Text("Sequence ID: %llu", lastEvent.SequenceID);
                } else {
                    ImGui::Text("Last Event: None");
                    ImGui::Text("Last Source: None");
                    ImGui::Text("Last Target: None");
                    ImGui::Text("Last Objective: None");
                    ImGui::Text("Sequence ID: 0");
                }
            } else {
                ImGui::Text("Event Bus: Not Available");
            }

            if (m_GameMode && m_GameMode->GetObjectiveSystem()) {
                ObjectiveSystem* objSys = m_GameMode->GetObjectiveSystem();
                const auto& objectives = objSys->GetObjectives();

                ImGui::Separator();
                ImGui::TextDisabled("OBJECTIVES");
                
                ImGui::Text("Active Objective: %s", m_GameMode->GetGameState().ActiveObjectiveID.c_str());
                ImGui::Text("Completed Count: %zu / %zu", objSys->GetCompletedCount(), objectives.size());
                
                ImGui::Spacing();
                ImGui::Text("Registered Objectives:");
                for (const auto& pair : objectives) {
                    const auto& obj = pair.second;
                    std::string stateStr = ObjectiveStateToString(obj.State);
                    ImGui::Text("[%s]\t%s", stateStr.c_str(), obj.ID.c_str());
                }

                ImGui::Spacing();
                ImGui::Text("Last Objective Event:");
                ImGui::Text("%s -> %s", objSys->GetLastEventName().c_str(), objSys->GetLastEventObjectiveID().c_str());
            }

            if (m_GameMode) {
                ImGui::Separator();
                ImGui::TextDisabled("DEBUG CONTROLS");
                if (playerEnt != INVALID_ENTITY && m_Context && m_Context->ecs) {
                    auto& coord = m_Context->ecs->getCoordinator();
                    auto pscType = coord.GetComponentType<PlayerStateComponent>();
                    if (coord.GetSignature(playerEnt).test(pscType)) {
                        auto& psc = coord.GetComponent<PlayerStateComponent>(playerEnt);
                        if (ImGui::Button("Damage Player (-25 HP)")) {
                            if (psc.Health > 0.0f) {
                                psc.Health -= 25.0f;
                                if (psc.Health <= 0.0f) {
                                    psc.Health = 0.0f;
                                    psc.IsAlive = false;
                                    m_GameMode->FailLevel();
                                }
                            }
                        }
                        ImGui::SameLine();
                        if (ImGui::Button("Heal Player (+25 HP)")) {
                            psc.Health = std::min(psc.Health + 25.0f, 100.0f);
                            if (psc.Health > 0.0f) {
                                psc.IsAlive = true;
                                if (m_GameMode->GetState() == GameSessionState::Failed) {
                                    m_GameMode->ResumeLevel();
                                }
                            }
                        }
                    }
                }
                
                auto& gs = m_GameMode->GetGameStateMutable();
                if (ImGui::Button("Complete Current Objective")) {
                    if (!gs.ActiveObjectiveID.empty()) {
                        gs.CompletedObjectives.push_back(gs.ActiveObjectiveID);
                        if (gs.ActiveObjectiveID == "OBJ_001") {
                            gs.ActiveObjectiveID = "OBJ_002";
                        } else if (gs.ActiveObjectiveID == "OBJ_002") {
                            gs.ActiveObjectiveID = "OBJ_003";
                        } else {
                            gs.ActiveObjectiveID = "";
                            m_GameMode->CompleteLevel();
                        }
                    }
                }
                ImGui::SameLine();
                if (ImGui::Button("Set Checkpoint CP_001")) {
                    gs.CurrentCheckpointID = "CP_001";
                }
            }
            
            // Gameplay Validation
            ImGui::Separator();
            if (ImGui::CollapsingHeader("Gameplay Validation Diagnostics", ImGuiTreeNodeFlags_DefaultOpen)) {
                DrawGameplayValidatorDiagnostics();
            }
            
            // Physics diagnostics
            ImGui::Separator();
            if (ImGui::CollapsingHeader("PhysX Physics Diagnostics", ImGuiTreeNodeFlags_None)) {
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
                            CORE_LOG_INFO("[Physics Test] Raycast HIT entity ID: %u, Distance: %.2f, Position: (%.2f, %.2f, %.2f), Normal: (%.2f, %.2f, %.2f)",
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
                            CORE_LOG_INFO("[Physics Test] Overlap Sphere HIT %zu entities:", overlapped.size());
                            for (size_t i = 0; i < overlapped.size(); ++i) {
                                CORE_LOG_INFO("  - Entity ID: %u", overlapped[i]);
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
            }

            // Player Controller Diagnostics
            ImGui::Separator();
            if (ImGui::CollapsingHeader("Player Controller Diagnostics", ImGuiTreeNodeFlags_None)) {
                if (m_SimulationState == EditorSimulationState::Play && m_Context && m_Context->ecs) {
                    auto* world = dynamic_cast<World*>(m_Context->ecs);
                    if (world) {
                        if (auto playerControllerSys = world->GetSystem<PlayerControllerSystem>()) {
                            Entity playerEnt = playerControllerSys->GetPlayerEntity();
                            if (playerEnt != 0 && m_Context->ecs->getCoordinator().IsEntityAlive(playerEnt)) {
                                auto& coordinator = m_Context->ecs->getCoordinator();
                                const auto& transform = coordinator.GetComponent<TransformComponent>(playerEnt);
                                const auto& ccc = coordinator.GetComponent<CharacterControllerComponent>(playerEnt);

                                ImGui::Text("Player Entity: %u", playerEnt);
                                ImGui::Text("Position: (%.2f, %.2f, %.2f)", transform.position.x, transform.position.y, transform.position.z);
                                ImGui::Text("Velocity: (%.2f, %.2f, %.2f)", ccc.velocity.x, ccc.velocity.y, ccc.velocity.z);
                                ImGui::Text("Yaw: %.1f, Pitch: %.1f", ccc.yaw, ccc.pitch);
                                ImGui::Text("Grounded: %s", ccc.isGrounded ? "Yes" : "No");
                                ImGui::Text("Wall Blocked: %s", playerControllerSys->IsBlocked() ? "Yes" : "No");
                                ImGui::Text("Cursor Captured: %s", m_CursorCaptured ? "Yes" : "No");
                            } else {
                                ImGui::TextColored(ImVec4(1.0f, 0.4f, 0.4f, 1.0f), "Player Entity not alive or not found.");
                            }
                        } else {
                            ImGui::TextColored(ImVec4(1.0f, 0.4f, 0.4f, 1.0f), "PlayerControllerSystem not found in world.");
                        }
                    }
                } else {
                    ImGui::Text("Not in Play Mode. Cursor Captured: %s", m_CursorCaptured ? "Yes" : "No");
                }
            }

            // Trigger Diagnostics
            ImGui::Separator();
            if (ImGui::CollapsingHeader("Trigger Diagnostics", ImGuiTreeNodeFlags_None)) {
                if (m_SimulationState == EditorSimulationState::Play && m_Context && m_Context->ecs) {
                    auto* world = dynamic_cast<World*>(m_Context->ecs);
                    if (world) {
                        if (auto triggerSys = world->GetSystem<eng::runtime::TriggerSystem>()) {
                            const auto& overlaps = triggerSys->GetCurrentOverlaps();
                            ImGui::Text("Active Overlap Pairs: %zu", overlaps.size());
                            
                            auto& coordinator = m_Context->ecs->getCoordinator();
                            for (const auto& pair : overlaps) {
                                std::string triggerName = "Trigger " + std::to_string(pair.triggerEntity);
                                std::string otherName = "Entity " + std::to_string(pair.otherEntity);
                                
                                if (coordinator.GetSignature(pair.triggerEntity).test(coordinator.GetComponentType<NameComponent>())) {
                                    triggerName = coordinator.GetComponent<NameComponent>(pair.triggerEntity).name;
                                }
                                if (coordinator.GetSignature(pair.otherEntity).test(coordinator.GetComponentType<NameComponent>())) {
                                    otherName = coordinator.GetComponent<NameComponent>(pair.otherEntity).name;
                                }
                                
                                ImGui::Text("- %s overlapping with %s", triggerName.c_str(), otherName.c_str());
                            }

                            // Display list of all trigger entities in scene
                            ImGui::Spacing();
                            ImGui::Text("Trigger Zones in Scene:");
                            for (Entity ent : coordinator.GetActiveEntities()) {
                                if (coordinator.IsEntityAlive(ent)) {
                                    auto sig = coordinator.GetSignature(ent);
                                    if (sig.test(coordinator.GetComponentType<TriggerComponent>())) {
                                        const auto& trigger = coordinator.GetComponent<TriggerComponent>(ent);
                                        std::string trigName = "Trigger " + std::to_string(ent);
                                        if (sig.test(coordinator.GetComponentType<NameComponent>())) {
                                            trigName = coordinator.GetComponent<NameComponent>(ent).name;
                                        }
                                        
                                        bool isActive = triggerSys->IsTriggerActive(ent);
                                        ImGui::Text("  [%s] %s (Event: %s)", 
                                            isActive ? "OVERLAPPED" : "IDLE", 
                                            trigName.c_str(), 
                                            trigger.eventName.c_str());
                                    }
                                }
                            }
                        } else {
                            ImGui::TextColored(ImVec4(1.0f, 0.4f, 0.4f, 1.0f), "TriggerSystem not found in world.");
                        }
                    }
                } else {
                    ImGui::Text("Not in Play Mode.");
                }
            }

            // Audio Diagnostics
            ImGui::Separator();
            if (ImGui::CollapsingHeader("Audio Diagnostics", ImGuiTreeNodeFlags_None)) {
                auto* audioSys = m_Context ? m_Context->audioSystem : nullptr;
                if (audioSys) {
                    ImGui::Text("Backend: miniaudio");
                    ImGui::Text("Initialized: %s", audioSys->IsInitialized() ? "true" : "false");
                    ImGui::Text("Master Volume: %.2f", audioSys->GetMasterVolume());
                    ImGui::Text("Loaded Clips: %zu", audioSys->GetLoadedClipsCount());
                    ImGui::Text("Active Sounds: %zu", audioSys->GetActiveSoundsCount());
                    ImGui::Text("Last Played: %s", audioSys->GetLastPlayedClip().c_str());
                    ImGui::Text("Last Error: %s", audioSys->GetLastError().c_str());
                } else {
                    ImGui::TextColored(ImVec4(1.0f, 0.4f, 0.4f, 1.0f), "AudioSystem not found in context.");
                }
            }

            // State Objects Diagnostics
            ImGui::Separator();
            if (ImGui::CollapsingHeader("State Objects Diagnostics", ImGuiTreeNodeFlags_None)) {
                if (m_GameMode && m_GameMode->GetObjectActivationSystem()) {
                    auto* stateSys = m_GameMode->GetObjectActivationSystem();
                    ImGui::Text("State Objects: %zu", stateSys->GetStateObjectsCount());
                    ImGui::Text("Activatable Objects: %zu", stateSys->GetActivatableObjectsCount());
                    ImGui::Text("Doors: %zu", stateSys->GetDoorsCount());
                    ImGui::Text("Last Activation ID: %s", stateSys->GetLastActivationID().c_str());
                    if (stateSys->GetLastActivationError() != "None") {
                        ImGui::TextColored(ImVec4(1.0f, 0.4f, 0.4f, 1.0f), "Last Error: %s", stateSys->GetLastActivationError().c_str());
                    } else {
                        ImGui::Text("Last Error: None");
                    }
                    ImGui::Separator();
                    stateSys->DrawDiagnosticsGUI();
                } else {
                    ImGui::Text("Not in Play Mode or ObjectActivationSystem not active.");
                }
            }

            // Checkpoints Diagnostics
            ImGui::Separator();
            if (ImGui::CollapsingHeader("Checkpoints Diagnostics", ImGuiTreeNodeFlags_None)) {
                if (m_GameMode && m_GameMode->GetCheckpointSystem()) {
                    auto* cpSys = m_GameMode->GetCheckpointSystem();
                    bool valid = cpSys->HasValidCheckpoint();
                    const auto& snap = cpSys->GetCurrentSnapshot();
                    
                    ImGui::Text("Current Checkpoint: %s", m_GameMode->GetGameState().CurrentCheckpointID.c_str());
                    ImGui::Text("Name: %s", valid ? snap.CheckpointName.c_str() : "None");
                    ImGui::Text("Snapshot Valid: %s", valid ? "true" : "false");
                    
                    if (valid) {
                        ImGui::Separator();
                        ImGui::Text("Captured Snapshot State:");
                        ImGui::Text("  Player Position: (%.2f, %.2f, %.2f)", 
                            snap.PlayerTransform.position.x, 
                            snap.PlayerTransform.position.y, 
                            snap.PlayerTransform.position.z);
                        ImGui::Text("  Active Objective: %s", snap.ActiveObjectiveID.c_str());
                        ImGui::Text("  Completed Objectives: %zu", snap.CompletedObjectives.size());
                        ImGui::Text("  Simple State Objects: %zu", snap.SimpleObjectStates.size());
                        ImGui::Text("  Elapsed Time: %.2fs", snap.ElapsedGameplayTime);
                    }
                    ImGui::Separator();
                    ImGui::Text("Last Event: %s", cpSys->GetLastCheckpointEvent().c_str());
                } else {
                    ImGui::Text("Not in Play Mode or CheckpointSystem not active.");
                }
            }

            // Gameplay Save Diagnostics
            ImGui::Separator();
            if (ImGui::CollapsingHeader("Gameplay Save Diagnostics", ImGuiTreeNodeFlags_None)) {
                auto* saveSys = m_Context ? m_Context->saveSystem : nullptr;
                if (saveSys) {
                    ImGui::Text("GAMEPLAY SAVE");
                    ImGui::Text("Last Save Path: %s", saveSys->GetLastSavePath().c_str());
                    ImGui::Text("Last Save Valid: %s", saveSys->IsLastSaveValid() ? "true" : "false");
                    ImGui::Text("Save Version: %u", saveSys->GetLastSaveVersion());
                    ImGui::Text("Scene: %s", saveSys->GetLastSaveScene().c_str());
                    ImGui::Text("Checksum: 0x%llX", saveSys->GetLastSaveChecksum());
                    ImGui::Text("Payload Size: %llu bytes", saveSys->GetLastSavePayloadSize());
                    
                    if (saveSys->IsLastSaveValid()) {
                        ImGui::Separator();
                        ImGui::Text("Captured:");
                        const auto& trans = saveSys->GetLastSavePlayerTransform();
                        ImGui::Text("  Player Transform: (%.2f, %.2f, %.2f)", trans.position.x, trans.position.y, trans.position.z);
                        ImGui::Text("  Player Health: %.1f", saveSys->GetLastSavePlayerHealth());
                        ImGui::Text("  Player Alive: %s", saveSys->IsLastSavePlayerAlive() ? "true" : "false");
                        ImGui::Text("  Active Objective: %s", saveSys->GetLastSaveActiveObjectiveID().c_str());
                        ImGui::Text("  Completed Objectives: %zu", saveSys->GetLastSaveCompletedObjectivesCount());
                        ImGui::Text("  Checkpoint: %s", saveSys->GetLastSaveCheckpointID().c_str());
                        ImGui::Text("  Interactable States: %zu", saveSys->GetLastSaveInteractableStatesCount());
                        ImGui::Text("  Simple Object States: %zu", saveSys->GetLastSaveSimpleObjectStatesCount());
                    }
                    
                    ImGui::Separator();
                    ImGui::Text("RESTORE");
                    ImGui::Text("Last Restore Result: %s", saveSys->GetLastRestoreResult().c_str());
                    ImGui::Text("Source Save: %s", saveSys->GetLastRestoreSourceSave().c_str());
                    ImGui::Text("Scene Match: %s", saveSys->IsLastRestoreSceneMatch() ? "true" : "false");
                    ImGui::Text("Checksum Valid: %s", saveSys->IsLastRestoreChecksumValid() ? "true" : "false");
                    ImGui::Text("Objects Restored: %zu", saveSys->GetLastRestoreObjectsCount());
                    ImGui::Text("Interactables Restored: %zu", saveSys->GetLastRestoreInteractablesCount());
                    ImGui::Text("Warnings: %zu", saveSys->GetLastRestoreWarnings());
                } else {
                    ImGui::Text("SaveSystem not active.");
                }
            }

            ImGui::End();
        }

        // Render Renderer Light Diagnostics
        if (m_ShowDiagnostics)
        {
            ImGui::Begin("Renderer Light Diagnostics");
            auto* engineLoop = m_Context ? dynamic_cast<eng::runtime::EngineLoop*>(m_Context->renderer) : nullptr;
            if (engineLoop && engineLoop->GetSceneRenderer()) {
                auto* sceneRenderer = engineLoop->GetSceneRenderer();
                auto lightData = sceneRenderer->getLastLightData();
                bool isFallback = sceneRenderer->isFallbackLightingActive();

                ImGui::Text("Lighting Mode: %s", isFallback ? "FALLBACK DEFAULT" : "ECS SCENE LIGHTS");
                ImGui::Text("Shader Mode: Lit (PBR Shading with LightUBO)");
                ImGui::Separator();

                // Ambient Light
                ImGui::Text("Ambient Light Color: (%.2f, %.2f, %.2f)", lightData.ambientColorIntensity.x, lightData.ambientColorIntensity.y, lightData.ambientColorIntensity.z);
                ImGui::Text("Ambient Light Intensity: %.2f", lightData.ambientColorIntensity.w);
                ImGui::Separator();

                // Directional Light
                ImGui::Text("Directional Light Dir: (%.2f, %.2f, %.2f)", lightData.directionalDirectionIntensity.x, lightData.directionalDirectionIntensity.y, lightData.directionalDirectionIntensity.z);
                ImGui::Text("Directional Light Color: (%.2f, %.2f, %.2f)", lightData.directionalColor.x, lightData.directionalColor.y, lightData.directionalColor.z);
                ImGui::Text("Directional Light Intensity: %.2f", lightData.directionalDirectionIntensity.w);
                ImGui::Separator();

                // Point Lights
                ImGui::Text("Point Lights Uploaded: %u / 16", lightData.pointLightCount);
                for (uint32_t i = 0; i < lightData.pointLightCount && i < 16; ++i) {
                    ImGui::PushID(i);
                    std::string label = "Point Light " + std::to_string(i);
                    if (ImGui::TreeNode(label.c_str())) {
                        ImGui::Text("Position: (%.2f, %.2f, %.2f)", lightData.pointPositionsRadius[i].x, lightData.pointPositionsRadius[i].y, lightData.pointPositionsRadius[i].z);
                        ImGui::Text("Radius: %.2f", lightData.pointPositionsRadius[i].w);
                        ImGui::Text("Color: (%.2f, %.2f, %.2f)", lightData.pointColorsIntensity[i].x, lightData.pointColorsIntensity[i].y, lightData.pointColorsIntensity[i].z);
                        ImGui::Text("Intensity: %.2f", lightData.pointColorsIntensity[i].w);
                        ImGui::TreePop();
                    }
                    ImGui::PopID();
                }
            } else {
                ImGui::Text("SceneRenderer not available.");
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
        if (sceneMgr) {
            auto* casted = dynamic_cast<SceneManager*>(sceneMgr);
            if (casted && casted->ShowValidationFailedModal()) {
                ImGui::OpenPopup("Scene Load Blocked");
                casted->ClearValidationFailedModalFlag();
            }
        }

        if (ImGui::BeginPopupModal("Scene Load Blocked", NULL, ImGuiWindowFlags_AlwaysAutoResize)) {
            if (sceneMgr) {
                auto* casted = dynamic_cast<SceneManager*>(sceneMgr);
                if (casted) {
                    const auto& report = casted->GetLastValidationReport();
                    if (report.HasErrors()) {
                        ImGui::TextColored(ImVec4(1.0f, 0.4f, 0.4f, 1.0f), "Scene failed validation and loading was blocked!");
                    } else {
                        ImGui::TextColored(ImVec4(0.4f, 1.0f, 0.4f, 1.0f), "Scene validation passed successfully!");
                    }
                    ImGui::Separator();
                    ImGui::Spacing();

                    ImGui::Text("Validation Report (%zu issues):", report.issues.size());
                    
                    if (report.issues.empty()) {
                        ImGui::TextColored(ImVec4(0.5f, 0.9f, 0.5f, 1.0f), "No issues found. Scene is completely clean.");
                    } else {
                        ImGui::BeginChild("ValidationIssuesList", ImVec2(500, 250), true);
                        for (const auto& issue : report.issues) {
                            ImVec4 color;
                            std::string prefix;
                            switch (issue.severity) {
                                case SceneValidationSeverity::Info:
                                    color = ImVec4(0.7f, 0.7f, 0.7f, 1.0f);
                                    prefix = "[Info]";
                                    break;
                                case SceneValidationSeverity::Warning:
                                    color = ImVec4(1.0f, 0.8f, 0.2f, 1.0f);
                                    prefix = "[Warning]";
                                    break;
                                case SceneValidationSeverity::Error:
                                    color = ImVec4(1.0f, 0.4f, 0.4f, 1.0f);
                                    prefix = "[Error]";
                                    break;
                                case SceneValidationSeverity::Fatal:
                                    color = ImVec4(1.0f, 0.1f, 0.1f, 1.0f);
                                    prefix = "[Fatal]";
                                    break;
                            }
                            ImGui::TextColored(color, "%s %s", prefix.c_str(), issue.code.c_str());
                            if (!issue.entityName.empty()) {
                                ImGui::SameLine();
                                ImGui::TextColored(ImVec4(0.4f, 0.8f, 1.0f, 1.0f), "(Entity: %s)", issue.entityName.c_str());
                            }
                            if (!issue.path.empty()) {
                                ImGui::TextColored(ImVec4(0.6f, 0.6f, 0.6f, 1.0f), "  Path: %s", issue.path.c_str());
                            }
                            ImGui::TextWrapped("  %s", issue.message.c_str());
                            ImGui::Separator();
                        }
                        ImGui::EndChild();
                    }
                }
            }

            ImGui::Spacing();
            ImGui::Separator();
            if (ImGui::Button("Close", ImVec2(120, 0))) {
                ImGui::CloseCurrentPopup();
            }
            ImGui::EndPopup();
        }

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

        Scene* activeScene = sceneMgr->GetActiveScene();
        if (!activeScene) {
            sceneMgr->CreateNewScene("TempPlayScene");
            activeScene = sceneMgr->GetActiveScene();
        }

        // --- Run Gameplay Validation ---
        m_LastValidationResults = m_GameplayValidator.ValidateScene(*activeScene);
        if (m_GameplayValidator.HasFatalErrors(m_LastValidationResults)) {
            CORE_LOG_ERROR("[Editor] Blocked EnterPlayMode: Fatal validation errors found in active scene!");
            for (const auto& res : m_LastValidationResults) {
                if (res.Severity == ValidationSeverity::Fatal) {
                    CORE_LOG_ERROR("  [FATAL] %s (Entity: %d, Component: %s)", 
                        res.Message.c_str(), (int)res.RelatedEntity, res.ComponentName.c_str());
                }
            }
            m_ShowGameplayValidatorWindow = true; // Auto-open validation window on fatal error
            m_SimulationState = EditorSimulationState::Edit;
            m_Context->editorSimulationState = EditorSimulationState::Edit;
            return false;
        }

        // 2. Save pre-play dirty state
        m_EditDirtyBeforePlay = m_DirtyState.IsSceneDirty();

        // 3. Start play session timing and cycle count
        m_PlayStopCycleCount++;
        m_PlaySessionStart = std::chrono::high_resolution_clock::now();

        // 4. Clear selection list
        m_Selection.Clear();

        // 5. Clone active scene and active ECS in-memory
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

        // Spawn GameMode when Play begins
        m_GameMode = std::make_unique<VerticalSliceGameMode>();
        m_Context->gameMode = m_GameMode.get();
        m_GameMode->OnLevelStart(m_Context);

        // Register console interaction event listener
        if (m_Context->gameplayEventBus) {
            m_Context->gameplayEventBus->Subscribe(GameplayEventType::Interaction, [this](const GameplayEvent& event) {
                auto* world = dynamic_cast<World*>(m_Context->ecs);
                if (world) {
                    auto& coordinator = world->getCoordinator();
                    if (coordinator.IsEntityAlive(event.Target)) {
                        auto sig = coordinator.GetSignature(event.Target);
                        if (sig.test(coordinator.GetComponentType<TriggerComponent>())) {
                            const auto& trigger = coordinator.GetComponent<TriggerComponent>(event.Target);
                            if (trigger.eventName == "ConsoleTrigger") {
                                // Find PointLightComponent with name containing "Lamp" / "lamp"
                                for (Entity ent : coordinator.GetActiveEntities()) {
                                    if (ent != 0 && coordinator.IsEntityAlive(ent)) {
                                        auto entSig = coordinator.GetSignature(ent);
                                        if (entSig.test(coordinator.GetComponentType<NameComponent>()) &&
                                            entSig.test(coordinator.GetComponentType<PointLightComponent>())) {
                                            auto& nameComp = coordinator.GetComponent<NameComponent>(ent);
                                            if (nameComp.name.find("Lamp") != std::string::npos || 
                                                nameComp.name.find("lamp") != std::string::npos) {
                                                auto& pointLight = coordinator.GetComponent<PointLightComponent>(ent);
                                                pointLight.color = { 0.0f, 1.0f, 0.0f }; // Change to Green
                                                CORE_LOG_INFO("[Gameplay] EventBus Triggered: Changed color of light '%s' to Green", nameComp.name.c_str());
                                            }
                                        }
                                    }
                                }
                            }
                        }
                    }
                }
            });
        }

        if (m_Context->physicsWorld) {
            m_Context->physicsWorld->ClearScene();
            m_Context->physicsWorld->RegisterStaticColliders(m_Context->ecs->getCoordinator());
            eng::physics::PhysicsDebugDraw::ClearDebugVisuals();
        }

        // 7. Find PlayerStartComponent to get spawn position.
        //    If none exists, auto-create a floor collider + PlayerStart so the
        //    player always has solid ground to stand on.
        auto& simCoordinator = m_Context->ecs->getCoordinator();
        glm::vec3 spawnPos(0.0f, 2.0f, 0.0f);

        const auto& activeEntities = simCoordinator.GetActiveEntities();
        auto playerStartType  = simCoordinator.GetComponentType<PlayerStartComponent>();
        auto transformType    = simCoordinator.GetComponentType<TransformComponent>();
        auto boxColliderType  = simCoordinator.GetComponentType<BoxColliderComponent>();

        bool foundPlayerStart = false;
        bool foundFloorCollider = false;

        for (Entity entity : activeEntities) {
            if (entity == 0 || !simCoordinator.IsEntityAlive(entity)) continue;
            auto sig = simCoordinator.GetSignature(entity);
            if (sig.test(playerStartType) && sig.test(transformType)) {
                const auto& transformComp = simCoordinator.GetComponent<TransformComponent>(entity);
                spawnPos = glm::vec3(transformComp.position.x, transformComp.position.y, transformComp.position.z);
                foundPlayerStart = true;
            }
            if (sig.test(boxColliderType)) {
                foundFloorCollider = true;
            }
        }

        // Auto-inject floor collider if the scene has no static colliders at all
        if (!foundFloorCollider) {
            CORE_LOG_WARN("[Editor] No BoxCollider found in scene — auto-creating floor collider for Play mode.");
            Entity autoFloor = simCoordinator.CreateEntity();
            simCoordinator.AddComponent<NameComponent>(autoFloor, NameComponent("[AutoFloor]"));
            TransformComponent ftc;
            ftc.position = Vector3(0.0f, -0.25f, 0.0f);
            ftc.scale    = Vector3(20.0f, 0.5f, 20.0f);
            ftc.dirty    = true;
            simCoordinator.AddComponent<TransformComponent>(autoFloor, ftc);
            BoxColliderComponent bcc;
            bcc.size   = { 20.0f, 0.5f, 20.0f };
            bcc.offset = { 0.0f, 0.0f, 0.0f };
            simCoordinator.AddComponent<BoxColliderComponent>(autoFloor, bcc);
            // Re-register colliders so the auto-floor is included
            if (m_Context->physicsWorld) {
                m_Context->physicsWorld->ClearScene();
                m_Context->physicsWorld->RegisterStaticColliders(simCoordinator);
            }
        }

        // Auto-inject PlayerStart if none was found
        if (!foundPlayerStart) {
            CORE_LOG_WARN("[Editor] No PlayerStart found — spawning player at default position (0, 2, 0).");
        }

        // 8. Create temporary runtime player entity
        Entity playerEntity = simCoordinator.CreateEntity();
        simCoordinator.AddComponent<NameComponent>(playerEntity, NameComponent("RuntimePlayer"));

        TransformComponent tc;
        tc.position = { spawnPos.x, spawnPos.y, spawnPos.z };
        simCoordinator.AddComponent<TransformComponent>(playerEntity, tc);

        CharacterControllerComponent ccc;
        simCoordinator.AddComponent<CharacterControllerComponent>(playerEntity, ccc);

        CameraComponent cameraComp;
        cameraComp.isPrimary = true;
        cameraComp.localOffset = { 0.0f, 1.6f, 0.0f };
        simCoordinator.AddComponent<CameraComponent>(playerEntity, cameraComp);

        InputComponent inputComp;
        simCoordinator.AddComponent<InputComponent>(playerEntity, inputComp);

        PlayerTagComponent ptc;
        simCoordinator.AddComponent<PlayerTagComponent>(playerEntity, ptc);

        PlayerStateComponent psc;
        psc.ActivePlayer = playerEntity;
        simCoordinator.AddComponent<PlayerStateComponent>(playerEntity, psc);

        auto* world = dynamic_cast<World*>(m_Context->ecs);
        if (world) {
            if (auto playerControllerSys = world->GetSystem<PlayerControllerSystem>()) {
                playerControllerSys->SetPlayerEntity(playerEntity);
            }
        }

        // Lock/capture the cursor when entering play mode
        m_CursorCaptured = true;
        auto* engineLoop = dynamic_cast<eng::runtime::EngineLoop*>(m_Context->renderer);
        if (engineLoop && engineLoop->GetWindow()) {
            engineLoop->GetWindow()->SetCursorMode(eng::platform::CursorMode::Disabled);
        }

        CORE_LOG_INFO("[Editor] Entering Play Mode. In-memory simulation started.");
        return true;
    }

    bool EditorLayer::ExitPlayMode() {
        if (m_SimulationState != EditorSimulationState::Play) return false;

        // Destroy GameMode when Stop pressed
        if (m_GameMode) {
            m_GameMode->OnLevelEnd();
            m_GameMode.reset();
        }
        m_Context->gameMode = nullptr;

        if (m_Context && m_Context->gameplayEventBus) {
            m_Context->gameplayEventBus->ClearQueue();
        }

        auto* sceneMgr = dynamic_cast<SceneManager*>(m_Context->scenes);
        if (!sceneMgr) return false;

        // Release cursor capture
        m_CursorCaptured = false;
        auto* engineLoop = m_Context ? dynamic_cast<eng::runtime::EngineLoop*>(m_Context->renderer) : nullptr;
        if (engineLoop && engineLoop->GetWindow()) {
            engineLoop->GetWindow()->SetCursorMode(eng::platform::CursorMode::Normal);
        }

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
        
        // Spawn entity in front of the editor camera
        glm::vec3 entityPos = m_EditorCamera.position + m_EditorCamera.getForward() * 5.0f;
        TransformComponent tc;
        tc.position = Vector3(entityPos.x, entityPos.y, entityPos.z);
        tc.dirty = true;
        coordinator.AddComponent<TransformComponent>(newEntity, tc);
        
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
        m_EditorCamera.position = entityPos - m_EditorCamera.getForward() * 5.0f;
        m_EditorCamera.LookAt(entityPos);

        CORE_LOG_INFO("[Editor] Successfully created Entity %u from mesh asset: %s", newEntity, entityName.c_str());
    }

    void EditorLayer::DrawGameplayValidatorWindow() {
        if (!m_ShowGameplayValidatorWindow) return;

        ImGui::Begin("Gameplay Validation Results", &m_ShowGameplayValidatorWindow);

        if (m_LastValidationResults.empty()) {
            ImGui::Text("No validation results cached. Click 'Validate Scene' in diagnostics or try entering Play Mode.");
            ImGui::End();
            return;
        }

        // Add a table
        static ImGuiTableFlags flags = ImGuiTableFlags_Resizable | ImGuiTableFlags_Reorderable | ImGuiTableFlags_Hideable | ImGuiTableFlags_RowBg | ImGuiTableFlags_BordersOuter | ImGuiTableFlags_BordersV | ImGuiTableFlags_ScrollY;
        
        if (ImGui::BeginTable("ValidationResultsTable", 4, flags, ImVec2(0.0f, 0.0f))) {
            ImGui::TableSetupColumn("Severity", ImGuiTableColumnFlags_WidthFixed, 80.0f);
            ImGui::TableSetupColumn("Component", ImGuiTableColumnFlags_WidthFixed, 150.0f);
            ImGui::TableSetupColumn("Message", ImGuiTableColumnFlags_WidthStretch);
            ImGui::TableSetupColumn("Entity ID", ImGuiTableColumnFlags_WidthFixed, 80.0f);
            ImGui::TableHeadersRow();

            int id = 0;
            for (const auto& res : m_LastValidationResults) {
                ImGui::TableNextRow();
                ImGui::TableSetColumnIndex(0);

                ImVec4 color;
                const char* severityStr = "";
                switch (res.Severity) {
                    case ValidationSeverity::Info:
                        color = ImVec4(0.2f, 0.7f, 1.0f, 1.0f);
                        severityStr = "Info";
                        break;
                    case ValidationSeverity::Warning:
                        color = ImVec4(1.0f, 0.8f, 0.2f, 1.0f);
                        severityStr = "Warning";
                        break;
                    case ValidationSeverity::Error:
                        color = ImVec4(1.0f, 0.5f, 0.0f, 1.0f);
                        severityStr = "Error";
                        break;
                    case ValidationSeverity::Fatal:
                        color = ImVec4(1.0f, 0.2f, 0.2f, 1.0f);
                        severityStr = "Fatal";
                        break;
                }

                ImGui::PushStyleColor(ImGuiCol_Text, color);
                std::string label = severityStr;
                std::string selectId = "##row_" + std::to_string(id++);
                
                bool isSelected = false;
                if (ImGui::Selectable((label + selectId).c_str(), isSelected, ImGuiSelectableFlags_SpanAllColumns)) {
                    if (res.RelatedEntity != INVALID_ENTITY) {
                        m_Selection.Select(res.RelatedEntity);
                    }
                }
                ImGui::PopStyleColor();

                ImGui::TableSetColumnIndex(1);
                ImGui::Text("%s", res.ComponentName.c_str());

                ImGui::TableSetColumnIndex(2);
                ImGui::TextWrapped("%s", res.Message.c_str());

                ImGui::TableSetColumnIndex(3);
                if (res.RelatedEntity != INVALID_ENTITY) {
                    ImGui::Text("%d", (int)res.RelatedEntity);
                } else {
                    ImGui::Text("N/A");
                }
            }
            ImGui::EndTable();
        }

        ImGui::End();
    }

    void EditorLayer::DrawGameplayValidatorDiagnostics() {
        auto* sceneMgr = dynamic_cast<SceneManager*>(m_Context->scenes);
        if (!sceneMgr) return;
        Scene* activeScene = sceneMgr->GetActiveScene();
        if (!activeScene) {
            ImGui::Text("No active scene to validate.");
            return;
        }

        if (ImGui::Button("Validate Scene")) {
            sceneMgr->SyncECSToScene();
            m_LastValidationResults = m_GameplayValidator.ValidateScene(*activeScene);
        }
        ImGui::SameLine();
        if (ImGui::Button("Clear Results")) {
            m_LastValidationResults.clear();
        }

        size_t infoCount = 0;
        size_t warnCount = 0;
        size_t errCount = 0;
        size_t fatalCount = 0;
        for (const auto& res : m_LastValidationResults) {
            switch (res.Severity) {
                case ValidationSeverity::Info: infoCount++; break;
                case ValidationSeverity::Warning: warnCount++; break;
                case ValidationSeverity::Error: errCount++; break;
                case ValidationSeverity::Fatal: fatalCount++; break;
            }
        }

        ImGui::Text("Results Summary:");
        ImGui::BulletText("Fatal Errors: %zu", fatalCount);
        ImGui::BulletText("Errors: %zu", errCount);
        ImGui::BulletText("Warnings: %zu", warnCount);
        ImGui::BulletText("Info: %zu", infoCount);

        if (fatalCount > 0) {
            ImGui::TextColored(ImVec4(1.0f, 0.2f, 0.2f, 1.0f), "FATAL: Play mode will be blocked!");
        } else if (errCount > 0) {
            ImGui::TextColored(ImVec4(1.0f, 0.5f, 0.0f, 1.0f), "Errors present. Check validation panel.");
        } else if (warnCount > 0) {
            ImGui::TextColored(ImVec4(1.0f, 0.8f, 0.2f, 1.0f), "Warnings present.");
        } else if (!m_LastValidationResults.empty()) {
            ImGui::TextColored(ImVec4(0.2f, 0.8f, 0.2f, 1.0f), "Validation passed with no errors!");
        }

        if (ImGui::Button("Open Validation Window")) {
            m_ShowGameplayValidatorWindow = true;
        }
    }

} // namespace eng::runtime
