#include "Runtime/Public/EngineRuntime.h"
#include "Runtime/Public/Editor/EditorLayer.h"
#include "Runtime/Public/Gameplay/GameMode.h"
#include "Runtime/Public/Gameplay/GameplayEventBus.h"
#include "Runtime/Public/Audio/AudioSystem.h"
#include "Runtime/Public/Gameplay/Save/GameplaySaveSystem.h"
#include "Runtime/Public/RuntimeContext.h"
#include "Runtime/Public/AssetRegistry.h"
#include "Runtime/Public/World/WorldManager.h"
#include "Runtime/Public/RuntimeState.h"
#include "Runtime/Public/AllocationDiagnostics.h"
#include "Runtime/Public/OwnershipValidation.h"
#include "Runtime/Public/RuntimeStageTracker.h"
#include "Runtime/Public/ProfilingHooks.h"
#include "Core/Diagnostics/Diagnostics.h"
#include "Core/Diagnostics/Assert.h"
#include "Core/Diagnostics/Validation.h"
#include "Core/Memory/AllocatorValidation.h"
#include "Core/Diagnostics/StressTest.h"
#include "Runtime/Public/AssetRegistryTests.h"
#include "Runtime/Public/FormatTests.h"
#include "Runtime/Public/TextureImportTests.h"
#include "Runtime/Public/MeshImportTests.h"
#include "Runtime/Public/AssetCacheTests.h"
#include "Runtime/Public/AssetLoadingStressTests.h"
#include "Runtime/Public/HotReloadTests.h"
#include "Runtime/Public/PackageTests.h"
#include <cstdlib>

#include "Physics/Public/PhysicsWorld.h"
#include "Physics/Public/PhysicsDebugDraw.h"

// Subsystem Concrete Headers
#include "Core/World.h"
#include "Input/InputManager.h"
#include "EventManagement/EventManager.h"
#include "Systems/Scheduler/SystemScheduler.h"
#include "Scene/SceneManager.h"
#include "RenderingEngine/Runtime/engine/EngineLoop.h"
#include "RenderingEngine/Runtime/Resources/AssetCache.h"
#include "Serializer/ECS/SchemaRegistry.h"
#include "Physics/Public/PhysicsWorld.h"
#include "Runtime/Public/Gameplay/Systems/InteractionSystem.h"
#include "ECS/BoundsUpdateSystem.h"

#include "Core/Logger.h"
#include "Core/Timer.h"

#include <iostream>
#include <chrono>

namespace eng::runtime {

    EngineRuntime::EngineRuntime() {
        m_State.store(RuntimeState::Uninitialized, std::memory_order_relaxed);
    }

    EngineRuntime::~EngineRuntime() {
        Shutdown();
    }

    void EngineRuntime::InputThreadWorker() {
        while (m_InputThreadRunning.load(std::memory_order_relaxed)) {
            std::string line;
            if (std::getline(std::cin, line)) {
                // If input is active, process it
                if (m_Input) {
                    // We can post CLI commands here or push to input system queue
                    // For compatibility, we can trigger rebindings or gameplay commands
                    if (line == "quit") {
                        if (m_Renderer) {
                            static_cast<EngineLoop*>(m_Renderer.get())->RequestExit();
                        }
                        m_State.store(RuntimeState::ShuttingDown, std::memory_order_relaxed);
                    }
                }
            }
        }
    }

    bool EngineRuntime::Initialize(int argc, char* argv[]) {
        m_State.store(RuntimeState::Initializing, std::memory_order_relaxed);
        LOG_INFO("[Runtime] Beginning EngineRuntime Initialization...");

        bool failECS = false;
        bool failRenderer = false;
        bool testMemory = false;
        bool testStress = false;
        bool testAssets = false;
        bool testFormats = false;
        bool testTextures = false;
        bool testMeshes = false;
        bool testLoad = false;
        bool testReload = false;
        bool testPackage = false;
        for (int i = 1; i < argc; ++i) {
            if (argv[i]) {
                std::string arg(argv[i]);
                if (arg == "--test-fail-ecs") {
                    failECS = true;
                } else if (arg == "--test-fail-renderer") {
                    failRenderer = true;
                } else if (arg == "--test-memory") {
                    testMemory = true;
                } else if (arg == "--test-stress") {
                    testStress = true;
                } else if (arg == "--test-assets") {
                    testAssets = true;
                } else if (arg == "--test-formats") {
                    testFormats = true;
                } else if (arg == "--test-textures") {
                    testTextures = true;
                } else if (arg == "--test-meshes") {
                    testMeshes = true;
                } else if (arg == "--test-load") {
                    testLoad = true;
                } else if (arg == "--test-reload") {
                    testReload = true;
                } else if (arg == "--test-package") {
                    testPackage = true;
                } else if (arg == "--editor") {
                    m_Context.mode = RuntimeMode::Editor;
                }
            }
        }

        if (testMemory) {
            // Run stress tests
            bool success = eng::memory::RunMemoryValidationTests();
            std::exit(success ? 0 : 1);
        }

        if (testStress) {
            // Run runtime stress tests
            bool success = eng::diagnostics::RunRuntimeStressTests();
            std::exit(success ? 0 : 1);
        }

        if (testAssets) {
            // Run asset registry tests
            bool success = eng::runtime::RunAssetRegistryTests();
            std::exit(success ? 0 : 1);
        }

        if (testFormats) {
            // Run format validation tests
            bool success = eng::runtime::RunFormatTests();
            std::exit(success ? 0 : 1);
        }

        if (testTextures) {
            // Run texture import tests
            bool success = eng::runtime::RunTextureImportTests();
            std::exit(success ? 0 : 1);
        }

        if (testMeshes) {
            // Run mesh import tests
            bool success = eng::runtime::RunMeshImportTests();
            std::exit(success ? 0 : 1);
        }

        if (testLoad) {
            // Run asset loading cache and stress tests
            bool success = eng::runtime::RunAssetCacheTests() && eng::runtime::RunAssetLoadingStressTests();
            std::exit(success ? 0 : 1);
        }

        if (testReload) {
            // Run hot reload tests
            bool success = eng::runtime::RunTextureReloadTests() &&
                           eng::runtime::RunShaderReloadTests() &&
                           eng::runtime::RunMeshReloadTests() &&
                           eng::runtime::RunHotReloadStressTests();
            std::exit(success ? 0 : 1);
        }

        if (testPackage) {
            // Run package pipeline validation tests
            bool success = eng::runtime::RunPackageTests();
            std::exit(success ? 0 : 1);
        }

        if (failECS) {
            OMNIX_FATAL_ASSERT(false, "Forced ECS failure via CLI argument --test-fail-ecs");
        }
        
        auto startTime = std::chrono::high_resolution_clock::now();

        // 1. Core Services (Logger and Timer are static and already initialized)
        Timer::Init();

        // 2. Input System Initialization
        m_Input = std::make_unique<InputManager>();
        TrackAllocation("Input", sizeof(InputManager));
        RegisterSystemStartup("Input");
        m_Input->Initialize();

        // Event System Initialization
        m_EventManager = std::make_unique<Omnix::EventManager>();
        TrackAllocation("Events", sizeof(Omnix::EventManager));
        RegisterSystemStartup("Events");

        // Gameplay Event Bus Initialization
        m_GameplayEventBus = std::make_unique<GameplayEventBus>(m_EventManager.get());
        TrackAllocation("GameplayEvents", sizeof(GameplayEventBus));

        m_AudioSystem = std::make_unique<AudioSystem>();
        TrackAllocation("Audio", sizeof(AudioSystem));

        m_GameplaySaveSystem = std::make_unique<GameplaySaveSystem>();
        TrackAllocation("SaveSystem", sizeof(GameplaySaveSystem));

        // 3. Spawning Input Thread
        m_InputThreadRunning.store(true, std::memory_order_relaxed);
        m_InputThread = std::thread(&EngineRuntime::InputThreadWorker, this);

        // Metadata Schema Registry
        m_SchemaRegistry = std::make_unique<ComponentSchemaRegistry>();
        TrackAllocation("SchemaRegistry", sizeof(ComponentSchemaRegistry));

        // 4. ECS World Initialization
        auto world = std::make_unique<World>();
        TrackAllocation("ECS", sizeof(World));
        RegisterSystemStartup("ECS");
        world->Initialize();
        m_ECS = std::move(world); // Keep owning pointer in m_ECS!

        // 5. Scheduler Initialization
        m_Scheduler = std::make_unique<SystemScheduler>();
        TrackAllocation("Scheduler", sizeof(SystemScheduler));
        RegisterSystemStartup("Scheduler");
        m_Scheduler->Initialize();

        // 6. Renderer (EngineLoop) Initialization
        auto rendererLoop = std::make_unique<EngineLoop>();
        TrackAllocation("Renderer", sizeof(EngineLoop));
        RegisterSystemStartup("Renderer");
        rendererLoop->SetExternalWorld(static_cast<World*>(m_ECS.get()));
        
        if (failRenderer) {
            LOG_ERROR("[Runtime] Renderer Initialization failed (forced via CLI argument --test-fail-renderer)!");
            m_Renderer = std::move(rendererLoop);
            Shutdown();
            return false;
        }

        eng::core::Result renderResult = rendererLoop->Initialize();
        if (renderResult.IsFailure()) {
            LOG_ERROR("[Runtime] Renderer Initialization failed!");
            m_Renderer = std::move(rendererLoop);
            Shutdown();
            return false;
        }
        m_Renderer = std::move(rendererLoop);

        // Initialize PhysicsWorld
        m_PhysicsWorld = std::make_unique<eng::physics::PhysicsWorld>();
        TrackAllocation("PhysicsWorld", sizeof(eng::physics::PhysicsWorld));
        RegisterSystemStartup("PhysicsWorld");
        m_PhysicsWorld->Initialize();

        // 7. Assets Initialization (Retrieve from device or create)
        m_Assets = std::make_unique<AssetCache>(nullptr);
        TrackAllocation("Assets", sizeof(AssetCache));
        RegisterSystemStartup("Assets");

        m_AssetRegistry = std::make_unique<AssetRegistry>();
        m_AssetRegistry->LoadRegistry("AssetRegistry.json");
        if (m_AssetRegistry->GetAssets().empty()) {
            m_AssetRegistry->RegisterAsset("Assets/Models/cube.obj", AssetType::Mesh);
            m_AssetRegistry->RegisterAsset("Assets/Models/pyramid.obj", AssetType::Mesh);
            m_AssetRegistry->RegisterAsset("Assets/Materials/brick.omnixmat", AssetType::Material);
            m_AssetRegistry->RegisterAsset("Assets/Materials/wood.omnixmat", AssetType::Material);
            m_AssetRegistry->RegisterAsset("Assets/Textures/brick_albedo.png", AssetType::Texture);
            m_AssetRegistry->RegisterAsset("Assets/Textures/wood_albedo.png", AssetType::Texture);
            m_AssetRegistry->SaveRegistry("AssetRegistry.json");
        }

        // 8. Scene System (refactored to standard instanced class)
        auto sceneManager = std::make_unique<SceneManager>(&m_ECS->getCoordinator());
        sceneManager->SetAssetRegistry(m_AssetRegistry.get());
        m_Scenes = std::move(sceneManager);
        TrackAllocation("Scene", sizeof(SceneManager));
        RegisterSystemStartup("Scene");

        // WorldManager Subsystem Initialization
        m_WorldManager = std::make_unique<Omnix::WorldManager>(m_Assets.get(), m_AssetRegistry.get(), m_Scenes.get());
        TrackAllocation("WorldManager", sizeof(Omnix::WorldManager));
        RegisterSystemStartup("WorldManager");

        // 9. Populate Context
        m_Context.renderer = m_Renderer.get();
        m_Context.physicsWorld = m_PhysicsWorld.get();
        m_Context.assets = m_Assets.get();
        m_Context.assetRegistry = m_AssetRegistry.get();
        
        auto* loop = dynamic_cast<EngineLoop*>(m_Renderer.get());
        if (loop) {
            loop->SetAssetRegistry(m_AssetRegistry.get());
        }

        m_Context.scenes = m_Scenes.get();
        m_Context.scheduler = m_Scheduler.get();
        m_Context.ecs = m_ECS.get();
        m_Context.input = m_Input.get();
        m_Context.events = m_EventManager.get();
        m_Context.gameplayEventBus = m_GameplayEventBus.get();
        m_Context.audioSystem = m_AudioSystem.get();
        m_Context.saveSystem = m_GameplaySaveSystem.get();
        m_Context.worldManager = m_WorldManager.get();
        m_GameplaySaveSystem->Initialize(&m_Context);
        m_Context.timing = &m_Timing;
        m_Context.currentStage = &m_CurrentStage;

        m_Context.swapECS = [this](std::unique_ptr<eng::runtime::IECSWorld> newECS) {
            return this->SetECS(std::move(newECS));
        };

        // Initialize AudioSystem
        RegisterSystemStartup("Audio");
        if (!m_AudioSystem->Initialize(&m_Context)) {
            LOG_ERROR("[Runtime] AudioSystem initialization failed!");
            Shutdown();
            return false;
        }

        // 10. Editor Layer Subsystem Initialization
        if (m_Context.mode == RuntimeMode::Editor) {
            m_Editor = std::make_unique<EditorLayer>();
            TrackAllocation("Editor", sizeof(EditorLayer));
            RegisterSystemStartup("Editor");
            if (!m_Editor->Initialize(&m_Context)) {
                LOG_ERROR("[Runtime] Editor initialization failed!");
                Shutdown();
                return false;
            }
        }

        auto endTime = std::chrono::high_resolution_clock::now();
        m_StartupTimeMs = std::chrono::duration<double, std::milli>(endTime - startTime).count();
        
        LOG_INFO("[Runtime] EngineRuntime Initialized Successfully in %.2f ms", m_StartupTimeMs);
        m_State.store(RuntimeState::Running, std::memory_order_relaxed);
        return true;
    }

    void LogFrameDiagnostics(double dtSeconds);

    void EngineRuntime::Run() {
        if (GetState() != RuntimeState::Running) {
            LOG_ERROR("[Runtime] Run() called on uninitialized runtime!");
            return;
        }

        LOG_INFO("[Runtime] Engine entering main loop");
        
        using Clock = std::chrono::high_resolution_clock;
        auto lastFrameTimePoint = Clock::now();
        uint64_t frameIdx = 0;

        while (IsRunning()) {
            auto frameStart = Clock::now();
            
            // Calculate Delta Time
            double dt = std::chrono::duration<double>(frameStart - lastFrameTimePoint).count();
            lastFrameTimePoint = frameStart;

            // Track active scene changes for physics registrations
            static Scene* lastActiveScene = nullptr;
            static SceneManager::TransitionState lastTransitionState = SceneManager::TransitionState::Running;
            SceneManager* sceneMgr = dynamic_cast<SceneManager*>(m_Scenes.get());
            Scene* currentActiveScene = sceneMgr ? sceneMgr->GetActiveScene() : nullptr;
            SceneManager::TransitionState currentTransitionState = sceneMgr ? sceneMgr->GetTransitionState() : SceneManager::TransitionState::Running;
            bool sceneChanged = (currentActiveScene != lastActiveScene) || 
                                (currentTransitionState == SceneManager::TransitionState::Running && lastTransitionState != SceneManager::TransitionState::Running);
            if (sceneChanged) {
                lastActiveScene = currentActiveScene;
                lastTransitionState = currentTransitionState;
                if (m_PhysicsWorld && m_ECS) {
                    m_PhysicsWorld->RegisterStaticColliders(m_ECS->getCoordinator());
                    eng::physics::PhysicsDebugDraw::ClearDebugVisuals();
                }
            }
            
            // Limit delta time to avoid spiral of death in lag spikes
            if (dt > 0.1) {
                dt = 0.1;
            }

            // Frame Begin Stage
            m_CurrentStage = FrameStage::FrameBegin;
            RuntimeStageTracker::SetCurrentStage(m_CurrentStage);
            {
                OMNIX_PROFILE_SCOPE("FrameBegin");
                eng::core::g_Profiler.BeginFrame(frameIdx++);
            }

            if (m_Editor) {
                m_Editor->BeginFrame();
            }

            // Input Stage
            m_CurrentStage = FrameStage::Input;
            RuntimeStageTracker::SetCurrentStage(m_CurrentStage);
            {
                OMNIX_PROFILE_SCOPE("Input");
                if (m_Input) {
                    m_Input->Update();
                }
            }

            // Events Stage
            m_CurrentStage = FrameStage::Events;
            RuntimeStageTracker::SetCurrentStage(m_CurrentStage);
            {
                OMNIX_PROFILE_SCOPE("Events");
                if (m_EventManager) {
                    m_EventManager->processQueue();
                }
            }

            // PreUpdate Stage
            m_CurrentStage = FrameStage::PreUpdate;
            RuntimeStageTracker::SetCurrentStage(m_CurrentStage);
            {
                OMNIX_PROFILE_SCOPE("PreUpdate");
            }

            // Update Stage
            m_CurrentStage = FrameStage::Update;
            RuntimeStageTracker::SetCurrentStage(m_CurrentStage);
            auto updateStart = Clock::now();
            {
                OMNIX_PROFILE_SCOPE("Update");
                if (m_Scheduler) {
                    static_cast<SystemScheduler*>(m_Scheduler.get())->RunPending();
                }
                if (m_Scenes) {
                    m_Scenes->Update(static_cast<float>(dt));
                }
                if (m_WorldManager) {
                    m_WorldManager->Update(m_Context, static_cast<float>(dt));
                }
                if (m_ECS) {
                    m_ECS->Update(static_cast<float>(dt));

                    bool shouldSimulate = (m_Context.mode == RuntimeMode::Game) ||
                                          (m_Context.mode == RuntimeMode::Editor && 
                                           (m_Context.editorSimulationState == EditorSimulationState::Play ||
                                            m_Context.editorSimulationState == EditorSimulationState::Step));
                    if (shouldSimulate) {
                        auto& coordinator = m_ECS->getCoordinator();
                        auto* world = dynamic_cast<World*>(m_ECS.get());
                        if (world) {
                            if (auto playerSys = world->GetSystem<PlayerSystem>()) {
                                playerSys->Update(static_cast<float>(dt), coordinator, m_Input.get());
                            }
                            if (auto physicsSys = world->GetSystem<PhysicsSystem>()) {
                                physicsSys->Update(static_cast<float>(dt), coordinator);
                            }
                        }
                    }
                }
                if (m_AudioSystem) {
                    m_AudioSystem->Update(static_cast<float>(dt));
                }
            }
            auto updateEnd = Clock::now();
            m_Timing.updateTime = std::chrono::duration<double>(updateEnd - updateStart).count();

            // PostUpdate Stage
            m_CurrentStage = FrameStage::PostUpdate;
            RuntimeStageTracker::SetCurrentStage(m_CurrentStage);
            {
                OMNIX_PROFILE_SCOPE("PostUpdate");
            }

            // Physics Stage
            m_CurrentStage = FrameStage::Physics;
            RuntimeStageTracker::SetCurrentStage(m_CurrentStage);
            {
                OMNIX_PROFILE_SCOPE("Physics");
                bool shouldSimulate = (m_Context.mode == RuntimeMode::Game) ||
                                      (m_Context.mode == RuntimeMode::Editor && 
                                       (m_Context.editorSimulationState == EditorSimulationState::Play ||
                                        m_Context.editorSimulationState == EditorSimulationState::Step));
                if (shouldSimulate && m_PhysicsWorld) {
                    m_PhysicsWorld->FixedUpdate(static_cast<float>(dt));
                    auto& coordinator = m_ECS->getCoordinator();
                    auto* world = dynamic_cast<World*>(m_ECS.get());
                    if (world) {
                        auto playerControllerSys = world->GetSystem<PlayerControllerSystem>();
                        auto triggerSys = world->GetSystem<TriggerSystem>();
                        int steps = m_PhysicsWorld->GetStepsThisFrame();
                        float fixedDt = m_PhysicsWorld->GetFixedTimestep();
                        for (int i = 0; i < steps; ++i) {
                            if (playerControllerSys) {
                                playerControllerSys->FixedUpdate(m_PhysicsWorld.get(), coordinator, fixedDt);
                            }
                            if (triggerSys) {
                                triggerSys->FixedUpdate(m_Context, fixedDt);
                            }
                        }
                    }
                } else {
                    auto* world = dynamic_cast<World*>(m_ECS.get());
                    if (world) {
                        if (auto triggerSys = world->GetSystem<TriggerSystem>()) {
                            triggerSys->ClearOverlaps();
                        }
                    }
                }
            }
            // Interaction Stage (Play Mode only)
            {
                bool shouldSimulate = (m_Context.mode == RuntimeMode::Game) ||
                                      (m_Context.mode == RuntimeMode::Editor && 
                                       (m_Context.editorSimulationState == EditorSimulationState::Play ||
                                        m_Context.editorSimulationState == EditorSimulationState::Step));
                if (shouldSimulate && m_ECS) {
                    auto* world = dynamic_cast<World*>(m_ECS.get());
                    if (world) {
                        if (auto interactionSys = world->GetSystem<InteractionSystem>()) {
                            interactionSys->Update(static_cast<float>(dt), m_Context);
                        }
                    }
                }
            }

            // Flush gameplay events
            if (m_Context.gameplayEventBus) {
                m_Context.gameplayEventBus->FlushEvents();
            }

            // Bounds Update Stage — runs every frame (editor + game)
            // so world-space AABBs are always fresh for debug draw / culling.
            if (m_ECS) {
                auto* world = dynamic_cast<World*>(m_ECS.get());
                if (world) {
                    if (auto boundsSys = world->GetSystem<BoundsUpdateSystem>()) {
                        boundsSys->Update(static_cast<float>(dt), m_ECS->getCoordinator());
                    }
                }
            }

            // GameMode Tick
            if (m_Context.gameMode) {
                m_Context.gameMode->Tick(static_cast<float>(dt));
            }

            if (m_Context.editorSimulationState == EditorSimulationState::Step) {
                m_Context.editorSimulationState = EditorSimulationState::Pause;
            }

            // Animation Stage
            m_CurrentStage = FrameStage::Animation;
            RuntimeStageTracker::SetCurrentStage(m_CurrentStage);
            {
                OMNIX_PROFILE_SCOPE("Animation");
            }

            // RenderPreparation Stage
            m_CurrentStage = FrameStage::RenderPreparation;
            RuntimeStageTracker::SetCurrentStage(m_CurrentStage);
            {
                OMNIX_PROFILE_SCOPE("RenderPreparation");
            }

            // Render Stage
            m_CurrentStage = FrameStage::Render;
            RuntimeStageTracker::SetCurrentStage(m_CurrentStage);
            
            if (m_Editor) {
                m_Editor->Render();
            }

            auto renderStart = Clock::now();
            {
                OMNIX_PROFILE_SCOPE("Render");
                if (m_Renderer) {
                    m_Renderer->BeginFrame(dt);
                    m_Renderer->Render();
                    m_Renderer->EndFrame();
                }
            }
            auto renderEnd = Clock::now();
            m_Timing.renderTime = std::chrono::duration<double>(renderEnd - renderStart).count();

            if (m_Editor) {
                m_Editor->EndFrame();
            }

            // Frame End Stage
            m_CurrentStage = FrameStage::FrameEnd;
            RuntimeStageTracker::SetCurrentStage(m_CurrentStage);
            {
                OMNIX_PROFILE_SCOPE("FrameEnd");
                eng::core::g_Profiler.EndFrame();
            }

            // Check if renderer requested exit
            if (m_Renderer && !static_cast<EngineLoop*>(m_Renderer.get())->IsRunning()) {
                m_State.store(RuntimeState::ShuttingDown, std::memory_order_relaxed);
            }

            auto frameEnd = Clock::now();
            m_Timing.frameTime = std::chrono::duration<double>(frameEnd - frameStart).count();
            m_Timing.deltaTime = dt;

            // Frame diagnostics / logging
            LogFrameDiagnostics(m_Timing.deltaTime);

            // Print timing logs occasionally to see the breakdown
            static uint64_t s_LoggedFrames = 0;
            if (++s_LoggedFrames % 300 == 0) {
                LOG_DEBUG("[Loop] Frame breakdown - Frame: %.2fms, Update: %.2fms, Render: %.2fms",
                          m_Timing.frameTime * 1000.0, m_Timing.updateTime * 1000.0, m_Timing.renderTime * 1000.0);

                if (m_ECS) {
                    eng::diagnostics::ReportSubsystemHealth("ECS", std::to_string(m_ECS->getCoordinator().GetLivingEntityCount()) + " living entities");
                }
                if (m_Renderer) {
                    bool isRunning = static_cast<EngineLoop*>(m_Renderer.get())->IsRunning();
                    eng::diagnostics::ReportSubsystemHealth("Renderer", isRunning ? "Running" : "Stopped");
                }
                if (m_Assets) {
                    eng::diagnostics::ReportSubsystemHealth("Assets", "Active");
                }
                eng::diagnostics::PrintDiagnosticsReport();
            }

            std::this_thread::sleep_for(std::chrono::milliseconds(16));
        }
    }

    void EngineRuntime::Shutdown() {
        if (GetState() == RuntimeState::Uninitialized) {
            return;
        }

        m_State.store(RuntimeState::ShuttingDown, std::memory_order_relaxed);
        LOG_INFO("[Runtime] Beginning EngineRuntime Shutdown...");

        // 1. Stop CLI Input Thread
        m_InputThreadRunning.store(false, std::memory_order_relaxed);
        if (m_InputThread.joinable()) {
            m_InputThread.detach();
        }

        // Editor Shutdown
        if (m_Editor) {
            RegisterSystemShutdown("Editor");
            m_Editor->Shutdown();
            m_Editor.reset();
            TrackDeallocation("Editor", sizeof(EditorLayer));
        }

        // Audio Shutdown
        if (m_AudioSystem) {
            RegisterSystemShutdown("Audio");
            m_AudioSystem->Shutdown();
            m_AudioSystem.reset();
            TrackDeallocation("Audio", sizeof(AudioSystem));
        }

        // Save System Shutdown
        if (m_GameplaySaveSystem) {
            m_GameplaySaveSystem.reset();
            TrackDeallocation("SaveSystem", sizeof(GameplaySaveSystem));
        }

        // PhysicsWorld Shutdown
        if (m_PhysicsWorld) {
            RegisterSystemShutdown("PhysicsWorld");
            m_PhysicsWorld->Shutdown();
            m_PhysicsWorld.reset();
            TrackDeallocation("PhysicsWorld", sizeof(eng::physics::PhysicsWorld));
        }

        // WorldManager Shutdown
        if (m_WorldManager) {
            RegisterSystemShutdown("WorldManager");
            m_WorldManager.reset();
            TrackDeallocation("WorldManager", sizeof(Omnix::WorldManager));
        }

        // 2. Scene Shutdown
        if (m_Scenes) {
            RegisterSystemShutdown("Scene");
            m_Scenes.reset();
            TrackDeallocation("Scene", sizeof(SceneManager));
        }

        // 3. Assets Shutdown
        if (m_Assets) {
            RegisterSystemShutdown("Assets");
            m_Assets.reset();
            TrackDeallocation("Assets", sizeof(AssetCache));
        }

        // 4. Renderer Shutdown
        if (m_Renderer) {
            RegisterSystemShutdown("Renderer");
            m_Renderer->Shutdown();
            m_Renderer.reset();
            TrackDeallocation("Renderer", sizeof(EngineLoop));
        }

        // 5. Scheduler Shutdown
        if (m_Scheduler) {
            RegisterSystemShutdown("Scheduler");
            m_Scheduler->Shutdown();
            m_Scheduler.reset();
            TrackDeallocation("Scheduler", sizeof(SystemScheduler));
        }

        // 6. ECS Shutdown (Now safely owned by EngineRuntime!)
        if (m_ECS) {
            RegisterSystemShutdown("ECS");
            m_ECS->Shutdown();
            m_ECS.reset();
            TrackDeallocation("ECS", sizeof(World));
        }

        // Schema Registry Shutdown
        if (m_SchemaRegistry) {
            m_SchemaRegistry.reset();
            TrackDeallocation("SchemaRegistry", sizeof(ComponentSchemaRegistry));
        }

        // 7. Input Shutdown
        if (m_Input) {
            RegisterSystemShutdown("Input");
            m_Input.reset();
            TrackDeallocation("Input", sizeof(InputManager));
        }

        // Event System Shutdown
        if (m_EventManager) {
            RegisterSystemShutdown("Events");
            m_EventManager.reset();
            TrackDeallocation("Events", sizeof(Omnix::EventManager));
        }

        LOG_INFO("[Runtime] EngineRuntime Subsystem Shutdown Complete.");

        // Run validation and leak checks
        ValidateExecutionSequence();
        ReportMemoryLeaks();

        m_State.store(RuntimeState::Uninitialized, std::memory_order_relaxed);
    }

    std::unique_ptr<eng::runtime::IECSWorld> EngineRuntime::SetECS(std::unique_ptr<eng::runtime::IECSWorld> ecs) {
        if (!ecs) return nullptr;

        auto oldECS = std::move(m_ECS);
        m_ECS = std::move(ecs);
        m_Context.ecs = m_ECS.get();

        if (m_Renderer) {
            auto* rendererLoop = dynamic_cast<EngineLoop*>(m_Renderer.get());
            if (rendererLoop) {
                rendererLoop->SetExternalWorld(static_cast<World*>(m_ECS.get()));
            }
        }

        if (m_Scenes) {
            auto* sceneMgr = dynamic_cast<SceneManager*>(m_Scenes.get());
            if (sceneMgr) {
                sceneMgr->SetCoordinator(&m_ECS->getCoordinator());
            }
        }
        return oldECS;
    }

} // namespace eng::runtime
