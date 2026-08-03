#include "Gameplay/GameMode.h"
#include "Runtime/RuntimeContext.h"
#include "Gameplay/PlayerStateComponent.h"
#include "Gameplay/Save/GameplaySaveSystem.h"
#include "Core/Logging/Logger.h"
#include "Scene/SceneManager.h"
#include "Scene/Scene.h"
#include "ECS/Coordinator.h"
#include "ECS/ECSComponents.h"
#include "ECS/Public/IECSWorld.h"
#include "Gameplay/GameplayEvent.h"
#include "Gameplay/GameplayEventBus.h"
#include "Gameplay/Objectives/ObjectiveSystem.h"
#include "Gameplay/CheckpointSystem.h"
#include "Gameplay/UI/GameplayHUD.h"
#include "Gameplay/StateObjects/ObjectActivationSystem.h"
#include "Core/World.h"
#include "Gameplay/PlayerControllerSystem.h"
#include "Gameplay/Systems/InteractionSystem.h"
#include "Gameplay/Components/ObjectiveComponent.h"
#include "Gameplay/StateObjects/SimpleStateComponent.h"
#include "Gameplay/StateObjects/ActivatableComponent.h"
#include "Gameplay/StateObjects/DoorComponent.h"

#include "Gameplay/Checkpoints/CheckpointComponent.h"

namespace {
    struct GameplayECSRegister {
        GameplayECSRegister() {
            eng::runtime::World::RegisterGameplayCallback([](Coordinator& coordinator) {
                // Register Components
                coordinator.RegisterComponent<eng::runtime::PlayerStateComponent>();
                coordinator.RegisterComponent<eng::runtime::PlayerTagComponent>();
                coordinator.RegisterComponent<eng::runtime::ObjectiveComponent>();
                coordinator.RegisterComponent<eng::runtime::SimpleStateComponent>();
                coordinator.RegisterComponent<eng::runtime::ActivatableComponent>();
                coordinator.RegisterComponent<eng::runtime::DoorComponent>();
                coordinator.RegisterComponent<eng::runtime::CheckpointComponent>();

                // Register Systems
                // --- PlayerControllerSystem ---
                {
                    auto playerControllerSys = coordinator.RegisterSystem<eng::runtime::PlayerControllerSystem>();
                    ::Signature sig;
                    sig.set(coordinator.GetComponentType<TransformComponent>());
                    sig.set(coordinator.GetComponentType<CharacterControllerComponent>());
                    sig.set(coordinator.GetComponentType<CameraComponent>());
                    coordinator.SetSystemSignature<eng::runtime::PlayerControllerSystem>(sig);
                }

                // --- InteractionSystem ---
                {
                    auto interactionSys = coordinator.RegisterSystem<eng::runtime::InteractionSystem>();
                    ::Signature sig;
                    sig.set(coordinator.GetComponentType<TransformComponent>());
                    sig.set(coordinator.GetComponentType<InteractableComponent>());
                    coordinator.SetSystemSignature<eng::runtime::InteractionSystem>(sig);
                }
            });
        }
    };
    static GameplayECSRegister g_GameplayECSRegister;
}

namespace eng::runtime {


    GameMode::GameMode() = default;
    GameMode::~GameMode() = default;

    void GameMode::OnLevelStart(RuntimeContext* context) {
        m_Context = context;
        if (m_Context) {
            m_Context->gameMode = this;
        }
        m_State = GameSessionState::Playing;

        // Reset game state
        m_GameState.Reset();
        m_GameState.SessionState = GameSessionState::Playing;
        m_GameState.ActiveObjectiveID = "OBJ_001";
        m_GameState.CurrentCheckpointID = "CP_START";

        // Query active scene name
        auto* sceneMgr = dynamic_cast<SceneManager*>(m_Context->scenes);
        if (m_Context && sceneMgr && sceneMgr->GetActiveScene()) {
            m_GameState.ActiveSceneName = sceneMgr->GetActiveScene()->GetName();
        }

        // Find player entity on start
        Entity playerEnt = FindPlayerEntity();
        if (playerEnt != INVALID_ENTITY) {
            auto& coordinator = m_Context->ecs->getCoordinator();
            auto pscType = coordinator.GetComponentType<PlayerStateComponent>();
            if (coordinator.GetSignature(playerEnt).test(pscType)) {
                auto& psc = coordinator.GetComponent<PlayerStateComponent>(playerEnt);
                psc.ActivePlayer = playerEnt;
                LOG_INFO("[GameMode] OnLevelStart: Discovered player entity %u", playerEnt);
            }
        } else {
            LOG_WARN("[GameMode] OnLevelStart: No player entity with PlayerTagComponent found on start.");
        }

        // Subscribe to gameplay events
        if (m_Context && m_Context->gameplayEventBus) {
            m_Context->gameplayEventBus->Subscribe(GameplayEventType::LevelCompleted, [this](const GameplayEvent& event) {
                CompleteLevel();
                if (m_GameplayHUD) {
                    m_GameplayHUD->ShowNotification("Level Complete", 3.0f);
                }
            });

            m_Context->gameplayEventBus->Subscribe(GameplayEventType::PlayerDied, [this](const GameplayEvent& event) {
                FailLevel();
                if (m_GameplayHUD) {
                    m_GameplayHUD->ShowNotification("You Died", 3.0f);
                }
            });

            m_Context->gameplayEventBus->Subscribe(GameplayEventType::CheckpointReached, [this](const GameplayEvent& event) {
                m_GameState.CurrentCheckpointID = event.CheckpointID;
                LOG_INFO("[GameMode] GameState checkpoint ID updated to '%s'", event.CheckpointID.c_str());
                if (m_GameplayHUD) {
                    m_GameplayHUD->ShowNotification("Checkpoint Reached", 2.0f);
                }
            });

            m_Context->gameplayEventBus->Subscribe(GameplayEventType::ObjectiveCompleted, [this](const GameplayEvent& event) {
                if (m_GameplayHUD) {
                    std::string text = "Objective Completed";
                    if (m_ObjectiveSystem) {
                        const auto* obj = m_ObjectiveSystem->GetObjective(event.ObjectiveID);
                        if (obj) {
                            text = "Objective Completed: " + obj->Title;
                        }
                    }
                    m_GameplayHUD->ShowNotification(text, 2.0f);
                }
            });
        }

        // Instantiate and initialize gameplay systems
        m_ObjectiveSystem = std::make_unique<ObjectiveSystem>();
        m_ObjectiveSystem->Initialize(m_Context);
        m_ObjectiveSystem->OnLevelStart();

        m_ObjectActivationSystem = std::make_unique<ObjectActivationSystem>();
        m_ObjectActivationSystem->Initialize(m_Context);
        m_ObjectActivationSystem->OnPlayStart();

        m_CheckpointSystem = std::make_unique<CheckpointSystem>();
        m_CheckpointSystem->Initialize(m_Context);
        m_CheckpointSystem->OnPlayStart();

        m_GameplayHUD = std::make_unique<GameplayHUD>();
        GameplayHUDContext hudContext;
        hudContext.GameState = &m_GameState;
        hudContext.PlayerState = nullptr; // Dynamically resolved by GetPlayerState() inside the HUD
        hudContext.InteractionPrompt = m_Context ? &m_Context->interactionPrompt : nullptr;
        hudContext.Objectives = m_ObjectiveSystem.get();
        hudContext.ECS = m_Context ? m_Context->ecs : nullptr;
        hudContext.EventBus = m_Context ? m_Context->gameplayEventBus : nullptr;
        m_GameplayHUD->SetContext(hudContext);
        m_GameplayHUD->OnPlayStart();

        LOG_INFO("[GameMode] OnLevelStart: Level Started");
    }

    void GameMode::OnLevelEnd() {
        if (m_Context) {
            m_Context->gameMode = nullptr;
        }
        m_State = GameSessionState::None;
        m_GameState.SessionState = GameSessionState::None;

        if (m_GameplayHUD) {
            m_GameplayHUD->OnPlayStop();
        }
        m_GameplayHUD.reset();

        m_ObjectiveSystem.reset();
        if (m_CheckpointSystem) {
            m_CheckpointSystem->OnPlayStop();
        }
        m_CheckpointSystem.reset();

        if (m_ObjectActivationSystem) {
            m_ObjectActivationSystem->OnPlayStop();
        }
        m_ObjectActivationSystem.reset();

        // Clear all persistent event handlers on play stop to prevent dangling pointer crashes
        if (m_Context && m_Context->gameplayEventBus) {
            m_Context->gameplayEventBus->ClearHandlers();
        }

        LOG_INFO("[GameMode] OnLevelEnd: Level Ended");
    }

    void GameMode::Tick(float dt) {
        if (m_GameplayHUD) {
            m_GameplayHUD->Update(dt);
        }

        // Validate player exists every tick
        Entity playerEnt = FindPlayerEntity();
        if (playerEnt == INVALID_ENTITY) {
            LOG_ERROR("[GameMode] Tick: Player validation failed! Player entity does not exist.");
            // Disable gameplay tick by skipping time and state updates
            return;
        }

        // Verify player is alive and state matches
        if (m_Context && m_Context->ecs) {
            auto& coordinator = m_Context->ecs->getCoordinator();
            auto pscType = coordinator.GetComponentType<PlayerStateComponent>();
            if (coordinator.GetSignature(playerEnt).test(pscType)) {
                auto& psc = coordinator.GetComponent<PlayerStateComponent>(playerEnt);
                psc.ActivePlayer = playerEnt;

                // Sync health death state
                if (psc.Health <= 0.0f && psc.IsAlive) {
                    psc.IsAlive = false;
                }

                if (!psc.IsAlive && m_State == GameSessionState::Playing) {
                    if (m_Context->gameplayEventBus) {
                        GameplayEvent dieEvent;
                        dieEvent.Type = GameplayEventType::PlayerDied;
                        dieEvent.Source = playerEnt;
                        m_Context->gameplayEventBus->QueueEvent(dieEvent);
                    }
                }
            }
        }

        // Update elapsed time
        if (m_State == GameSessionState::Playing) {
            m_GameState.ElapsedGameplayTime += dt;
            if (m_ObjectiveSystem) {
                m_ObjectiveSystem->Update(dt);
            }
            if (m_CheckpointSystem) {
                m_CheckpointSystem->Update(dt);
            }
            if (m_ObjectActivationSystem) {
                m_ObjectActivationSystem->Update(dt);
            }
        }
    }

    void GameMode::CompleteLevel() {
        m_State = GameSessionState::Completed;
        m_GameState.SessionState = GameSessionState::Completed;
        LOG_INFO("[GameMode] CompleteLevel: Level Completed");
    }

    void GameMode::FailLevel() {
        m_State = GameSessionState::Failed;
        m_GameState.SessionState = GameSessionState::Failed;
        LOG_INFO("[GameMode] FailLevel: Level Failed");
    }

    void GameMode::RestartLevel() {
        if (m_CheckpointSystem && m_CheckpointSystem->HasValidCheckpoint()) {
            m_CheckpointSystem->RestoreLatestCheckpoint();
            
            // Clear event bus queue on checkpoint restart
            if (m_Context && m_Context->gameplayEventBus) {
                m_Context->gameplayEventBus->ClearQueue();
            }

            if (m_GameplayHUD) {
                m_GameplayHUD->ClearNotifications();
                m_GameplayHUD->ShowNotification("Restored Checkpoint", 2.0f);
            }
            
            m_State = GameSessionState::Playing;
            m_GameState.SessionState = GameSessionState::Playing;
            LOG_INFO("[GameMode] RestartLevel: Restored from checkpoint '%s'", m_GameState.CurrentCheckpointID.c_str());
        } else {
            m_State = GameSessionState::Restarting;
            m_GameState.Reset();
            m_GameState.SessionState = GameSessionState::Restarting;

            if (m_GameplayHUD) {
                m_GameplayHUD->ClearNotifications();
            }
            
            // Reset player state if player exists
            Entity playerEnt = FindPlayerEntity();
            if (playerEnt != INVALID_ENTITY && m_Context && m_Context->ecs) {
                auto& coordinator = m_Context->ecs->getCoordinator();
                auto pscType = coordinator.GetComponentType<PlayerStateComponent>();
                if (coordinator.GetSignature(playerEnt).test(pscType)) {
                    coordinator.GetComponent<PlayerStateComponent>(playerEnt).Reset();
                }
            }

            // Clear event bus queue on restart
            if (m_Context && m_Context->gameplayEventBus) {
                m_Context->gameplayEventBus->ClearQueue();
            }

            if (m_ObjectiveSystem) {
                m_ObjectiveSystem->OnLevelRestart();
            }

            LOG_INFO("[GameMode] RestartLevel: Full Level Reset performed");
        }
    }

    void GameMode::PauseLevel() {
        m_State = GameSessionState::Paused;
        m_GameState.SessionState = GameSessionState::Paused;
        LOG_INFO("[GameMode] PauseLevel: Level Paused");
    }

    void GameMode::ResumeLevel() {
        m_State = GameSessionState::Playing;
        m_GameState.SessionState = GameSessionState::Playing;
        LOG_INFO("[GameMode] ResumeLevel: Level Resumed");
    }

    bool GameMode::RestoreFromSnapshot(const GameplaySaveSnapshot& snapshot) {
        if (!m_Context || !m_Context->saveSystem) {
            LOG_ERROR("[GameMode] Cannot restore from snapshot: saveSystem is null");
            return false;
        }
        return m_Context->saveSystem->RestoreSnapshot(snapshot);
    }

    GameSessionState GameMode::GetState() const {
        return m_State;
    }

    Entity GameMode::FindPlayerEntity() const {
        if (!m_Context || !m_Context->ecs) {
            return INVALID_ENTITY;
        }

        auto& coordinator = m_Context->ecs->getCoordinator();
        auto playerTagType = coordinator.GetComponentType<PlayerTagComponent>();

        for (Entity entity : coordinator.GetActiveEntities()) {
            if (entity != 0 && coordinator.IsEntityAlive(entity)) {
                auto sig = coordinator.GetSignature(entity);
                if (sig.test(playerTagType)) {
                    return entity;
                }
            }
        }

        return INVALID_ENTITY;
    }

} // namespace eng::runtime
