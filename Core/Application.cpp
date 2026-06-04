/* -------------------------------------------------------------------------
 * Core/Application.cpp
 * -----------------------------------------------------------------------*/

#include "Timer.h"
#include "Logger.h"
#include "World.h"
#include "../ECS/ECSComponents.h"
#include "../Input/InputManager.h"
#include "Runtime/engine/EngineLoop.h"

using eng::runtime::World;

#include <iostream>
#include <thread>
#include <chrono>
#include <string>
#include <vector>
#include <queue>
#include <mutex>
#include <atomic>

// Thread-safe input queue for CLI
std::queue<std::string> g_InputQueue;
std::mutex g_InputMutex;
std::atomic<bool> g_InputThreadRunning{true};

void InputThreadFunc() {
    while (g_InputThreadRunning) {
        std::string line;
        if (std::getline(std::cin, line)) {
            std::lock_guard<std::mutex> lock(g_InputMutex);
            g_InputQueue.push(line);
        }
    }
}

enum class StateID {
    STATE_NONE = 0,
    STATE_BOOT,
    STATE_MAINMENU,
    STATE_GAMEPLAY,
    STATE_SHUTDOWN
};

struct Transition {
    bool requested = false;
    StateID target = StateID::STATE_NONE;
};

class IGameState {
public:
    virtual ~IGameState() = default;
    virtual void on_enter(World* world) = 0;
    virtual void on_exit() = 0;
    virtual void handle_input(InputManager* inputManager, const std::string& cliInput) = 0;
    virtual void update(float dt, InputManager* inputManager) = 0;
    virtual Transition get_transition() = 0;
    virtual StateID get_state_id() const = 0;
};

class BootState : public IGameState {
public:
    void on_enter(World*) override { m_elapsed = 0.0f; m_transition = {}; }
    void on_exit() override {}
    void handle_input(InputManager*, const std::string&) override {}
    void update(float dt, InputManager*) override {
        m_elapsed += dt;
        if (m_elapsed > 0.5f) {
            m_transition.requested = true;
            m_transition.target = StateID::STATE_MAINMENU;
        }
    }
    Transition get_transition() override { return m_transition; }
    StateID get_state_id() const override { return StateID::STATE_BOOT; }
private:
    float m_elapsed = 0.0f;
    Transition m_transition{};
};

class MainMenuState : public IGameState {
public:
    void on_enter(World*) override { m_transition = {}; LOG_INFO("--- MAIN MENU --- Type 'start' to play, 'quit' to exit."); }
    void on_exit() override {}
    void handle_input(InputManager*, const std::string& cliInput) override {
        if (cliInput == "start") { m_transition.requested = true; m_transition.target = StateID::STATE_GAMEPLAY; }
        if (cliInput == "quit") { m_transition.requested = true; m_transition.target = StateID::STATE_SHUTDOWN; }
    }
    void update(float, InputManager*) override {}
    Transition get_transition() override { return m_transition; }
    StateID get_state_id() const override { return StateID::STATE_MAINMENU; }
private:
    Transition m_transition{};
};

class GameplayState : public IGameState {
public:
    void on_enter(World* world) override {
        m_world = world;
        m_transition = {};
        LOG_INFO("--- GOLDEN SCENE START ---");
        LOG_INFO("Controls: 'w','a','s','d' to move, 'space' to jump, 'quit' to exit.");

        auto& coordinator = m_world->getCoordinator();

        m_player = coordinator.CreateEntity();
        coordinator.AddComponent(m_player, TransformComponent());
        coordinator.GetComponent<TransformComponent>(m_player).position = {0, 1, 0};
        coordinator.AddComponent(m_player, RigidBodyComponent());
        coordinator.AddComponent(m_player, PlayerControllerComponent());
        coordinator.AddComponent(m_player, MeshRendererComponent());
        coordinator.AddComponent(m_player, TagComponent("Player"));

        Entity floor = coordinator.CreateEntity();
        TransformComponent floorTrans;
        floorTrans.scale = {20, 1, 20};
        coordinator.AddComponent(floor, floorTrans);
        coordinator.AddComponent(floor, MeshRendererComponent());
        coordinator.AddComponent(floor, TagComponent("Floor"));
    }

    void on_exit() override { LOG_INFO("--- GOLDEN SCENE END ---"); }

    void handle_input(InputManager* inputManager, const std::string& cliInput) override {
        if (cliInput == "quit") { m_transition.requested = true; m_transition.target = StateID::STATE_MAINMENU; }
        
        if (cliInput == "w") inputManager->AddBinding(InputBinding("MoveForward", DeviceType::Keyboard, InputEvent::Type::KeyDown, 'w'));
        if (cliInput == "s") inputManager->AddBinding(InputBinding("MoveBackward", DeviceType::Keyboard, InputEvent::Type::KeyDown, 's'));
        if (cliInput == "a") inputManager->AddBinding(InputBinding("MoveLeft", DeviceType::Keyboard, InputEvent::Type::KeyDown, 'a'));
        if (cliInput == "d") inputManager->AddBinding(InputBinding("MoveRight", DeviceType::Keyboard, InputEvent::Type::KeyDown, 'd'));
        if (cliInput == "space") inputManager->AddBinding(InputBinding("Jump", DeviceType::Keyboard, InputEvent::Type::KeyDown, ' '));
    }

    void update(float dt, InputManager* inputManager) override {
        auto& coordinator = m_world->getCoordinator();
        if (auto playerSys = m_world->GetSystem<PlayerSystem>()) playerSys->Update(dt, coordinator, inputManager);
        m_world->GetSystem<PhysicsSystem>()->Update(dt, coordinator);
        m_world->GetSystem<RenderSystem>()->Update(dt, coordinator);

        static float timer = 0;
        timer += dt;
        if (timer > 1.0f) {
            auto& pt = coordinator.GetComponent<TransformComponent>(m_player);
            LOG_INFO(("Player Pos: " + std::to_string(pt.position.x) + ", " + std::to_string(pt.position.y) + ", " + std::to_string(pt.position.z)).c_str());
            timer = 0;
        }
    }

    Transition get_transition() override { return m_transition; }
    StateID get_state_id() const override { return StateID::STATE_GAMEPLAY; }

private:
    World* m_world = nullptr;
    Entity m_player;
    Transition m_transition{};
};

class StateMachine {
public:
    StateMachine() : m_current(nullptr), m_pending(StateID::STATE_BOOT), m_hasPending(true) {}
    void request_transition(StateID id) { m_pending = id; m_hasPending = true; }
    void process_pending(World*& world) {
        if (!m_hasPending) return;
        if (m_current) { m_current->on_exit(); delete m_current; m_current = nullptr; }
        if (world) { delete world; world = nullptr; }
        world = new World();
        m_current = create_state(m_pending);
        m_current->on_enter(world);
        m_hasPending = false;
    }
    IGameState* current() const { return m_current; }
private:
    IGameState* create_state(StateID id) {
        switch (id) {
            case StateID::STATE_BOOT: return new BootState();
            case StateID::STATE_MAINMENU: return new MainMenuState();
            case StateID::STATE_GAMEPLAY: return new GameplayState();
            default: return nullptr;
        }
    }
    IGameState* m_current = nullptr;
    StateID m_pending;
    bool m_hasPending = false;
};

static void DebugGridRender(const eng::renderer::EngineResources& res,
                            const eng::runtime::World& world)
{
    // The render callback is called every frame by EngineLoop::Tick().
    // You have full access to low-level Vulkan objects in 'res' and 
    // the ECS data in 'world'.
    
    // For now, this is a placeholder. In a real scenario, you would
    // record command buffers here or call your custom renderer.
}

int EngineMain(int, char*[]) {
    Logger::Init("Omnix.log", LogLevel::Trace);
    LOG_INFO("=== Omnix Golden Scene Application ===");

    std::thread inputThread(InputThreadFunc);
    inputThread.detach();

    InputManager inputManager;
    inputManager.Initialize();

    World* world = nullptr;
    StateMachine sm;
    sm.process_pending(world);

    // Initialize Rendering Engine
    eng::runtime::EngineLoop engineLoop;
    bool engineStarted = false;

    bool running = true;
    Timer::Init();

    // Register our custom render logic
    engineLoop.RegisterRenderCallback(DebugGridRender);

    while (running) {
        Timer::Update();
        float dt = static_cast<float>(Timer::GetDeltaSeconds());

        sm.process_pending(world);
        if (!sm.current()) break;

        std::string cliInput = "";
        {
            std::lock_guard<std::mutex> lock(g_InputMutex);
            if (!g_InputQueue.empty()) {
                cliInput = g_InputQueue.front();
                g_InputQueue.pop();
            }
        }

        sm.current()->handle_input(&inputManager, cliInput);
        sm.current()->update(dt, &inputManager);

        // Start/Update Rendering Engine when in Gameplay
        if (sm.current()->get_state_id() == StateID::STATE_GAMEPLAY) {
            if (!engineStarted) {
                // Pass the world used by the state machine to the rendering engine
                engineLoop.SetExternalWorld(world);
                if (engineLoop.Initialize().IsSuccess()) {
                    engineStarted = true;
                    LOG_INFO("Rendering Engine started successfully.");
                } else {
                    LOG_ERROR("Failed to start Rendering Engine.");
                }
            }
            
            if (engineStarted) {
                // Manually tick the engine loop's update/render logic
                engineLoop.Tick();
            }
        }

        Transition t = sm.current()->get_transition();
        if (t.requested) {
            if (t.target == StateID::STATE_SHUTDOWN) running = false;
            else sm.request_transition(t.target);
        }

        std::this_thread::sleep_for(std::chrono::milliseconds(16));
    }

    g_InputThreadRunning = false;
    if (world) delete world;
    Logger::Shutdown();
    return 0;
}
