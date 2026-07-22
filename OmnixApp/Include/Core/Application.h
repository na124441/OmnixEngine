#pragma once

#include "Core/Engine/EngineRuntime.h"
#include "Core/Platform/Thread.h"
#include <memory>
#include <string>
#include <queue>
#include <mutex>
#include <atomic>

namespace eng::app {

    enum class StateID : uint8_t {
        None = 0,
        Boot,
        MainMenu,
        Gameplay,
        Shutdown
    };

    struct StateTransition {
        bool requested = false;
        StateID target = StateID::None;
    };

    class Application;

    class IApplicationState {
    public:
        virtual ~IApplicationState() = default;
        virtual void OnEnter(Application* app) = 0;
        virtual void OnExit(Application* app) = 0;
        virtual void HandleInput(Application* app, const std::string& cliInput) = 0;
        virtual void OnUpdate(Application* app, float dt) = 0;
        virtual StateTransition GetTransition() const = 0;
        virtual StateID GetStateID() const = 0;
    };

    class ApplicationStateMachine {
    public:
        ApplicationStateMachine();
        ~ApplicationStateMachine();

        void RequestTransition(StateID id);
        void ProcessPending(Application* app);
        IApplicationState* GetCurrentState() const { return m_CurrentState.get(); }
        StateID GetCurrentStateID() const { return m_CurrentState ? m_CurrentState->GetStateID() : StateID::None; }

    private:
        std::unique_ptr<IApplicationState> CreateState(StateID id);

        std::unique_ptr<IApplicationState> m_CurrentState;
        StateID m_PendingState = StateID::Boot;
        bool m_HasPendingState = true;
    };

    class Application {
    public:
        Application();
        ~Application();

        Application(const Application&) = delete;
        Application& operator=(const Application&) = delete;
        Application(Application&&) = delete;
        Application& operator=(Application&&) = delete;

        static Application* GetInstance() { return s_Instance; }

        bool Initialize(int argc = 0, char* argv[] = nullptr);
        void Run();
        void Shutdown();

        void RequestStateTransition(StateID targetState);
        StateID GetCurrentStateID() const { return m_StateMachine.GetCurrentStateID(); }
        
        eng::core::EngineRuntime& GetEngineRuntime() { return m_EngineRuntime; }
        const eng::core::EngineRuntime& GetEngineRuntime() const { return m_EngineRuntime; }

        void PushCLICommand(const std::string& command);
        bool PopCLICommand(std::string& outCommand);

    private:
        void ConsoleInputWorker();

        static Application* s_Instance;

        eng::core::EngineRuntime m_EngineRuntime;
        ApplicationStateMachine m_StateMachine;

        // Thread-safe CLI Input Queue & Console Thread
        eng::platform::Thread m_ConsoleThread;
        std::atomic<bool> m_ConsoleThreadRunning{ false };
        std::queue<std::string> m_CLIQueue;
        std::mutex m_CLIMutex;

        bool m_IsRunning = false;
        bool m_TestMode = false;
    };

    // Application state machine test runner helper
    bool RunApplicationStateMachineTests();

} // namespace eng::app
