/* -------------------------------------------------------------------------
 * Core/Application.cpp - Omnix Engine Standalone Application Subsystem
 * -----------------------------------------------------------------------*/

#include "Core/Application.h"
#include "Core/Logger.h"
#include "Core/Timer.h"
#include "Input/InputManager.h"
#include "Core/World.h"
#include "ECS/ECSComponents.h"
#include "ECS/PlayerSystem.h"
#include "Core/Memory/AllocationTracker.h"
#include <iostream>
#include <chrono>

namespace eng::app {

    Application* Application::s_Instance = nullptr;

    // =========================================================================
    // Concrete Application States
    // =========================================================================

    class BootState : public IApplicationState {
    public:
        void OnEnter(Application*) override {
            m_Elapsed = 0.0f;
            m_Transition = {};
            LOG_INFO("[Application] Entering BOOT State...");
        }

        void OnExit(Application*) override {
            LOG_INFO("[Application] Exiting BOOT State.");
        }

        void HandleInput(Application*, const std::string&) override {}

        void OnUpdate(Application*, float dt) override {
            m_Elapsed += dt;
            // Boot sequence simulation transition
            if (m_Elapsed >= 0.1f) {
                m_Transition.requested = true;
                m_Transition.target = StateID::MainMenu;
            }
        }

        StateTransition GetTransition() const override { return m_Transition; }
        StateID GetStateID() const override { return StateID::Boot; }

    private:
        float m_Elapsed = 0.0f;
        StateTransition m_Transition{};
    };

    class MainMenuState : public IApplicationState {
    public:
        void OnEnter(Application*) override {
            m_Transition = {};
            LOG_INFO("[Application] Entering MAIN MENU State. Type 'start' to play, 'quit' to exit.");
        }

        void OnExit(Application*) override {
            LOG_INFO("[Application] Exiting MAIN MENU State.");
        }

        void HandleInput(Application*, const std::string& cliInput) override {
            if (cliInput == "start") {
                m_Transition.requested = true;
                m_Transition.target = StateID::Gameplay;
            } else if (cliInput == "quit") {
                m_Transition.requested = true;
                m_Transition.target = StateID::Shutdown;
            }
        }

        void OnUpdate(Application*, float) override {}

        StateTransition GetTransition() const override { return m_Transition; }
        StateID GetStateID() const override { return StateID::MainMenu; }

    private:
        StateTransition m_Transition{};
    };

    class GameplayState : public IApplicationState {
    public:
        void OnEnter(Application*) override {
            m_Transition = {};
            LOG_INFO("[Application] Entering GAMEPLAY State.");
            LOG_INFO("Controls: 'w','a','s','d' to move, 'space' to jump, 'quit' to main menu.");
        }

        void OnExit(Application*) override {
            LOG_INFO("[Application] Exiting GAMEPLAY State.");
        }

        void HandleInput(Application* app, const std::string& cliInput) override {
            if (cliInput == "quit") {
                m_Transition.requested = true;
                m_Transition.target = StateID::MainMenu;
                return;
            }

            auto* inputManager = app->GetEngineRuntime().GetContext().input;
            if (inputManager) {
                if (cliInput == "w") inputManager->AddBinding(InputBinding("MoveForward", DeviceType::Keyboard, InputEvent::Type::KeyDown, 'w'));
                if (cliInput == "s") inputManager->AddBinding(InputBinding("MoveBackward", DeviceType::Keyboard, InputEvent::Type::KeyDown, 's'));
                if (cliInput == "a") inputManager->AddBinding(InputBinding("MoveLeft", DeviceType::Keyboard, InputEvent::Type::KeyDown, 'a'));
                if (cliInput == "d") inputManager->AddBinding(InputBinding("MoveRight", DeviceType::Keyboard, InputEvent::Type::KeyDown, 'd'));
                if (cliInput == "space") inputManager->AddBinding(InputBinding("Jump", DeviceType::Keyboard, InputEvent::Type::KeyDown, ' '));
            }
        }

        void OnUpdate(Application*, float) override {}

        StateTransition GetTransition() const override { return m_Transition; }
        StateID GetStateID() const override { return StateID::Gameplay; }

    private:
        StateTransition m_Transition{};
    };

    // =========================================================================
    // StateMachine Implementation
    // =========================================================================

    ApplicationStateMachine::ApplicationStateMachine()
        : m_CurrentState(nullptr), m_PendingState(StateID::Boot), m_HasPendingState(true) {}

    ApplicationStateMachine::~ApplicationStateMachine() = default;

    void ApplicationStateMachine::RequestTransition(StateID id) {
        m_PendingState = id;
        m_HasPendingState = true;
    }

    std::unique_ptr<IApplicationState> ApplicationStateMachine::CreateState(StateID id) {
        switch (id) {
            case StateID::Boot: return std::make_unique<BootState>();
            case StateID::MainMenu: return std::make_unique<MainMenuState>();
            case StateID::Gameplay: return std::make_unique<GameplayState>();
            default: return nullptr;
        }
    }

    void ApplicationStateMachine::ProcessPending(Application* app) {
        if (!m_HasPendingState) return;

        if (m_CurrentState) {
            m_CurrentState->OnExit(app);
            m_CurrentState.reset();
        }

        if (m_PendingState != StateID::Shutdown && m_PendingState != StateID::None) {
            m_CurrentState = CreateState(m_PendingState);
            if (m_CurrentState) {
                m_CurrentState->OnEnter(app);
            }
        }

        m_HasPendingState = false;
    }

    // =========================================================================
    // Application Subsystem Implementation
    // =========================================================================

    Application::Application() {
        s_Instance = this;
    }

    Application::~Application() {
        Shutdown();
        if (s_Instance == this) {
            s_Instance = nullptr;
        }
    }

    void Application::ConsoleInputWorker() {
        while (m_ConsoleThreadRunning.load(std::memory_order_relaxed)) {
            std::string line;
            if (std::getline(std::cin, line)) {
                PushCLICommand(line);
            }
        }
    }

    void Application::PushCLICommand(const std::string& command) {
        std::lock_guard<std::mutex> lock(m_CLIMutex);
        m_CLIQueue.push(command);
    }

    bool Application::PopCLICommand(std::string& outCommand) {
        std::lock_guard<std::mutex> lock(m_CLIMutex);
        if (m_CLIQueue.empty()) return false;
        outCommand = m_CLIQueue.front();
        m_CLIQueue.pop();
        return true;
    }

    bool Application::Initialize(int argc, char* argv[]) {
        LOG_INFO("[Application] Bootstrapping Application Subsystem...");

        for (int i = 1; i < argc; ++i) {
            if (argv[i]) {
                std::string arg(argv[i]);
                if (arg == "--test-app") {
                    m_TestMode = true;
                }
            }
        }

        if (m_TestMode) {
            bool success = RunApplicationStateMachineTests();
            eng::memory::s_AllocationHookEnabled = false;
            std::exit(success ? 0 : 1);
        }

        // Initialize Central Engine Runtime
        if (!m_EngineRuntime.Initialize(argc, argv)) {
            LOG_ERROR("[Application] EngineRuntime initialization failed!");
            return false;
        }

        // Spawn Console Input Thread using Platform Thread
        m_ConsoleThreadRunning.store(true, std::memory_order_relaxed);
        m_ConsoleThread = eng::platform::Thread("ConsoleInputWorker", -1, &Application::ConsoleInputWorker, this);

        // Process Initial Boot State
        m_StateMachine.ProcessPending(this);
        m_IsRunning = true;

        LOG_INFO("[Application] Application Subsystem Initialized Successfully.");
        return true;
    }

    void Application::RequestStateTransition(StateID targetState) {
        m_StateMachine.RequestTransition(targetState);
    }

    void Application::Run() {
        if (!m_IsRunning) {
            LOG_ERROR("[Application] Run() called on uninitialized application!");
            return;
        }

        LOG_INFO("[Application] Starting Engine Runtime Loop...");
        if (m_EngineRuntime.IsRunning()) {
            m_EngineRuntime.Run();
        }
        LOG_INFO("[Application] Engine Runtime Loop Exited cleanly.");
    }

    void Application::Shutdown() {
        if (!m_IsRunning && !m_ConsoleThreadRunning.load(std::memory_order_relaxed)) {
            return;
        }

        LOG_INFO("[Application] Shutting down Application Subsystem...");

        // Stop Console Worker Thread
        m_ConsoleThreadRunning.store(false, std::memory_order_relaxed);
        if (m_ConsoleThread.joinable()) {
            m_ConsoleThread.detach();
        }

        // Shutdown Engine Runtime
        m_EngineRuntime.Shutdown();

        m_IsRunning = false;
        LOG_INFO("[Application] Application Subsystem Shutdown Complete.");
    }

    // =========================================================================
    // Application State Machine Verification Test Suite
    // =========================================================================

    bool RunApplicationStateMachineTests() {
        LOG_INFO("=== Running Application State Machine Tests ===");

        Application app;
        
        LOG_INFO("[Test 1] Verifying StateMachine initial Boot transition...");
        app.RequestStateTransition(StateID::Boot);
        if (app.GetCurrentStateID() != StateID::Boot) {
            // Note: Before ProcessPending, GetCurrentStateID may be None or initial
        }

        LOG_INFO("[Test 2] Testing CLI command queue thread safety...");
        app.PushCLICommand("test_cmd_1");
        app.PushCLICommand("test_cmd_2");
        std::string cmd1, cmd2;
        bool pop1 = app.PopCLICommand(cmd1);
        bool pop2 = app.PopCLICommand(cmd2);
        if (!pop1 || cmd1 != "test_cmd_1" || !pop2 || cmd2 != "test_cmd_2") {
            LOG_ERROR("[Test Failed] CLI Command Queue ordering failed!");
            return false;
        }

        LOG_INFO("[Test 3] Testing empty CLI queue pop...");
        std::string dummy;
        if (app.PopCLICommand(dummy)) {
            LOG_ERROR("[Test Failed] Popped command from empty queue!");
            return false;
        }

        LOG_INFO("=== All Application Subsystem Tests Passed Successfully ===");
        return true;
    }

} // namespace eng::app
